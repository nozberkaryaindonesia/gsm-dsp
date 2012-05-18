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
complex rach_mod_seq_data[DEF_MAXLEN];
struct cxvec rach_mod_seq;

complex rach_mod_seq_data1[DEF_MAXLEN];
struct cxvec rach_mod_seq1;

complex rach_mod_seq_data2[DEF_MAXLEN];
struct cxvec rach_mod_seq2;

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

int init_rach_seq(struct cxvec *pls, float *flt_pls)
{
	int i, rc;
	struct bitvec rach_bvec;
	complex *data;
	int x0, x1;
	float *mod_seq;
	float sum_i, sum_q, norm, test;

	rach_bvec.len = RACH_SYNC_LEN;
	rach_bvec.data = rach_seq;

	cxvec_init(&rach_mod_seq, RACH_SYNC_LEN,
		   DEF_MAXLEN, 0, rach_mod_seq_data);
#if 0

	rc = gmsk_mod(&rach_bvec, pls, &rach_mod_seq);
	if (rc < 0)
		return -1;

	rach_mod_seq.data[41].real = 0;
	rach_mod_seq.data[41].imag = 0;
	rach_mod_seq.data[42].real = 0;
	rach_mod_seq.data[42].imag = 0;
	rach_mod_seq.data[43].real = 0;
	rach_mod_seq.data[43].imag = 0;

	//cxvec_scale_h(&rach_mod_seq, SCALE_DC_GAIN);

	data = rach_mod_seq.data;

	x0 = 0;
	x1 = 0;
	for (i = 0; i < (RACH_SYNC_LEN); i ++) {
		x0 += ((int) data[i].real * (int) data[i].real) -
		      ((int) data[i].imag * (int) data[i].imag);
		x1 += 2 * ((int) data[i].real * (int) data[i].imag);
	}

	ang.real = x0 >> 15;
	ang.imag = x1 >> 15;
#else
	mod_seq = malloc(RACH_SYNC_LEN * 2 * sizeof(float));
	flt_gmsk_mod(&rach_bvec, flt_pls, mod_seq, 4);

	mod_seq[2 * 41 + 0] = 0.0;
	mod_seq[2 * 41 + 1] = 0.0;
	mod_seq[2 * 42 + 0] = 0.0;
	mod_seq[2 * 42 + 1] = 0.0;
	mod_seq[2 * 43 + 0] = 0.0;
	mod_seq[2 * 43 + 1] = 0.0;

	//memcpy(((float *) dbg) + 44 + 156, mod_seq, 44 * 2 * sizeof(float));

	sum_i = 0.0;
	sum_q = 0.0;

	for (i = 0; i < RACH_SYNC_LEN; i++) {
		sum_i += mod_seq[2 * i + 0] * mod_seq[2 * i + 0] - 
			 mod_seq[2 * i + 1] * mod_seq[2 * i + 1];
		sum_q += 2 * mod_seq[2 * i + 0] * mod_seq[2 * i + 1];
	}

	//test = (sum_i * sum_i + sum_q * sum_q);
	norm = sqrt(sum_i * sum_i + sum_q * sum_q);
	for (i = 0; i < RACH_SYNC_LEN; i++) {
		mod_seq[2 * i + 0] /= norm;
		mod_seq[2 * i + 1] /= norm;
	}

	/* Convert this with intrinsics */
	ang.real = (short) ((sum_i / norm) * (float) (32768 / 2));
	ang.imag = (short) ((sum_q / norm) * (float) (32768 / 2));

	//memcpy(((float *) dbg) + 44 + 156 + 156 * 2, mod_seq, 44 * 2 * sizeof(float));
	//memcpy(((float *) dbg) + 44 + 156 + 156 * 2 + 44 * 2, &norm, sizeof(float));
	//memcpy(((float *) dbg) + 44 + 156 + 156 * 2 + 44 * 2 + 1, &test, sizeof(float));

	DSP_fltoq15(mod_seq, rach_mod_seq.data, RACH_SYNC_LEN * 2);
	free(mod_seq);
#endif
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
#if 0
	int i;
	int x0, x1;
	int y0, y1;
	int z0, z1, z2;

	for (i = 0; i < 15; i++) {
		x0 = ((int) ang.real) >> i;
		x1 = ((int) ang.imag) >> i;

		y0 = ((int) chan.real * (int) chan.real);
		y1 = ((int) chan.imag * (int) chan.imag);
		z2 = (y0 + y1);

		y0 = (x0 * (int) chan.real);
		y1 = (x1 * (int) chan.imag);
		z0 = (y0 + y1);

		y0 = (x0 * (int) chan.imag);
		y1 = (x1 * (int) chan.real);
		z1 = (y1 - y0);

		if ((abs(z0) < abs(z2)) && (abs(z1) < abs(z2))) {
			y0 = (z0) / (z2 >> 15);
			y1 = (z1) / (z2 >> 15);
			break;
		}
	}

	if (i == 14) {
		inv->real = 0; 
		inv->imag = 0;
	} else {
		inv->real = (short) y0; 
		inv->imag = (short) y1;
	}

	return i;
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
#endif
}

