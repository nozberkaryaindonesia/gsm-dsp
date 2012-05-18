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
#include "../gsm_hdr.h"
#include "../dsp/dsp.h"
#include "rach.h"
#include "normal.h"

#define GSM_PLS_LEN             4

char *dbg;

static int start = 0;

enum brst_type {
	RACH,
	TSC
};

enum brst_type get_corr_type(struct gsm_hdr *hdr)
{
	int tn = hdr->time.tn;
	int fn = hdr->time.fn;
	int mod51;

	if (tn == 0) {
		mod51 = fn % 51;

		if ((mod51 <= 36) && (mod51 >= 14))
			return RACH;
		else if ((mod51 == 4) || (mod51 == 5))
			return RACH;
		else if ((mod51 == 45) || (mod51 == 46))
			return RACH;
		else
			return TSC;
	} else {
		return TSC;
	}
}

/*
 * GSM Pulse filter
 */
complex gsm_pls_data[GSM_PLS_LEN];
struct cxvec gsm_pls;

float flt_gsm_pls[GSM_PLS_LEN * 2];

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
#if 0
	f_pls[0] = 0.0f;
	f_pls[1] = 0.0f;
	f_pls[2] = a * exp(b - c);             /* .1816230 */
	f_pls[3] = 0.0f; 
	f_pls[4] = a;                          /* .96 */
	f_pls[5] = 0.0f; 
	f_pls[6] = a * exp(b - c);             /* .1816230 */
	f_pls[7] = 0.0f;
#else
	f_pls[0] = a * exp(b - c);             /* .1816230 */
	f_pls[1] = 0.0f; 
	f_pls[2] = a;                          /* .96 */
	f_pls[3] = 0.0f; 
	f_pls[4] = a * exp(b - c);             /* .1816230 */
	f_pls[5] = 0.0f;
	f_pls[6] = 0.0f;
	f_pls[7] = 0.0f;
#endif
	memcpy(flt_gsm_pls, f_pls, GSM_PLS_LEN * 2 * sizeof(float));

	vnorm = flt_norm2(f_pls, 2 * GSM_PLS_LEN);

	/* Divide by samples-per-symbol if we supported it */
	div = vnorm;
	avg = sqrt(div);

	for (i = 0; i < (2 * GSM_PLS_LEN); i++) {
		f_pls[i] /= avg;
	}

	/* .18276207, .96602073, .18276207 */
	/* 5989, 31655, 5989 */
	DSP_fltoq15(f_pls, (short *) gsm_pls.data, 2 * GSM_PLS_LEN);

	return; 
}

static void init_gsm()
{
	init_dsp();
	init_gsm_pls();
	init_rach(&gsm_pls, flt_gsm_pls);
	init_norm(&gsm_pls, flt_gsm_pls);

	return;
}

static int test_rach(void *in, char *out, struct test_hdr *hdr)
{
	struct cxvec in_vec;

	cxvec_init(&in_vec, 156, DEF_MAXLEN, 44, (complex *) in);

	return detect_rach(&in_vec, hdr);
}

static int test_tsc(void *in, char *out, struct test_hdr *hdr)
{
	struct cxvec in_vec;

	cxvec_init(&in_vec, 156, DEF_MAXLEN, 44, (complex *) in);

	return detect_tsc(&in_vec, hdr);
}

/* Main entry point */
int handle_msg(char *in, int in_len, char *out, int out_len)
{
	int rc, good;
	struct gsm_hdr hdr_in;
	struct gsm_hdr *hdr_out;
	enum brst_type type;
	struct test_hdr hdr_test;
	dbg = out;

	if (start == 0) {
		init_gsm();
		start = 1;
	}

	memcpy(&hdr_in, in, sizeof(struct gsm_hdr));
	type = get_corr_type(&hdr_in);

	good = 1;

	switch (type) {
	case RACH:
		rc = test_rach(in, out, &hdr_test);
		if (rc < 0)
			good = 0;
		break;
	case TSC:
		rc = test_tsc(in, out, &hdr_test);
		if (rc < 0)
			good = 0;
		break;
	default:
		memset(out, 0, sizeof(struct gsm_hdr));
		return -1;
	}

	hdr_out = (struct gsm_hdr *) out;
	if (good) {
		hdr_out->time.tn = hdr_in.time.tn;
		hdr_out->time.fn = hdr_in.time.fn;
		hdr_out->data_offset = hdr_in.data_offset;
		hdr_out->data_len = 156;
		hdr_out->toa = rc; 

		hdr_out->x0 = hdr_test.x0;
		hdr_out->x1 = hdr_test.x1;
		hdr_out->x2 = hdr_test.x2;
		hdr_out->x3 = hdr_test.x3;
		hdr_out->x4 = hdr_test.x4;
		hdr_out->x5 = hdr_test.x5;
		hdr_out->x6 = hdr_test.x6;
		hdr_out->x7 = hdr_test.x7;
		hdr_out->x8 = hdr_test.x8;
	} else {
		hdr_out->time.tn = 0; 
		hdr_out->time.fn = 0;
		hdr_out->data_offset = 0; 
		hdr_out->data_len = 0; 
		hdr_out->toa = 0; 
	}

	return 0;
}
