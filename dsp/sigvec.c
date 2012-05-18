/*
 * Real and complex signal processing vectors
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

#include "sigvec.h"
#include "intrn.h"

void cxvec_init(struct cxvec *vec, int len, int buf_len, int start, complex *buf)
{
	vec->len = len;
	vec->buf_len = buf_len;
	vec->flags = 0;
	vec->start_idx = start;
	vec->buf = buf;
	vec->data = &buf[start];

	return;
}

void rvec_init(struct rvec *vec, int len, int buf_len, int start, short *buf)
{
	vec->len = len;
	vec->buf_len = buf_len;
	vec->flags = 0;
	vec->start_idx = start;
	vec->buf = buf;
	vec->data = &buf[start];

	return;
}

/* Vector must be a multiple of 2 */
int rvec_rvrs(struct rvec *vec)
{
	int i;
	short val;

	for (i = 0; i < (vec->len / 2); i++) {
		val = vec->data[i];

		vec->data[i] = vec->data[vec->len - 1 - i];
		vec->data[vec->len - 1 - i] = val;
	}

	return 0;
}

int cxvec_rvrs(struct cxvec *vec)
{
	int i;
	complex val;

	for (i = 0; i < (vec->len / 2); i++) {
		val = vec->data[i];

		vec->data[i].real = vec->data[vec->len - 1 - i].real;
		vec->data[i].imag = vec->data[vec->len - 1 - i].imag;

		vec->data[vec->len - 1 - i].real = val.real;
		vec->data[vec->len - 1 - i].imag = val.imag;
	}

	return 0;
}

int cxvec_rvrs_conj(struct cxvec *vec)
{
	int i;
	complex val;

	for (i = 0; i < (vec->len / 2); i++) {
		val = vec->data[i];

		vec->data[i].real = vec->data[vec->len - 1 - i].real;
		vec->data[i].imag = -(vec->data[vec->len - 1 - i].imag);

		vec->data[vec->len - 1 - i].real = val.real;
		vec->data[vec->len - 1 - i].imag = -val.imag;
	}

	return 0;
}

float flt_norm2(float *data, int len)
{
        int i;
        float sum = 0.0f;

        for (i = 0; i < len; i++) {
                sum += data[i] * data[i];
        }

        return sum;
}

int cxvec_scale(struct cxvec *in_vec, struct cxvec *out_vec, complex val, int shft)
{
#if 0
	int i;

	/* Packed casts */
	unsigned *restrict _in = (unsigned *) in->data;
	unsigned *restrict _out = (unsigned *) out->data;
	unsigned *restrict _val = (unsigned *) &val;
	for (i = 0; i < in->len; i++) {
		_out[i] = _cmpyr1(_in[i], *_val);
	}

	return in->len;
#else
	int i;
	int sum_a, sum_b;
	complex *in = in_vec->data;
	complex *out = out_vec->data;

	for (i = 0; i < in_vec->len; i++) {
		sum_a = (((int) in[i].real) << shft) * (int) val.real;
		sum_b = (((int) in[i].imag) << shft) * (int) val.imag;
		out[i].real = ((sum_a - sum_b) >> 15);

		sum_a = (((int) in[i].real) << shft) * (int) val.imag;
		sum_b = (((int) in[i].imag) << shft) * (int) val.real;
		out[i].imag = ((sum_a + sum_b) >> 15);
	}

	return 0;
#endif
}

/*
 * Input must be multiple of 4 and >= 8
 */
int cxvec_norm2(struct cxvec *vec)
{
	int sum;

	if ((vec->len % 4) || (vec->len < 8))
		return -1;
		
	sum = DSP_vecsumsq((short *) vec->data, 2 * vec->len);

	return sum;
}

/* No length restrictions */
int maxval(short *in, int len)
{
	int i;
	int max = 0;

	for (i = 0; i < len; i++) {
		if (in[i] > max)
			max = in[i];
	}

	return max;
}

int norm2(complex val)
{
	int sum;

	sum = (int) val.real * (int) val.real;
	sum += (int) val.imag * (int) val.imag;

	return (sum >> 15);
}
