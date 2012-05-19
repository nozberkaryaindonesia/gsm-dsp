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

#include "rach.h"
#include <stdlib.h>
#include <math.h>
#include "../dsp/dsp.h"

#define RACH_SYNC_LEN		(41 + 3)
#define RACH_CORR_PEAK		30
#define RACH_CORR_THRHLD	400

extern char *dbg;

/*
 * RACH sync sequence
 */
int rach_seq[RACH_SYNC_LEN] = {
	0,1,0,0,1,0,1,1,0,1,1,1,1,1,1,1,1,0,0,1,1,0,0,1,1,0,1,0,1,0,1,0,0,0,1,1,1,1,0,0,0
};

/*
 * Modulated RACH sequence
 */
static complex rach_mod_seq_data[DEF_MAXLEN];
static struct cxvec rach_mod_seq;

/*
 * RACH correlation output
 */
static complex corr_out_data[DEF_MAXLEN];
static struct cxvec corr_out;

/*
 * Delay compensated RACH burst
 */
static complex delay_data[DEF_MAXLEN];
static struct cxvec delay_brst;

/*
 * Phased corrected RACH burst
 */
static complex phase_data[DEF_MAXLEN];
static struct cxvec phase_brst;

/*
 * Bit output
 */
short demod_data[DEF_MAXLEN];
struct rvec demod_vec;

/*
 * Master angle
 */
static complex ang;

int init_rach_seq(float *flt_pls)
{
	int i;
	struct bitvec rach_bvec;
	float *mod_seq;
	float sum_i, sum_q, norm;

	rach_bvec.len = RACH_SYNC_LEN;
	rach_bvec.data = rach_seq;

	cxvec_init(&rach_mod_seq, RACH_SYNC_LEN,
		   DEF_MAXLEN, 0, rach_mod_seq_data);
	mod_seq = malloc(RACH_SYNC_LEN * 2 * sizeof(float));
	flt_gmsk_mod(&rach_bvec, flt_pls, mod_seq, 4);

	mod_seq[2 * 41 + 0] = 0.0;
	mod_seq[2 * 41 + 1] = 0.0;
	mod_seq[2 * 42 + 0] = 0.0;
	mod_seq[2 * 42 + 1] = 0.0;
	mod_seq[2 * 43 + 0] = 0.0;
	mod_seq[2 * 43 + 1] = 0.0;

	sum_i = 0.0;
	sum_q = 0.0;

	for (i = 0; i < RACH_SYNC_LEN; i++) {
		sum_i += mod_seq[2 * i + 0] * mod_seq[2 * i + 0] - 
			 mod_seq[2 * i + 1] * mod_seq[2 * i + 1];
		sum_q += 2 * mod_seq[2 * i + 0] * mod_seq[2 * i + 1];
	}

	norm = sqrt(sum_i * sum_i + sum_q * sum_q);
	for (i = 0; i < RACH_SYNC_LEN; i++) {
		mod_seq[2 * i + 0] /= norm;
		mod_seq[2 * i + 1] /= norm;
	}

	/* Convert this with intrinsics */
	ang.real = (short) ((sum_i / norm) * (float) (32768 / 2));
	ang.imag = (short) ((sum_q / norm) * (float) (32768 / 2));

	DSP_fltoq15(mod_seq, rach_mod_seq.data, RACH_SYNC_LEN * 2);
	free(mod_seq);

	cxvec_rvrs(&rach_mod_seq);
        rach_mod_seq.flags |= CXVEC_FLG_REVERSE;

	return 0;
}

static void reset()
{
	cxvec_init(&delay_brst, DEF_MAXLEN-64, DEF_MAXLEN, 32, delay_data);
	memset(delay_brst.buf, 0, delay_brst.buf_len * sizeof(complex));
}

static int invert_chan(complex chan, complex *inv)
{
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

		y0 = (int) ang.real * x0;
		y1 = (int) ang.imag * x1;
		z0 = (y0 + y1);

		y0 = (int) ang.real * x1;
		y1 = (int) ang.imag * x0;
		z1 = (y1 - y0);

		if ((abs(z0) < abs(z2)) && (abs(z1) < abs(z2))) {
			y0 = (z0) / (z2 >> 15);
			y1 = (z1) / (z2 >> 15);
			break;
		}
	}

	/* We halved the norm for headroom which creates a single shift */
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
}

int detect_rach(struct cxvec *in, struct test_hdr *hdr)
{
	int rc;
	struct vec_peak peak;
	complex conj;

	/* Check input vector validity */
	if ((in->len > DEF_MAXLEN) ||
	    (cxvec_headrm(in) < (RACH_SYNC_LEN / 2)) ||
	    (cxvec_tailrm(in) < (RACH_SYNC_LEN / 2 - 1))) {
		return -1;
	}

	corr_out.len = in->len;
	delay_brst.len = in->len;
	phase_brst.len = in->len;
	demod_vec.len = in->len;
	memset(corr_out.buf, 0, corr_out.buf_len * sizeof(complex));

	rc = cxvec_correlate(in, &rach_mod_seq, &corr_out, CONV_NO_DELAY);
	if (rc < 0)
		return -1;

	rc = cxvec_peak_detect(&corr_out, &peak);
	if (rc < 0)
		return -1;

	/* Bound peak and gain to sane values */
	if (((peak.gain.real * peak.gain.real) + 
	    (peak.gain.imag * peak.gain.imag)) < RACH_CORR_THRHLD)
		return -1;

	if ((peak.whole < (RACH_CORR_PEAK - 2)) ||
	    (peak.whole > (RACH_CORR_PEAK + 4)))
		return -1;

	/* Delay and phase offset compensation */
	cxvec_advance(in, &delay_brst, peak.whole - RACH_CORR_PEAK, peak.frac);
	invert_chan(peak.gain, &conj);
	cxvec_scale(&delay_brst, &phase_brst, conj, rc);

	/* Rotate and slice in GMSK demod */
	rach_demod(&phase_brst, NULL, &demod_vec);

	/* Hack: Output demodulated sequence through the debug path */
	//memcpy(((float *) dbg) + 44, demod_vec.data, 156 * sizeof(char));
	DSP_q15tofl(demod_vec.data, ((float *) dbg) + 44, 156);

	reset();

	return (peak.whole - RACH_CORR_PEAK) * 32 + peak.frac;
}

void init_rach(float *flt_pls)
{
	init_rach_seq(flt_pls);
	cxvec_init(&corr_out, DEF_MAXLEN, DEF_MAXLEN, 0, corr_out_data);
	cxvec_init(&delay_brst, DEF_MAXLEN-64, DEF_MAXLEN, 32, delay_data);
	cxvec_init(&phase_brst, DEF_MAXLEN, DEF_MAXLEN, 0, phase_data);

	demod_vec.data = demod_data;
}
