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
#include "normal.h"
#include "../dsp/dsp.h"

extern char *dbg;

#define USE_TSC			2
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
complex midambl_cvec_data[NUM_TSC][MIDAMBL_LEN];
struct cxvec midambl_cvecs[NUM_TSC];
struct vec_peak midambl_peaks[NUM_TSC];

/*
 * Correlation output
 */
static complex corr_out_data[DEF_MAXLEN];
static struct cxvec corr_out;

/*
 * Delay compensated normal burst
 */
static complex delay_data[DEF_MAXLEN];
static struct cxvec delay_brst;

/*
 * Phased corrected normal burst
 */
static complex phase_data[DEF_MAXLEN];
static struct cxvec phase_brst;

/*
 * Bit output
 */
static short demod_data[DEF_MAXLEN];
static struct rvec demod_vec;

/*
 * Correction angle
 */
static complex ang[NUM_TSC];

static int init_tsc(struct cxvec *pls, float *flt_pls)
{
	int i, n, rc;
	struct bitvec tsc_bvec;
	struct cxvec auto_corr;
	complex *data;
	int x0, x1;
	float *mod_seq;
	float sum_i, sum_q, norm, test;

#if 0
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
#endif
	/* Modulated training sequences midambles */

	mod_seq = malloc(MIDAMBL_LEN * 2 * sizeof(float));

	for (i = 0; i < NUM_TSC; i++) {
		tsc_bvec.len = MIDAMBL_LEN;
		tsc_bvec.data = &tsc_seqs[i][MIDAMBL_STRT];

		cxvec_init(&midambl_cvecs[i], MIDAMBL_LEN,
			   MIDAMBL_LEN, 0, &midambl_cvec_data[i][0]);

#if 0
		rc = gmsk_mod(&tsc_bvec, pls, &midambl_cvecs[i]);
		if (rc < 0)
			return -1;

		for (n = 0; n < MIDAMBL_LEN; n++) {
			midambl_cvecs[i].data[n].real = -midambl_cvecs[i].data[n].real;
			midambl_cvecs[i].data[n].imag = -midambl_cvecs[i].data[n].imag;
		}

		//cxvec_scale_h(&midambl_cvecs[i], SCALE_DC_GAIN);

		data = midambl_cvecs[i].data;

		x0 = 0;
		x1 = 0;
		for (n = 0; n < MIDAMBL_LEN; n ++) {
			x0 = ((int) data[n].real * (int) data[n].real) -
			     ((int) data[n].imag * (int) data[n].imag);
			x1 = 2 * ((int ) data[n].real * (int) data[n].imag);
		}
		ang[i].real = x0 >> 15;
		ang[i].imag = x1 >> 15;
#else
		flt_gmsk_mod(&tsc_bvec, flt_pls, mod_seq, 4);

#if 0
		if (i == 2) {
			memcpy(((float *) dbg) + 44 + 156 * 2, mod_seq, 16 * 2 * sizeof(float));
			//memset(((float *) dbg) + 44 + 156 * 2, 0, 16 * 2 * sizeof(float));
		}
#endif
		sum_i = 0.0;
		sum_q = 0.0;

		/* Need phase shift here!!!! */
		for (n = 0; n < MIDAMBL_LEN; n++) {
			sum_i += mod_seq[2 * n + 0] * mod_seq[2 * n + 0] -
				 mod_seq[2 * n + 1] * mod_seq[2 * n + 1];
			sum_q += 2 * mod_seq[2 * n + 0] * mod_seq[2 * n + 1];
		}

		test = (sum_i * sum_i + sum_q * sum_q);
		norm = sqrt(sum_i * sum_i + sum_q * sum_q);
		for (n = 0; n < MIDAMBL_LEN; n++) {
			mod_seq[2 * n + 0] /= -(norm * 2); 
			mod_seq[2 * n + 1] /= -(norm * 2);
		}

		/* Convert this with intrinsics */
		//ang[i].real = (short) ((sum_i / norm) * (float) (32768 / 2));
		//ang[i].imag = (short) ((sum_q / norm) * (float) (32768 / 2));

		ang[i].real = (short) ((sum_i / norm) * (float) (32768 / 2));
		ang[i].imag = 0; 

#if 0
		if (i == 2) {
			//memcpy(((float *) dbg) + 44 + 156 * 2 + 156 * 2, mod_seq, 16 * 2 * sizeof(float));
			memcpy(((float *) dbg) + 44 + 156 * 2 + 156 * 2 + 16 * 2, &sum_i, sizeof(float));
			memcpy(((float *) dbg) + 44 + 156 * 2 + 156 * 2 + 16 * 2 + 1, &sum_q, sizeof(float));
		}
#endif

		DSP_fltoq15(mod_seq, midambl_cvecs[i].data, MIDAMBL_LEN * 2);
#endif
		cxvec_rvrs(&midambl_cvecs[i]);
		midambl_cvecs[i].flags |= CXVEC_FLG_REVERSE;
	}

	free(mod_seq);
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

static void reset_norm()
{
	cxvec_init(&delay_brst, DEF_MAXLEN-64, DEF_MAXLEN, 32, delay_data);
	memset(delay_brst.buf, 0, delay_brst.buf_len * sizeof(complex));
}

static int invert_chan(complex chan, int tsc, complex *inv)
{
#if 0
	int i;
	int x0, x1;
	int y0, y1;
	int z0, z1, z2;
	complex conj;

	for (i = 0; i < 15; i++) {
		x0 = ((int) ang[USE_TSC].real) >> i;
		x1 = ((int) ang[USE_TSC].imag) >> i;

		y0 = ((int) chan.real * (int) chan.real);
		y1 = ((int) chan.imag * (int) chan.imag);
		z2 = (y0 + y1) >> 15;

		y0 = (x0 * (int) chan.real);
		y1 = (x1 * (int) chan.imag);
		z0 = (y0 + y1) >> 15;

		y0 = (x0 * (int) chan.imag);
		y1 = (x1 * (int) chan.real);
		z1 = (y1 - y0) >> 15;

		if ((z0 < z2) && (z1 < z2)) {
			y0 = (z0 << 15) / z2;
			y1 = (z1 << 15) / z2;
			break;
		}
	}

	if (i == 14) {
		conj.real = 0; 
		conj.imag = 0;
	} else {
		conj.real = (short) y0; 
		conj.imag = (short) y1;
	}

	return conj;
#else
	int i;
	int x0, x1;
	int y0, y1;
	int z0, z1, z2;

	for (i = 0; i < 15; i++) {
		x0 = ((int) chan.real) << i;
		x1 = ((int) chan.imag) << i;

		y0 = x0 * x0;
		y1 = x1 * x1;
		z2 = (y0 + y1);

		y0 = (int) ang[USE_TSC].real * x0;
		y1 = (int) ang[USE_TSC].imag * x1;
		z0 = (y0 + y1);

		y0 = (int) ang[USE_TSC].real * x1;
		y1 = (int) ang[USE_TSC].imag * x0;
		z1 = (y1 - y0);

		if ((abs(z0) < abs(z2)) && (abs(z1) < abs(z2))) {
			y0 = z0 / (z2 >> 15);
			y1 = z1 / (z2 >> 15);
			break;
		}
	}

	if (i > 0)
		i--;

	if (i == 14) {
		inv->real = 0; 
		inv->imag = 0;
		i = 0;
	} else {
		inv->real = (short) y0; 
		inv->imag = (short) y1;
	}

	return i; 
#endif
}

int detect_tsc(struct cxvec *in, int tsc, struct test_hdr *hdr)
{
	int rc;
	struct vec_peak peak;
	complex conj;

	if ((in->len > DEF_MAXLEN) ||
	    (cxvec_headrm(in) < (MIDAMBL_LEN / 2)) ||
	    (cxvec_tailrm(in) < (MIDAMBL_LEN / 2 - 1))) {
		return -1;
	}
	
	corr_out.len = in->len;
	memset(corr_out.buf, 0, corr_out.buf_len * sizeof(complex));

	rc = cxvec_correlate(in, &midambl_cvecs[tsc], &corr_out, CONV_NO_DELAY);
	if (rc < 0)
		return -1;

	rc = cxvec_peak_detect(&corr_out, &peak);
	if (rc < 0)
		return -1;

	if (((peak.gain.real * peak.gain.real) + 
	    (peak.gain.imag * peak.gain.imag)) < 800)
		return -1;

	/* Bound peak */
	//if ((peak.whole < 72) || (peak.whole > 74))
	//	return -1;

	if ((peak.whole < 72) || (peak.whole > 77))
		return -1;

	//memcpy(dbg, &peak, sizeof(struct vec_peak));
	//return 0;

	delay_brst.len = in->len;
	cxvec_advance(in, &delay_brst, peak.whole - 74, peak.frac);

	phase_brst.len = in->len;
	rc = invert_chan(peak.gain, tsc, &conj);
	if (rc < 0)
		rc = 0;

	cxvec_scale(&delay_brst, &phase_brst, conj, rc);

	demod_vec.len = 156;
	gmsk_demod(&phase_brst, NULL, &demod_vec);
	//gmsk_demod(in, NULL, &demod_vec);

	memcpy(((float *) dbg) + 44, demod_vec.data, 156 * sizeof(char));
	//DSP_q15tofl(demod_vec.data, ((float *) dbg) + 44, 156);
	//DSP_q15tofl(in->data, ((float *) dbg) + 44 + 156 * 2, 156 * 2);
	//DSP_q15tofl(phase_brst.data, ((float *) dbg) + 44 + 156 * 2, 156 * 2);
	//DSP_q15tofl(delay_brst.data, ((float *) dbg) + 44 + 156 * 2 + 156 * 2, 156 * 2);
	//DSP_q15tofl(midambl_cvecs[USE_TSC].data, ((float *) dbg) + 44 + 156 * 2 + 156 * 2, 16 * 2);
	//DSP_q15tofl(corr_out.data, ((float *) dbg) + 44 + 156, 156 * 2);

	hdr->x0 = peak.gain.real;
	hdr->x1 = peak.gain.imag;
	hdr->x2 = ang[tsc].real;
	hdr->x3 = ang[tsc].imag;
	hdr->x4 = conj.real; 
	hdr->x5 = conj.imag;
	hdr->x6 = peak.whole;
	hdr->x7 = peak.frac;
	hdr->x8 = (peak.gain.real * peak.gain.real) + (peak.gain.imag * peak.gain.imag);

	reset_norm();
	return (peak.whole - 74) * 32 + peak.frac; 
}

void init_norm(struct cxvec *pls, float *flt_pls)
{
	init_tsc(pls, flt_pls);
	cxvec_init(&corr_out, DEF_MAXLEN, DEF_MAXLEN, 0, corr_out_data);
	cxvec_init(&delay_brst, DEF_MAXLEN-64, DEF_MAXLEN, 32, delay_data);
	cxvec_init(&phase_brst, DEF_MAXLEN, DEF_MAXLEN, 0, phase_data);

	demod_vec.data = demod_data;
}
