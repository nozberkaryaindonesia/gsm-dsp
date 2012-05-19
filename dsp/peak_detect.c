/*
 * TI c64x+ GSM signal processing
 *
 * Copyright (C) 2012  Thomas Tsou <ttsou@vt.edu>
 *
 * All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdlib.h>
#include "dsp.h"

extern char *dbg;

#define INTERP_FILT_M			32
#define INTERP_FILT_WIDTH		8
#define INTERP_FILT_RATE		(INTERP_FILT_M * GSMRATE)
#define INTERP_FILT_MIN			4

/*
 * Interpolating filterbank
 * Real input with multiple of multiple of 8 taps
 */
complex interp_filt_data[INTERP_FILT_M][INTERP_FILT_WIDTH];
struct cxvec interp_filt[INTERP_FILT_M];

static complex early[INTERP_FILT_M];
static complex late[INTERP_FILT_M];
static int pow[DEF_MAXLEN];

/*
 * Create real floating point prototype filter and convert to Q15
 * 
 * Load partition filters in order so that increasing path number
 * interpolates results in a forward manner. Swap tap order due
 * to requires of DSPLIB real convolution.
 */
static int init_interp_filt()
{
	int i, n;
	int m = INTERP_FILT_M;
	int prot_filt_len = INTERP_FILT_M * INTERP_FILT_WIDTH;
	float *prot_filt_f;
	short *prot_filt_i;

	float midpt = prot_filt_len / 2 - 1; 

	prot_filt_f = (float *) malloc(prot_filt_len * sizeof(float));
	prot_filt_i = (short *) malloc(prot_filt_len * sizeof(short));

	for (i = 0; i < m; i++) {
		cxvec_init(&interp_filt[i], INTERP_FILT_WIDTH,
			   INTERP_FILT_WIDTH, 0, &interp_filt_data[i][0]);
	}

	for (i = 0; i < prot_filt_len; i++) {
		prot_filt_f[i] = sinc(((float) i - midpt) / m);
	}

	DSP_fltoq15(prot_filt_f, prot_filt_i, prot_filt_len);

	for (i = 0; i < INTERP_FILT_WIDTH; i++) {
		for (n = 0; n < m; n++) {
			interp_filt[n].data[i].real = prot_filt_i[i * m + n];
			interp_filt[n].data[i].imag = 0;
		}
	}

	free(prot_filt_f);
	free(prot_filt_i);

	return 0;
}

static int maxidx(int *in, int len)
{
	int i;
	int idx = -1;
	int max = -1;

	for (i = 0; i < len; i++) {
		if (in[i] > max) {
			max = in[i];
			idx = i;
		}
	}

	return idx;
}

static int cx_maxidx(complex *in, int len)
{
	int i;

#ifdef INTRN_DOTP2
	for (i = 0; i < len; i++) {
		pow[i] = _dotp2((int) in[i], (int) in[i]);
	}
#else
#ifdef INTRN_SADD
	int sum_r, sum_i;

	for (i = 0; i < len; i++) {
		sum_r = ((int) in[i].real) * ((int) in[i].real);
		sum_i = ((int) in[i].imag) * ((int) in[i].imag);
		pow[i] = _sadd(sum_r, sum_i);
	}
#else
	int sum_r, sum_i;

	for (i = 0; i < len; i++) {
		sum_r = ((int) in[i].real) * ((int) in[i].real);
		sum_i = ((int) in[i].imag) * ((int) in[i].imag);
		pow[i] = sum_r + sum_i;
	}
#endif
#endif
	return maxidx(pow, len);
}

/*
 * Determine the case where the correlation point is at the very
 * end and a minimally padded vector will still roll off the end
 * before it gets to the center tap.
 */
static int interp_pts(struct cxvec *in, int idx,
		      int frac, complex *early, complex *late)
{
	int rc;

	complex conv_out[2];

	rc = cx_conv2(&in->data[idx], &interp_filt[frac], conv_out, CONV_NO_DELAY);

	*early = conv_out[0];
	*late = conv_out[1];

	return rc;
}

int cxvec_peak_detect(struct cxvec *in, struct vec_peak *peak)
{
	int i;
	int idx, early_idx, late_idx;
	int max, early_max, late_max;

	/* Find the power */
	idx = cx_maxidx(in->data, in->len);
	max = norm2(in->data[idx]);

	peak->orig = idx;

	if ((idx < 20) || (idx > 85)) 
		return -1;

	/* Drop back one sample so we do early / late  in the same pass */
	for (i = 0; i < INTERP_FILT_M; i++) {
		interp_pts(in, idx, i, &early[i], &late[i]);
	}

	early_idx = cx_maxidx(early, INTERP_FILT_M);
	early_max = norm2(early[early_idx]);

	late_idx = cx_maxidx(late, INTERP_FILT_M);
	late_max = norm2(late[late_idx]);

	/*
	 * Late sample bank includes the centre sample
	 * Keep them discrete for now to avoid confusion
	 */
	if ((early_max > late_max) && (early_max > max)) {
		peak->gain = early[early_idx];
		peak->whole = idx - 1;
		peak->frac = early_idx; 
	} else if ((late_max > early_max) && (late_max > max)) {
		peak->gain = late[late_idx];
		peak->whole = idx; 
		peak->frac = late_idx; 
	} else {
		peak->gain = in->data[idx];
		peak->whole = idx;
		peak->frac = 0; 
	}

	return peak->frac; 
}

void init_peak_detect()
{
	init_interp_filt();
	memset(pow, 0, DEF_MAXLEN * sizeof(int));
}
