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

#ifndef _CONVOLVE_H_
#define _CONVOLVE_H_

#include "sigvec.h"

enum conv_type {
	CONV_NO_DELAY,
	CONV_START
};

int cxvec_convolve(struct cxvec *restrict in,
		   struct cxvec *restrict h,
		   struct cxvec *restrict out,
		   enum conv_type type);

int cx_conv2(complex *restrict in, struct cxvec *restrict h,
	     complex *restrict out, enum conv_type type);

int cxvec_correlate(struct cxvec *restrict in,
		    struct cxvec *restrict h,
		    struct cxvec *restrict out,
		    enum conv_type type);

int flt_conv_start(float *in, float *h, float *out, int h_len, int in_len);
int flt_conv_no_delay(float *in, float *h, float *out, int h_len, int in_len);

#endif /* _CONVOLVE_H_ */
