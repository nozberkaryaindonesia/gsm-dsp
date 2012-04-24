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

/* No length restrictions */
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

/* No length restrictions */
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

int norm2(complex val)
{
	int sum;

	sum = (int) val.real * (int) val.real;
	sum += (int) val.imag * (int) val.imag;

	return (sum >> 15);
}
