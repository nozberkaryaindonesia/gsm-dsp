/*
 * dsp.c
 *
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

/*
 * Note that these external libraries are copied into the DEBUG/RELEASE
 * directories because the library paths are broken or I'm just stupid.
 */

#include <dsplib.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "gsm.h"
#include "dsp.h"
#include "dsp_filter.h"
#include "dsp_intrn.h"

#define M_PIf				(3.14159265358979323846264338327f)

extern char *dbg;

float sinc(float x)
{
	if (x == 0.0f)
		return .9999999;

	return sin(M_PIf * x) / (M_PIf * x);
}

/*
 * Create complex floating point prototype filter and convert to Q15
 * 
 * Load the partition filters in reverse and shift the center tap by
 * M samples so that filter path '0' corresponds to center of the
 * sinc funtion with increasing delay by path number.
 *
 * Taps must be a factor of 2
 *
 * Input must be a factor of 4.
 */
static int init_delay_filt()
{
	int i, n;
	int m = DELAY_FILT_M;
	int prot_filt_len = DELAY_FILT_M * DELAY_FILT_WIDTH;
	float *prot_filt_f;
	short *prot_filt_i;

	float midpt = prot_filt_len / 2 + m; 

	prot_filt_f = (float *) malloc(prot_filt_len * 2 * sizeof(float));
	prot_filt_i = (short *) malloc(prot_filt_len * 2 * sizeof(short));

	for (i = 0; i < m; i++) {
		init_cxvec(&delay_filt[i], DELAY_FILT_WIDTH,
			   DELAY_FILT_WIDTH, 0, &delay_filt_data[i][0]);

	}
	for (i = 0; i < prot_filt_len; i++) {
		prot_filt_f[2*i + 0] = sinc(2*((float)i - midpt)*(GSMRATE/2)/(GSMRATE * m));
		prot_filt_f[2*i + 1] = 0;
	}

	DSP_fltoq15(prot_filt_f, prot_filt_i, 2 * prot_filt_len);

	for (i = 0; i < DELAY_FILT_WIDTH; i++) {
		for (n = 0; n < m; n++) {
			delay_filt[m-1-n].data[i].real = prot_filt_i[2 * i * m + 2 * n];
			delay_filt[m-1-n].data[i].imag = 0; 
		}
	}

	free(prot_filt_f);
	free(prot_filt_i);

	return 0;
}

/*
 * Create real floating point prototype filter and onvert to Q15
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
		init_rvec(&interp_filt[i], INTERP_FILT_WIDTH,
			  INTERP_FILT_WIDTH, 0, &interp_filt_data[i][0]);
	}

	for (i = 0; i < prot_filt_len; i++) {
		prot_filt_f[i] = sinc(2*((float)i - midpt)*(GSMRATE/2)/(GSMRATE * m));
	}

	DSP_fltoq15(prot_filt_f, prot_filt_i, prot_filt_len);

	for (i = 0; i < INTERP_FILT_WIDTH; i++) {
		for (n = 0; n < m; n++) {
			interp_filt[n].data[i] = prot_filt_i[i * m + n];
		}
	}

	for (i = 0; i < m; i++) {
		reverse_real(&interp_filt[i]);
	}

	free(prot_filt_f);
	free(prot_filt_i);

	return 0;
}

/*
 * Initialize 1/4 sample rate shift vectors in floating point and
 * convert to Q15.
 */
#define FREQ_SHFT_SCALE		0.4
static int init_freq_shft()
{
	int i;
	float *fvec;

	init_cxvec(&freq_shft_up, DEF_BUFLEN, DEF_BUFLEN, 0, freq_shft_up_data);
	init_cxvec(&freq_shft_dn, DEF_BUFLEN, DEF_BUFLEN, 0, freq_shft_dn_data);

	fvec = (float *) malloc(freq_shft_up.len * 2 * sizeof(float));

	for (i = 0; i < freq_shft_up.len; i++) {
		fvec[2*i + 1] = sin(2*M_PIf*((float) i / 4) / GSMRATE) / FREQ_SHFT_SCALE;
		fvec[2*i + 0] = cos(2*M_PIf*((float) i / 4) / GSMRATE) / FREQ_SHFT_SCALE;
	}

	DSP_fltoq15(fvec, (short *) freq_shft_up.data, freq_shft_up.len * 2);

	for (i = 0; i < freq_shft_dn.len; i++) {
		fvec[2*i + 1] = cos(2*M_PIf*((float) i / 4) / GSMRATE) / FREQ_SHFT_SCALE;
		fvec[2*i + 0] = sin(2*M_PIf*((float) i / 4) / GSMRATE) / FREQ_SHFT_SCALE;
	}

	DSP_fltoq15(fvec, (short *) freq_shft_dn.data, freq_shft_dn.len * 2);

	free(fvec);

	return 0;
}

/*
 * Delay a complex vector by N / M of a sample interval
 */
