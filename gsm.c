/*
 * gsm.c
 *
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

#include "dsp.h"
#include "gsm_data.h"

static int start = 0;
char *dbg;

static void init_gsm_pls()
{
	int i;
	float a = .96;
	float b = -1.1380;
	float c = .527;
	float f_pls[2 * GSM_PLS_LEN];
	float avg, div, vnorm;

	/* Pulse length of 4 */
	init_cxvec(&gsm_pls, GSM_PLS_LEN, GSM_PLS_LEN, 0, gsm_pls_data);

	f_pls[0] = 0.0f;
	f_pls[1] = 0.0f;
	f_pls[2] = a * exp(b - c);
	f_pls[3] = 0.0f; 
	f_pls[4] = a;
	f_pls[5] = 0.0f; 
	f_pls[6] = a * exp(b - c);
	f_pls[7] = 0.0f;

	vnorm = fvec_norm2(f_pls, 2 * GSM_PLS_LEN);
	div = vnorm; /* div by sps if we supported */
	avg = sqrt(div);

	for (i = 0; i < (2 * GSM_PLS_LEN); i++) {
		f_pls[i] /= avg;
	}

	DSP_fltoq15(f_pls, (short *) gsm_pls.data, 2 * GSM_PLS_LEN);

	return; 
}

static int init_tsc()
{
	int i, ret;
	struct bitvec tsc_bvec;
	struct cxvec auto_corr;

	/* Modulated training sequences */
	for (i = 0; i < NUM_TSC; i++) {
		tsc_bvec.len = TSC_LEN;
		tsc_bvec.data = &tsc_seqs[i][0];

		init_cxvec(&tsc_cvecs[i], tsc_bvec.len,
			   DEF_BUFLEN, MIDAMBL_LEN, &tsc_cvec_data[i][0]);

		memset(tsc_cvecs[i].buf, 0, tsc_cvecs[i].buf_len * sizeof(complex));

		/* FIXME Guard interval? */
		ret = gmsk_mod(&tsc_bvec, &gsm_pls, &tsc_cvecs[i]);
		if (ret < 0)
			return -1;
	}

	/* Modulated training sequences midambles */
	for (i = 0; i < NUM_TSC; i++) {
		tsc_bvec.len = MIDAMBL_LEN;
		tsc_bvec.data = &tsc_seqs[i][MIDAMBL_STRT];

		init_cxvec(&midambl_cvecs[i], tsc_bvec.len,
			   DEF_BUFLEN, 0, &midambl_cvec_data[i][0]);

		memset(midambl_cvecs[i].buf, 0, midambl_cvecs[i].buf_len * sizeof(complex));

		/* FIXME Guard interval? */
		ret = gmsk_mod(&tsc_bvec, &gsm_pls, &midambl_cvecs[i]);
		if (ret < 0)
			return -1;

		reverse_conj(&midambl_cvecs[i]);
	}

	/* Autocorrelation */
	auto_corr.buf = malloc(DEF_BUFLEN * sizeof(complex));
	init_cxvec(&auto_corr, TSC_LEN, DEF_BUFLEN, 0, auto_corr.buf); 

	/* Need this for good measure on Q side */
	memset(auto_corr.buf, 0, auto_corr.buf_len * sizeof(complex));

	for (i = 0; i < NUM_TSC; i++) {
		correlate(&tsc_cvecs[i], &midambl_cvecs[i], &auto_corr);
		peak_detect(&auto_corr, &midambl_peaks[i]);
	}

	free(auto_corr.buf);

	return 0;
}

static void init_gsm()
{
	init_dsp();
	init_gsm_pls();
	init_tsc();

	return;
}

/* Main entry point */
int handle_msg(char *in_buf, int in_len, char *out_buf, int out_len)
{
	int i;
	dbg = out_buf;

	if (!start) {
		init_gsm();
		start = 1;
	}

	memset(dbg, 0, 512);

	for (i = 0; i < NUM_TSC; i++) {
		memcpy(dbg + i * sizeof(struct vec_peak), &midambl_peaks[i], sizeof(struct vec_peak));
	}

	return 0;
}


