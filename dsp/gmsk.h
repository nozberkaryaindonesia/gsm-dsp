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

#ifndef _GMSK_H_
#define _GMSK_H_

#include "sigvec.h"

enum freq_shft_dir {
	FREQ_SHFT_UP,
	FREQ_SHFT_DN
};

void init_gmsk();

void rotate(struct cxvec *restrict in_vec,
	    struct cxvec *restrict out_vec, enum freq_shft_dir);

int gmsk_mod(struct bitvec *restrict bvec,
	     struct cxvec *restrict h,
	     struct cxvec *restrict out);

int flt_gmsk_mod(struct bitvec *in, float *h, float *out, int h_len);

int gmsk_demod(struct cxvec *in, struct rvec *out);
int rach_demod(struct cxvec *in, struct rvec *out);

#endif /* _GMSK_H_ */