int delay(struct cxvec *in_vec, struct cxvec *out_vec, int delay)
{
	int ret;

	ret = convolve(in_vec, &delay_filt[delay], out_vec);
	if (ret < 0)
		return -1;

	return 0;
}

#define FULL_SCALE		25000

/*
 * Modulate a bit vector with GMSK
 */
int gmsk_mod(struct bitvec *bvec, struct cxvec *h, struct cxvec *out)
{
	int i;
	static complex in_data[DEF_BUFLEN];
	static struct cxvec in_vec;

	init_cxvec(&in_vec, bvec->len,
		   DEF_BUFLEN, h->len / 2, in_data);

	memset(in_vec.buf, 0, in_vec.buf_len * sizeof(complex));

	for (i = 0; i < bvec->len; i++) {
		in_vec.data[i].real = (2 * bvec->data[i] - 1) * FULL_SCALE / 8;
		in_vec.data[i].imag = 0;
	}

	convolve(&in_vec, h, out);

	return 0;
}

/* 
 * Calculate vector norm
 *
 * Input must be multiple of 4 and >= 8
 */
int vec_norm2(struct cxvec *vec)
{
	int sum;

	if ((vec->len % 4) || (vec->len < 8))
		return -1;
		
	sum = DSP_vecsumsq((short *) vec->data, 2 * vec->len);

	return sum;
}

int cxvec_energy_detct(struct cxvec *vec)
{
	return vec_norm2(vec);
}

int convolve(struct cxvec *in, struct cxvec *h, struct cxvec *out)
{
	int pad_len;

	pad_len = in->len % 4;
	in->len += pad_len;

	/* Shift start index to center */
	in->start_idx += h->len / 2;
	in->data = &in->buf[in->start_idx];

	/*
	 * Something about this call is broken in TI DSPLIB 3.1.0.0
	 * where the upper include defines itself out in the actual
	 * header with the declaration.
	 */
	DSP_fir_cplx((short *) in->data,
		     (short *) h->data,
		     (short *) out->data,
		     h->len, in->len);

	in->start_idx -= h->len / 2;
	in->data = &in->buf[in->start_idx];
	in->len -= pad_len;

	out->len = in->len; 
	memset(&out->data[out->len], 0, pad_len * sizeof(complex));

	return 0;
}

/*
 * Skip the custom bounds for now, but they'll only be index changes after
 * the convolve, which is interally always full span.
 */
int correlate(struct cxvec *in, struct cxvec *h, struct cxvec *out)
{
	//DSP_q15tofl((short *) h->data, (float *) dbg, 2 * h->len);

	convolve(in, h, out);

	return 0;
}

/*
 * Check for 'restrict' validity
 */
int vec_power(struct cxvec *in_vec, struct rvec *out_vec)
{
	int i;
	complex *in = in_vec->data;
	short *out = out_vec->data;

#ifdef INTRN_DOTP2
	for (i = 0; i < in->len, i++) {
		out[i] = (_dotp2((int) in[i], (int) in[i]) >> 15);
	}
#else
#ifdef INTRN_SADD
	int sum_r, sum_i;

	for (i = 0; i < in_vec->len; i++) {
		sum_r = ((int) in[i].real) * ((int) in[i].real);
		sum_i = ((int) in[i].imag) * ((int) in[i].imag);
		out[i] = (_sadd(sum_r, sum_i) >> 15);
	}
#else
	int sum_r, sum_i;

	for (i = 0; i < in_vec->len; i++) {
		sum_r = ((int) in[i].real) * ((int) in[i].real);
		sum_i = ((int) in[i].imag) * ((int) in[i].imag);
		out[i] = ((sum_r + sum_i) >> 15);
	}
#endif
#endif
	return 0;
}

/*
 * Does not reset start index
 * Taps multiple of 16. Data multiples of 4.
 */
int convolve_real(struct rvec *in, struct rvec *h, struct rvec *out)
{
	int pad_len;

	pad_len = in->len % INTERP_FILT_MIN;
	in->len += pad_len;

	DSP_fir_r8(in->data, h->data, out->data, h->len, in->len);

	in->len -= pad_len;
	out->len = in->len; 

	memset(&out->data[out->len], 0, pad_len * sizeof(short));

	return 0;
}

/*
 * Resets start index. Data must start on h->len / 2;
 * Taps multiple of 16. Data multiples of 4.
 */
int delay_real(struct rvec *in, struct rvec *h, struct rvec *out)
{
	int pad_len, orig_idx;

	orig_idx = in->start_idx;
	in->start_idx = 0;
	in->data = &in->buf[in->start_idx];

	pad_len = in->len % 4;
	in->len += pad_len;

	DSP_fir_r8(in->data, h->data, out->data, h->len, in->len);

	in->start_idx = orig_idx;
	in->len -= pad_len;
	out->len = in->len; 

	memset(&out->data[out->len], 0, pad_len * sizeof(short));

	return 0;
}

