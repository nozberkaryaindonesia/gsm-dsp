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
#include <dsplib.h>
#include "dsp.h"

#define DELAY_FILT_M                    32
#define DELAY_FILT_WIDTH                8
#define DELAY_FILT_MIN                  4

/* 
 * Fractional delay filterbank
 * Complex input with real taps
 */
complex delay_filt_data[DELAY_FILT_M][DELAY_FILT_WIDTH];
struct cxvec delay_filt[DELAY_FILT_M];

/*
 * Create complex floating point prototype filter and convert to Q15
 * 
 * Load the partition filters in reverse and shift the center tap by
 * M samples so that filter path '0' corresponds to center of the
 * sinc funtion with increasing delay by path number.
 *
 * Taps must be a factor of 2
 *
 * Input must be a factor of 4.
 */
static int init_delay_filt()
{
	int i, n;
	int m = DELAY_FILT_M;
	int prot_filt_len = DELAY_FILT_M * DELAY_FILT_WIDTH;
	float *prot_filt_f;
	short *prot_filt_i;

	float midpt = prot_filt_len / 2 - m; 

	prot_filt_f = (float *) malloc(prot_filt_len * sizeof(float));
	prot_filt_i = (short *) malloc(prot_filt_len * sizeof(short));

	for (i = 0; i < m; i++) {
		cxvec_init(&delay_filt[i], DELAY_FILT_WIDTH,
			   DELAY_FILT_WIDTH, 0, &delay_filt_data[i][0]);

	}
	for (i = 0; i < prot_filt_len; i++) {
		prot_filt_f[i] = sinc(((float) i - midpt) / m);
	}

	DSP_fltoq15(prot_filt_f, prot_filt_i, prot_filt_len);

	/* Normal loading */
	for (i = 0; i < DELAY_FILT_WIDTH; i++) {
		for (n = 0; n < m; n++) {
			delay_filt[n].data[i].real = prot_filt_i[i * m + n];
			delay_filt[n].data[i].imag = 0; 
		}
	}

	free(prot_filt_f);
	free(prot_filt_i);

	return 0;
}

int cxvec_advance(struct cxvec *in, struct cxvec *out, int whole, int frac)
{
	cxvec_convolve(in, &delay_filt[frac], out, CONV_NO_DELAY);

	out->start_idx += whole;
	out->data = &out->buf[out->start_idx];

	return 0;
}

void init_delay()
{
	init_delay_filt();
}
