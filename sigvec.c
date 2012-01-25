/*
 * sigvec.h
 *
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

void init_cxvec(struct cxvec *vec, int len, int buf_len, int start, complex *buf)
{
	vec->len = len;
	vec->buf_len = buf_len;
	vec->flags = 0;
	vec->start_idx = start;
	vec->buf = buf;
	vec->data = &buf[start];

	return;
}

void init_rvec(struct rvec *vec, int len, int buf_len, int start, short *buf)
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
int reverse_real(struct rvec *vec)
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

/* Vector must be a multiple of 2 */
int reverse_conj(struct cxvec *vec)
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

float fvec_norm2(float *data, int len)
{
        int i;
        float sum = 0.0f;

        for (i = 0; i < len; i++) {
                sum += data[i] * data[i];
        }

        return sum;
}
