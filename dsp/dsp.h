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

#ifndef _DSP_H_
#define _DSP_H_

#include "sigvec.h"
#include "convolve.h"
#include "gmsk.h"
#include "delay.h"
#include "peak_detect.h"

#define DEF_MAXLEN		256
#define M_PIf			(3.14159265358979323846264338327f)

enum scale_type {
	SCALE_DC_GAIN,
	SCALE_NO_SAT,
	SCALE_POW
};

float sinc(float x);
int flt_scale_h(float *h, int len, enum scale_type type);
int cxvec_scale_h(struct cxvec *h, enum scale_type type);
int cxvec_rot90(struct cxvec *h);
 
void init_dsp();

#endif /* _DSP_H_ */
