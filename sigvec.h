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

#ifndef _SIGVEC_H_
#define _SIGVEC_H_

typedef struct complex {
	short real;
	short imag;
} complex;

struct vec_peak {
	int orig;
	int whole;
	int frac;
	int pow;
};

struct cxvec {
	int len;
	int buf_len;
	int flags;
	int start_idx;
	complex *buf;
	complex *data;
	complex _buf[1];
};

struct rvec {
	int len;
	int buf_len;
	int flags;
	int start_idx;
	short *buf;
	short *data;
	short _buf[1];
};

struct bitvec {
        int len;
        int const *data;
};

/* Complex vectors */
void init_cxvec(struct cxvec *vec, int len, int buf_len, int start, complex *buf);
int reverse_conj(struct cxvec *vec);

/* Real vectors */
void init_rvec(struct rvec *vec, int len, int buf_len, int start, short *buf);
int reverse_real(struct rvec *vec);

/* Floating point utilities */
float fvec_norm2(float *data, int len);

#endif /* _SIGVEC_H_ */
