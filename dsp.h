/*
 * dsp.h
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

#ifndef _DSP_H_
#define _DSP_H_

#include "sigvec.h"

void init_dsp();
int convolve(struct cxvec *in, struct cxvec *h,struct cxvec *out);
int correlate(struct cxvec *a, struct cxvec *b, struct cxvec *c);
int peak_detect(struct cxvec *in_vec, struct vec_peak *peak);
int gmsk_mod(struct bitvec *in, struct cxvec *h, struct cxvec *out);
int rotate(struct cxvec *in_vec, struct cxvec *out_vec, int up);

#endif /* _DSP_H_ */
