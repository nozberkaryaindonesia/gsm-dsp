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

#include <dsplib.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../msg.h"
#include "../dsp/dsp.h"
#include "rach.h"

#define GSM_PLS_LEN             4

char *dbg;

static int start = 0;

/*
 * GSM Pulse filter
 */
complex gsm_pls_data[GSM_PLS_LEN];
struct cxvec gsm_pls;

static void init_gsm_pls()
{
	int i;
	float a = .96;
	float b = -1.1380;
	float c = .527;
	float f_pls[2 * GSM_PLS_LEN];
	float avg, div, vnorm;

	/* Pulse length of 4 */
	cxvec_init(&gsm_pls, GSM_PLS_LEN, GSM_PLS_LEN, 0, gsm_pls_data);

	f_pls[0] = 0.0f;
	f_pls[1] = 0.0f;
	f_pls[2] = a * exp(b - c);
	f_pls[3] = 0.0f; 
	f_pls[4] = a;
	f_pls[5] = 0.0f; 
	f_pls[6] = a * exp(b - c);
	f_pls[7] = 0.0f;

	vnorm = flt_norm2(f_pls, 2 * GSM_PLS_LEN);
	div = vnorm; /* div by sps if we supported */
	avg = sqrt(div);

	for (i = 0; i < (2 * GSM_PLS_LEN); i++) {
		f_pls[i] /= avg;
	}

	DSP_fltoq15(f_pls, (short *) gsm_pls.data, 2 * GSM_PLS_LEN);

	return; 
}

static void init_gsm()
{
	init_dsp();
	init_gsm_pls();
	init_rach(&gsm_pls);
	init_norm(&gsm_pls);

	return;
}

static void test_rach(char *in, char *out)
{
	struct cxvec in_vec;

	cxvec_init(&in_vec, 156, DEF_MAXLEN, 44, (complex *) in);

	detect_rach(&in_vec);
}

/* Main entry point */
int handle_msg(char *in, int in_len, char *out, int out_len)
{
	dbg = out;

	if (start == 0) {
		init_gsm();
		start = 1;
	}

	//test_rach(in, out);

	//DSP_q15tofl(in, (float *) dbg, 156 * 2);

	return 0;
}
