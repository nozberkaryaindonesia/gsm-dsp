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

#include <string.h>

#define CXVEC_FLG_REAL_ONLY	(1 << 0)
#define CXVEC_FLG_MEM_ALIGN	(1 << 1)
#define CXVEC_FLG_REVERSE	(1 << 2)

typedef struct complex {
	short real;
	short imag;
} complex;

struct cxvec {
	short len;
	short buf_len;
	short flags;
	short start_idx;
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
        int *data;
};

/* Complex vectors */
inline int cxvec_headrm(struct cxvec *vec)
{
        return vec->start_idx;
}

inline int cxvec_tailrm(struct cxvec *vec)
{
        return vec->buf_len - (vec->start_idx + vec->len);
}

inline void cxvec_clear_head(struct cxvec *vec)
{
        memset(vec->buf, 0, vec->start_idx * sizeof(complex));
}

inline void cxvec_clear_tail(struct cxvec *vec)
{
        memset(vec->data + vec->len, 0, cxvec_tailrm(vec));
}

void cxvec_init(struct cxvec *vec, int len,
		 int buf_len, int start, complex *buf);

int cxvec_rvrs(struct cxvec *vec);
int cxvec_rvrs_conj(struct cxvec *vec);
int cxvec_pow(struct cxvec *in, struct rvec *out);
int cxvec_norm2(struct cxvec *vec);

/* Real vectors */
inline int rvec_headrm(struct rvec *vec)
{
        return vec->start_idx;
}

inline int rvec_tailrm(struct rvec *vec)
{
        return vec->buf_len - (vec->start_idx + vec->len);
}

inline void rvec_clear_head(struct rvec *vec)
{
        memset(vec->buf, 0, vec->start_idx * sizeof(complex));
}

inline void rvec_clear_tail(struct rvec *vec)
{
        memset(vec->data + vec->len, 0, rvec_tailrm(vec));
}

void rvec_init(struct rvec *vec, int len, int buf_len, int start, short *buf);
int rvec_rvrs(struct rvec *vec);

/* Complex */
int norm2(complex val);

/* Floating point */
float flt_norm2(float *data, int len);

/* Integer */
int maxval(short *in, int len);
int maxidx(short *in, int len);

#endif /* _SIGVEC_H_ */
