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
#define INTERP_FILT_WIDTH		16 
#define INTERP_FILT_RATE		(INTERP_FILT_M * GSMRATE)
#define INTERP_FILT_MIN			4

/*
 * Interpolating filterbank
 * Real input with multiple of multiple of 8 taps
 */
short interp_filt_data[INTERP_FILT_M][INTERP_FILT_WIDTH];
struct rvec interp_filt[INTERP_FILT_M];

static short early[INTERP_FILT_M];
static short late[INTERP_FILT_M];
static short power_buf[DEF_MAXLEN];
struct rvec pow_vec;

/*
 * Create real floating point prototype filter and convert to Q15
 * 
 * Load partition filters in order so that increasing path number
 * interpolates results in a forward manner. Swap tap order due
 * to requires of DSPLIB real convolution.
 * 
 * Taps must be a factor of 4 or 8 depending on convolution used.
 *
 * Input must be a factor or 4.
 */
static int init_interp_filt()
{
	int i, n;
	int m = INTERP_FILT_M;
	int prot_filt_len = INTERP_FILT_M * INTERP_FILT_WIDTH;
	float *prot_filt_f;
	short *prot_filt_i;

	float midpt = prot_filt_len / 2 - m; 

	prot_filt_f = (float *) malloc(prot_filt_len * sizeof(float));
	prot_filt_i = (short *) malloc(prot_filt_len * sizeof(short));

	for (i = 0; i < m; i++) {
		rvec_init(&interp_filt[i], INTERP_FILT_WIDTH,
			  INTERP_FILT_WIDTH, 0, &interp_filt_data[i][0]);
	}

	for (i = 0; i < prot_filt_len; i++) {
		prot_filt_f[i] = sinc(((float) i - midpt) / m);
	}

	//flt_scale_h(prot_filt_f, prot_filt_len, SCALE_DC_GAIN);

	DSP_fltoq15(prot_filt_f, prot_filt_i, prot_filt_len);

	for (i = 0; i < INTERP_FILT_WIDTH; i++) {
		for (n = 0; n < m; n++) {
			interp_filt[n].data[i] = prot_filt_i[i * m + n];
		}
	}

	for (i = 0; i < m; i++) {
		rvec_rvrs(&interp_filt[i]);
	}

	free(prot_filt_f);
	free(prot_filt_i);

	return 0;
}

/*
 * Determine the case where the correlation point is at the very
 * end and a minimally padded vector will still roll off the end
 * before it gets to the center tap.
 */
static int interp_pts(struct rvec *in, short idx,
		      int frac, short *early, short *late)
{
	short conv_out[2];

	real_convolve2(&in->data[idx], &interp_filt[frac],
		       conv_out, CONV_NO_DELAY);

	*early = conv_out[0];
	*late = conv_out[1];

	return 0;
}

int cxvec_peak_detect(struct cxvec *restrict in, struct vec_peak *restrict peak)
{
	int i;
	int m = INTERP_FILT_M;
	short early_pow, late_pow;

	/* Find the power */
	cxvec_pow(in, &pow_vec);
	peak->gain = maxval(pow_vec.data, in->len);
	peak->orig = maxidx(pow_vec.data, in->len);

	/* Drop back one sample so we do early / late  in the same pass */
	for (i = 0; i < m; i++) {
		interp_pts(&pow_vec, peak->orig - 1, i, &early[i], &late[i]);
	}                                                                                                    
	//DSP_q15tofl(early, dbg, INTERP_FILT_M);
	//return 0;

	early_pow = maxval(early, INTERP_FILT_M);
	late_pow = maxval(late, INTERP_FILT_M);

	/*
	 * Late sample bank includes the centre sample
	 * Keep them discrete for now to avoid confusion
	 */
	if ((early_pow > late_pow) && (early_pow > peak->gain)) {
		peak->gain = early_pow;
		peak->whole = peak->orig - 1;
		peak->frac = maxidx(early, INTERP_FILT_M);
	} else if ((late_pow > early_pow) && (late_pow > peak->gain)) {
		peak->gain = late_pow;
		peak->whole = peak->orig;
		peak->frac = maxidx(late, INTERP_FILT_M);
	} else {
		peak->whole = peak->orig;
		peak->frac = 0; 
	}

	return 0; 
}

void init_peak_detect()
{
	int headrm = INTERP_FILT_WIDTH;

	init_interp_filt();

	rvec_init(&pow_vec, DEF_MAXLEN - headrm, DEF_MAXLEN,
		  headrm, power_buf);
	memset(pow_vec.buf, 0, pow_vec.buf_len * sizeof(short));
}


/* Consider the case of vector roll off and zero values are included */
/* Single sided width ignoring the adjacent two samples */
/* Total must be a factor of 2 */
/* Single sided width must be '4' */
int peak_to_mean(struct cxvec * vec, int peak, int idx, int width)
{
	int i;
	int sum = 0;

	for (i = 2; i <= (width + 1); i++) {
		sum += norm2(vec->data[idx - i]);
		sum += norm2(vec->data[idx + i]);
	}

	/* For 8 samples */
	/* Not square rooting this value so we need to make power comparisons */
	return ((peak >> 2) > (sum >> 3));
}