int rotate(struct cxvec *in_vec, struct cxvec *out_vec, int up)
{
	int i;

#ifdef INTRN_CMPYR
	/* Packed casts */
	unsigned *_in = (unsigned *) in_vec->data;
	unsigned *_out = (unsigned *) out_vec->data;
	unsigned *_rot;

	if (up)
		_rot = (unsigned *) &freq_shft_up;
	else
		_rot = (unsigned *) &freq_shft_dn;

	/* Why can't I cast packed 16-bit int to 'complex'? */
	for (i = 0; i < in_vec->len; i++) {
		_out[i] = _cmpyr(_in[i], _rot[i]);
	}
#else
	struct cxvec *rot;
	int sum_a, sum_b;
	complex *in = in_vec->data;
	complex *out = out_vec->data;
	complex *rot;

	if (up)
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
	return 0;
}

/*
 * Determine the case where the correlation point is at the very
 * end and a minimally padded vector will still roll off the end
 * before it gets to the center tap.
 */
static int interp_pt(struct rvec *in, short idx, int frac, short *early, short *late)
{
	/* Output buffer needs to be half filter width */
	static short out_buf[DEF_BUFLEN];
	static struct rvec interp_vec, out_vec;

	/*
	 * Start index is important here
	 *   '0' start_idx represents centering sample '0' of the input
	 *   signal at center tap. 
	 */
	interp_vec.len = INTERP_FILT_MIN;
	interp_vec.buf_len = in->buf_len;
	interp_vec.start_idx = idx;
	interp_vec.buf = in->buf;
	interp_vec.data = &interp_vec.buf[interp_vec.start_idx]; 

	/* Always reset this vector */
	out_vec.len = 0; 
	out_vec.buf_len = INTERP_FILT_WIDTH;
	out_vec.start_idx = 0; 
	out_vec.buf = out_buf;
	out_vec.data = out_vec.buf;

	convolve_real(&interp_vec, &interp_filt[frac], &out_vec);

	*early = out_vec.data[0];
	*late = out_vec.data[1];

	return 0;
}

int maxval(short *in, int len)
{
	int i;
	int max = -32768;

	for (i = 0; i < len; i++) {
		if (in[i] > max)
			max = in[i];
	}

	return max;
}

int maxidx(short *in, int len)
{
	int i, idx;
	int max = -32768;

	for (i = 0; i < len; i++) {
		if (in[i] > max) {
			max = in[i];
			idx = i;
		}
	}

	return idx;
}

/* 
 * Peak detect
 * Watch for overflow? use Q31 values?
 * FIXME: Must be multiple of 16 and >= 48
*/
int peak_detect(struct cxvec *in_vec, struct vec_peak *peak)
{
	int i;
	int m = INTERP_FILT_M;
	short max_idx, early_pow, late_pow;

	static short early[INTERP_FILT_M];
	static short late[INTERP_FILT_M];

	static short pow_vec_buf[DEF_BUFLEN];
	struct rvec pow_vec;

	init_rvec(&pow_vec, in_vec->len, DEF_BUFLEN,
		  INTERP_FILT_WIDTH / 2, pow_vec_buf);

	memset(pow_vec.buf, 0, pow_vec.buf_len * sizeof(short));

	/* Find the power */
	vec_power(in_vec, &pow_vec);

	if ((in_vec->len % 16) == 0) {
		peak->pow = DSP_maxval(pow_vec.data, in_vec->len);
		max_idx = DSP_maxidx((short *) pow_vec.data, pow_vec.len);
	} else {
		peak->pow = maxval(pow_vec.data, in_vec->len);
		max_idx = maxidx((short *) pow_vec.data, pow_vec.len);
	}

	/* Drop back one sample so we do early / late  in the same pass */
	for (i = 0; i < m; i++) {
		interp_pt(&pow_vec, max_idx - 1, i, &early[i], &late[i]);
	}

	peak->orig = max_idx;
	if ((INTERP_FILT_M % 16) == 0) {
		early_pow = DSP_maxval(early, INTERP_FILT_M);
		late_pow = DSP_maxval(late, INTERP_FILT_M);
	} else {
		early_pow = maxval(early, INTERP_FILT_M);
		late_pow = maxval(late, INTERP_FILT_M);
	}

	/*
	 * Late sample bank includes the centre sample
	 * Keep them discrete for now to avoid confusion
	 */
	if ((early_pow > late_pow) && (early_pow > peak->pow)) {
		peak->pow = early_pow;
		peak->whole = max_idx - 1;
		peak->frac = DSP_maxidx(early, INTERP_FILT_M);
	} else if ((late_pow > early_pow) && (late_pow > peak->pow)) {
		peak->pow = late_pow;
		peak->whole = max_idx;
		peak->frac = DSP_maxidx(late, INTERP_FILT_M);
	} else {
		peak->whole = max_idx;
		peak->frac = 0; 
	}

	return 0; 
}

void init_dsp()
{
	init_delay_filt();
	init_interp_filt();
	init_freq_shft();
}
