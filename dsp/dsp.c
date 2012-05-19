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

#include <stdlib.h>
#include <math.h>
#include "dsp.h"

extern char *dbg;

float sinc(float x)
{
	if (x == 0.0f)
		return .9999999;

	return sin(M_PIf * x) / (M_PIf * x);
}

int flt_scale_h(float *h, int len, enum scale_type type)
{
	int i;
	float sum = 0;

	switch (type) {
	case SCALE_DC_GAIN:
		for (i = 0; i < len; i++)
			sum += sqrt(h[2 * i + 0] * h[2 * i + 0] +
				    h[2 * i + 0] * h[2 * i + 0]);
		break;
	case SCALE_NO_SAT:
		for (i = 0; i < len; i++)
			sum += abs(h[2 * i]);
		break;
	case SCALE_POW:
		for (i = 0; i < len; i++) {
			sum += h[2 * i + 0] * h[2 * i + 0];
			sum += h[2 * i + 1] * h[2 * i + 1];
		}
		break;
	default:
		return -1;
	}

	if (type == SCALE_POW)
		sum = sqrt(sum);

	for (i = 0; i < len; i++) {
		h[2 * i + 0] /= sum;
		h[2 * i + 1] /= sum;
	}

	return 0;
}

int cxvec_scale_h(struct cxvec *h, enum scale_type type)
{
	int rc;
	float *flt_h = (float *) malloc(h->len * 2 * sizeof(float));

	DSP_q15tofl(h->data, flt_h, h->len * 2);

	rc = flt_scale_h(flt_h, h->len, type);
	if (rc < 0) {
		free(flt_h);
		return -1;
	}

	DSP_fltoq15(flt_h, h->data, h->len * 2);

	free(flt_h);
	return 0;
}

void init_dsp()
{
	init_gmsk();
	init_delay();
	init_peak_detect();
}