int detect_rach(struct cxvec *in, struct test_hdr *hdr)
{
	int rc;
	struct vec_peak peak;
	complex conj;

	if ((in->len > DEF_MAXLEN) ||
	    (cxvec_headrm(in) < (RACH_SYNC_LEN / 2)) ||
	    (cxvec_tailrm(in) < (RACH_SYNC_LEN / 2 - 1))) {
		return -1;
	}

	corr_out.len = in->len;
	memset(corr_out.buf, 0, corr_out.buf_len * sizeof(complex));

	rc = cxvec_correlate(in, &rach_mod_seq, &corr_out, CONV_NO_DELAY);
	if (rc < 0)
		return -1;

	//DSP_q15tofl((short *) in->data, (float *) dbg + 44, 157 * 2);
	//return 0;

	//DSP_q15tofl(rach_mod_seq.data, ((float *) dbg) + 44 + 156, 44 * 2);

	rc = cxvec_peak_detect(&corr_out, &peak);
	if (rc < 0)
		return -1;

	if (((peak.gain.real * peak.gain.real) + 
	    (peak.gain.imag * peak.gain.imag)) < 400)
		return -1;

	if ((peak.whole < 28) || (peak.whole > 35))
		return -1;

	delay_brst.len = in->len;
	phase_brst.len = in->len;

	cxvec_advance(in, &delay_brst, peak.whole - 30, peak.frac);

	rc = invert_chan(peak.gain, &conj);

	cxvec_scale(&delay_brst, &phase_brst, conj, rc);

	demod_vec.len = 156;
	rach_demod(&phase_brst, NULL, &demod_vec);
	//gmsk_demod(in, NULL, &demod_vec);
	//return 0;

	memcpy(((float *) dbg) + 44, demod_vec.data, 156 * sizeof(char));
	//DSP_q15tofl(demod_vec.data, ((float *) dbg) + 44, 156);
	//DSP_q15tofl(in->data, ((float *) dbg) + 44 + 156, 156 * 2);
	//DSP_q15tofl(corr_out.data, ((float *) dbg) + 44 + 156 + 156 * 2, 156 * 2);
	//DSP_q15tofl(phase_brst.data, ((float *) dbg) + 44 + 156 * 2, 156 * 2);
	//DSP_q15tofl(delay_brst.data, ((float *) dbg) + 44 + 156 + 156 * 2, 156 * 2);
	//DSP_q15tofl(rach_mod_seq.data, ((float *) dbg) + 44 + 156 * 2 + 156 * 2, 44 * 2);

	hdr->x0 = peak.gain.real;
	hdr->x1 = peak.gain.imag;
	hdr->x2 = ang.real;
	hdr->x3 = ang.imag;
	hdr->x4 = conj.real; 
	hdr->x5 = conj.imag;
	hdr->x6 = peak.whole;
	hdr->x7 = peak.frac;
	hdr->x8 = rc;

	//DSP_q15tofl((short *) delay_brst.data, (float *) dbg, 156 * 2);
	//DSP_q15tofl((short *) rach_mod_seq.data, ((float *) dbg) + 156 * 2, 44 * 2); 

	reset();

	return (peak.whole - 30) * 32 + peak.frac;
	//return peak.orig;
}

void init_rach(struct cxvec *pls, float *flt_pls)
{
	init_rach_seq(pls, flt_pls);
	cxvec_init(&corr_out, DEF_MAXLEN, DEF_MAXLEN, 0, corr_out_data);
	cxvec_init(&delay_brst, DEF_MAXLEN-64, DEF_MAXLEN, 32, delay_data);
	cxvec_init(&phase_brst, DEF_MAXLEN, DEF_MAXLEN, 0, phase_data);

	demod_vec.data = demod_data;
	//DSP_q15tofl(rach_mod_seq.data, (float *) dbg, RACH_SYNC_LEN * 2);
}
