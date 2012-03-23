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
#include "normal.h"
#include "../dsp/dsp.h"

#define USE_TSC			0
#define NUM_TSC			8
#define TSC_LEN			26

/*
 * Training sequences and midambles
 */
int tsc_seqs[NUM_TSC][TSC_LEN] = {
        { 0,0,1,0,0,1,0,1,1,1,0,0,0,0,1,0,0,0,1,0,0,1,0,1,1,1 },
        { 0,0,1,0,1,1,0,1,1,1,0,1,1,1,1,0,0,0,1,0,1,1,0,1,1,1 },
        { 0,1,0,0,0,0,1,1,1,0,1,1,1,0,1,0,0,1,0,0,0,0,1,1,1,0 },
        { 0,1,0,0,0,1,1,1,1,0,1,1,0,1,0,0,0,1,0,0,0,1,1,1,1,0 },
        { 0,0,0,1,1,0,1,0,1,1,1,0,0,1,0,0,0,0,0,1,1,0,1,0,1,1 },
        { 0,1,0,0,1,1,1,0,1,0,1,1,0,0,0,0,0,1,0,0,1,1,1,0,1,0 },
        { 1,0,1,0,0,1,1,1,1,1,0,1,1,0,0,0,1,0,1,0,0,1,1,1,1,1 },
        { 1,1,1,0,1,1,1,1,0,0,0,1,0,0,1,0,1,1,1,0,1,1,1,1,0,0 },
};

complex tsc_cvec_data[NUM_TSC][DEF_MAXLEN];
struct cxvec tsc_cvecs[NUM_TSC];

#define MIDAMBL_STRT		5
#define MIDAMBL_LEN             16
complex midambl_cvec_data[NUM_TSC][DEF_MAXLEN];
struct cxvec midambl_cvecs[NUM_TSC];
struct vec_peak midambl_peaks[NUM_TSC];

/*
 * Correlation output
 */
static complex corr_out_data[DEF_MAXLEN];
static struct cxvec corr_out;

/*
 * Delay compensated RACH burst
 */
static complex delay_data[DEF_MAXLEN];
static struct cxvec delay_brst;

/*
 * Bit output
 */
static complex demod_data[DEF_MAXLEN];
static struct cxvec demod_vec;

static int init_tsc(struct cxvec *pls)
{
	int i, rc;
	struct bitvec tsc_bvec;
	struct cxvec auto_corr;

	/* Modulated training sequences */
	for (i = 0; i < NUM_TSC; i++) {
		tsc_bvec.len = TSC_LEN;
		tsc_bvec.data = &tsc_seqs[i][0];

		cxvec_init(&tsc_cvecs[i], tsc_bvec.len,
			   DEF_MAXLEN, MIDAMBL_LEN, &tsc_cvec_data[i][0]);

		memset(tsc_cvecs[i].buf, 0, tsc_cvecs[i].buf_len * sizeof(complex));

		/* FIXME Guard interval? */
		rc = gmsk_mod(&tsc_bvec, pls, &tsc_cvecs[i]);
		if (rc < 0)
			return -1;
	}

	/* Modulated training sequences midambles */
	for (i = 0; i < NUM_TSC; i++) {
		tsc_bvec.len = MIDAMBL_LEN;
		tsc_bvec.data = &tsc_seqs[i][MIDAMBL_STRT];

		cxvec_init(&midambl_cvecs[i], tsc_bvec.len,
			   DEF_MAXLEN, 0, &midambl_cvec_data[i][0]);

		memset(midambl_cvecs[i].buf, 0, midambl_cvecs[i].buf_len * sizeof(complex));

		/* FIXME Guard interval? */
		rc = gmsk_mod(&tsc_bvec, pls, &midambl_cvecs[i]);
		if (rc < 0)
			return -1;

		cxvec_rvrs(&midambl_cvecs[i]);
	}
#if 0
	/* Autocorrelation */
	auto_corr.buf = malloc(DEF_MAXLEN * sizeof(complex));
	cxvec_init(&auto_corr, TSC_LEN, DEF_MAXLEN, 0, auto_corr.buf); 

	/* Need this for good measure on Q side */
	memset(auto_corr.buf, 0, auto_corr.buf_len * sizeof(complex));

	for (i = 0; i < NUM_TSC; i++) {
		cxvec_correlate(&tsc_cvecs[i], &midambl_cvecs[i], &auto_corr);
		peak_detect(&auto_corr, &midambl_peaks[i]);
	}

	free(auto_corr.buf);
#endif

	return 0;
}

int detect_tsc(struct cxvec *in)
{
	int rc;
	struct vec_peak peak;

	if ((in->len > DEF_MAXLEN) ||
	    (cxvec_headrm(in) < (MIDAMBL_LEN / 2 - 1)) ||
	    (cxvec_tailrm(in) < (MIDAMBL_LEN / 2))) {
		return -1;
	}

	corr_out.len = in->len;
	memset(corr_out.buf, 0, corr_out.buf_len * sizeof(complex));

	rc = cxvec_correlate(in, &midambl_cvecs[USE_TSC], &corr_out, CONV_NO_DELAY);
	if (rc < 0)
		return -1;

	rc = cxvec_peak_detect(&corr_out, &peak);
	if (rc < 0)
		return -1;

	delay_brst.len = in->len;
	cxvec_advance(in, &delay_brst, 0, peak.frac);

	demod_vec.len = 156;
	gmsk_demod(&delay_brst, NULL, &demod_vec);

	//memcpy(dbg, &peak, sizeof(peak));

	return 0;
}

void init_norm(struct cxvec *pls)
{
	init_tsc(pls);
	cxvec_init(&corr_out, DEF_MAXLEN, DEF_MAXLEN, 0, corr_out_data);
	cxvec_init(&delay_brst, DEF_MAXLEN, DEF_MAXLEN, 0, delay_data);

	demod_vec.data = demod_data;
}
