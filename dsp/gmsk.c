/*
 * TI C64x+ GSM signal processing
 *
 * Copyright (C) 2012 Thomas Tsou <ttsou@vt.edu>
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
#include <dsplib.h>
#include <math.h>
#include "dsp.h"
#include "sigvec.h"
#include "convolve.h"
#include "gmsk.h"

extern char *dbg;

/*
 * 1/4 sample rate frequency shift vector
 */
#define FREQ_SHFT_SCALE			0.9

static complex freq_shft_up_data[DEF_MAXLEN];
static complex freq_shft_dn_data[DEF_MAXLEN];
static struct cxvec freq_shft_up;
static struct cxvec freq_shft_dn;

/*
 * GMSK I/O buffers for frequency rotation
 */
static complex rot_in_data[DEF_MAXLEN];
static complex rot_out_data[DEF_MAXLEN];
static struct cxvec rot_in;
static struct cxvec rot_out;

/*
 * Initialize 1/4 sample rate shift vectors in floating point and
 * convert to Q15.
 */
static int init_freq_shft()
{
	int i;
	float *fvec;

	cxvec_init(&freq_shft_up, DEF_MAXLEN, DEF_MAXLEN, 0, freq_shft_up_data);
	cxvec_init(&freq_shft_dn, DEF_MAXLEN, DEF_MAXLEN, 0, freq_shft_dn_data);

	fvec = (float *) malloc(freq_shft_up.len * 2 * sizeof(float));

	for (i = 0; i < freq_shft_up.len; i++) {
		fvec[2*i + 0] = sin(M_PIf*((float) i / 2)) / FREQ_SHFT_SCALE;
		fvec[2*i + 1] = cos(M_PIf*((float) i / 2)) / FREQ_SHFT_SCALE;
	}

	DSP_fltoq15(fvec, (short *) freq_shft_up.data, freq_shft_up.len * 2);

	for (i = 0; i < freq_shft_dn.len; i++) {
		fvec[2*i + 0] = sin(M_PIf*((float) -i / 2)) / FREQ_SHFT_SCALE;
		fvec[2*i + 1] = cos(M_PIf*((float) -i / 2)) / FREQ_SHFT_SCALE;
	}

	DSP_fltoq15(fvec, (short *) freq_shft_dn.data, freq_shft_dn.len * 2);

	free(fvec);

	return 0;
}

void rotate(struct cxvec *restrict in_vec,
	    struct cxvec *restrict out_vec, enum freq_shft_dir dir)
{
	int i;

#ifdef INTRN_CMPYR
	/* Packed casts */
	unsigned *restrict _in = (unsigned *) in_vec->data;
	unsigned *restrict _out = (unsigned *) out_vec->data;
	unsigned *restrict _rot;

	if (dir == FREQ_SHFT_UP)
		_rot = (unsigned *) &freq_shft_up;
	else
		_rot = (unsigned *) &freq_shft_dn;

	for (i = 0; i < in_vec->len; i++) {
		_out[i] = _cmpyr(_in[i], _rot[i]);
	}
#else
	int sum_a, sum_b;
	complex *in = in_vec->data;
	complex *out = out_vec->data;
	complex *rot;

	if (dir == FREQ_SHFT_UP)
		rot = freq_shft_up.data;
	else
		rot = freq_shft_dn.data;

	for (i = 0; i < in_vec->len; i++) {
		sum_a = (int) in[i].real * (int) rot[i].real;
		sum_b = (int) in[i].imag * (int) rot[i].imag;
		out[i].real = ((sum_a - sum_b) >> 15);

		sum_a = (int) in[i].real * (int) rot[i].imag;
		sum_b = (int) in[i].imag * (int) rot[i].real;
		out[i].imag = ((sum_a + sum_b) >> 15);
	}
#endif
}

static inline void bvec_to_cxvec(struct bitvec *bvec, struct cxvec *cvec)
{
	int i;

	for (i = 0; i < bvec->len; i++) {
		cvec->data[i].real = (2 * bvec->data[i] - 1) * 16000;
		cvec->data[i].imag = 0;
	}
}

/*
 * Modulate a bit vector with GMSK. We only need this for the correlation
 * sequences where no guard interval is necessary.
 */
int gmsk_mod(struct bitvec *restrict bvec,
	     struct cxvec *restrict h,
	     struct cxvec *restrict out)
{
	int rc;

	if (bvec->len > DEF_MAXLEN) {
		return -1;
	}

	rot_in.len = out->len;
	rot_out.len = out->len;

	bvec_to_cxvec(bvec, &rot_in);

	/* Frequency shift 1/4 rate up */
	rotate(&rot_in, &rot_out, FREQ_SHFT_UP);

	//DSP_q15tofl(rot_out.data, dbg, 16 * 2);

	/* Pulse shaping */
	rc = cxvec_convolve(&rot_out, h, out, CONV_NO_DELAY);
	if (rc < 0) {
		return -1;
	}

	//DSP_q15tofl((short *) out->data, (float *) dbg, 16 * 2);

	return 0;
}

static int slice(struct cxvec *in, short *out)
{
	short i;

	/* Hack */
	for (i = 0; i < in->len; i++) {
#if 1
		if (in->data[i + 1].real > 0)
			out[i] = 32000;
		else
			out[i] = 0;
#else
		if (in->data[i + 1].real > 0)
			out[i] = ((2 * in->data[i + 1].real) >> 1) + 16380;
		else
			out[i] = (2 * in->data[i + 1].real + 32760) >> 1;
#endif
	}

	return 0;
}

static int slice2(struct cxvec *in, short *out)
{
	short i;

	for (i = 0; i < in->len; i++) {
#if 1
		if (in->data[i].real < 0)
			out[i] = 32000;
		else
			out[i] = 0;
#else
		if (in->data[i].real < 0)
			out[i] = (2 * in->data[i].real + 32760) >> 1;
		else
			out[i] = ((2 * in->data[i].real) >> 1) + 16380;
#endif
	}

	return 0;
}

#if 1
int gmsk_demod(struct cxvec *in, struct cxvec *h, struct rvec *out)
{
	if (in->len > DEF_MAXLEN) {
		return -1;
	}

	rot_in.len = out->len;
	rot_out.len = out->len;

	memset(rot_out.buf, 0, rot_out.buf_len * sizeof(complex));

	rotate(in, &rot_out, FREQ_SHFT_UP);

	slice(&rot_out, out->data);
	//DSP_q15tofl((short *) rot_out.data, (float *) dbg, 156 * 2);

	return 0;
}
#endif

int rach_demod(struct cxvec *in, struct cxvec *h, struct rvec *out)
{
	if (in->len > DEF_MAXLEN) {
		return -1;
	}

	rot_in.len = out->len;
	rot_out.len = out->len;

	memset(rot_out.buf, 0, rot_out.buf_len * sizeof(complex));

	rotate(in, &rot_out, FREQ_SHFT_UP);

	slice(&rot_out, out->data);
	//DSP_q15tofl((short *) rot_out.data, (float *) dbg, 156 * 2);

	return 0;
}

void init_gmsk()
{
	init_freq_shft();

	cxvec_init(&rot_in, DEF_MAXLEN, DEF_MAXLEN, 0, rot_in_data);
	cxvec_init(&rot_out, DEF_MAXLEN, DEF_MAXLEN - 64, 64, rot_out_data);

	memset(rot_out.data, 0, rot_out.buf_len * sizeof(complex));
}
