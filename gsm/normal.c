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

#define MIDAMBL_STRT		5
#define MIDAMBL_LEN             16
#define NORM_CORR_PEAK		74

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

static complex midambl_cvec_data[NUM_TSC][MIDAMBL_LEN];
static struct cxvec midambl_cvecs[NUM_TSC];

/*
 * Correlation output
 */
static complex corr_out_data[DEF_MAXLEN];
static struct cxvec corr_out;

/*
 * Delay compensated burst
 */
static complex delay_data[DEF_MAXLEN];
static struct cxvec delay_brst;

/*
 * Phased corrected burst
 */
static complex phase_data[DEF_MAXLEN];
static struct cxvec phase_brst;

/*
 * Demodulator soft bit output 
 */
static short demod_data[DEF_MAXLEN];
static struct rvec demod_vec;

/*
 * Phsae correction values
 */
static complex ang[NUM_TSC];

/*
 * Initialize TSC correlation sequences
 */
static int init_tsc(float *flt_pls)
{
	int i, n;
	struct bitvec tsc_bvec;
	float *mod_seq;
	float sum_i, sum_q, norm;

	mod_seq = malloc(MIDAMBL_LEN * 2 * sizeof(float));

	for (i = 0; i < NUM_TSC; i++) {
		tsc_bvec.len = MIDAMBL_LEN;
		tsc_bvec.data = &tsc_seqs[i][MIDAMBL_STRT];

		cxvec_init(&midambl_cvecs[i], MIDAMBL_LEN,
			   MIDAMBL_LEN, 0, &midambl_cvec_data[i][0]);

		flt_gmsk_mod(&tsc_bvec, flt_pls, mod_seq, 4);

		/*
		 * Calculate dot product (autocorrelation peak)
		 */
		sum_i = 0.0;
		sum_q = 0.0;

		for (n = 0; n < MIDAMBL_LEN; n++) {
			sum_i += mod_seq[2 * n + 0] * mod_seq[2 * n + 0] -
				 mod_seq[2 * n + 1] * mod_seq[2 * n + 1];
			sum_q += 2 * mod_seq[2 * n + 0] * mod_seq[2 * n + 1];
		}

		/*
		 * Rotate ans scale sequence. The TSC midamble starts 66 symbols
		 * into the burst, which creates an 180 degree phase shift. Add
		 * shift at the same time as we scale.
		 */ 
		norm = sqrt(sum_i * sum_i + sum_q * sum_q);

		for (n = 0; n < MIDAMBL_LEN; n++) {
			mod_seq[2 * n + 0] /= -(norm * 2); 
			mod_seq[2 * n + 1] /= -(norm * 2);
		}

		/*
		 * Hack: Testing shows an 90 degree phase offset from the
		 * calculated centre. Compensate with an extra rotation that
		 * roughly shifts back to unity. This hack may only apply to
		 * TSC 2.
		 */
		ang[i].real = (short) ((sum_i / norm) * (float) (32768 / 2));
		ang[i].imag = 0; 

		/*
		 * Convert to Q15 and reverse sequence
		 */
		DSP_fltoq15(mod_seq, midambl_cvecs[i].data, MIDAMBL_LEN * 2);
		cxvec_rvrs(&midambl_cvecs[i]);
		midambl_cvecs[i].flags |= CXVEC_FLG_REVERSE;
	}

	free(mod_seq);

	return 0;
}

static void reset_norm()
{
	cxvec_init(&delay_brst, DEF_MAXLEN-64, DEF_MAXLEN, 32, delay_data);
	memset(delay_brst.buf, 0, delay_brst.buf_len * sizeof(complex));
}

/*
 * Invert a complex channel value (glorified divide in Q15). Manage scaling
 * with shifts to make sure that the divide is possible.
 */
static int invert_chan(complex chan, int tsc, complex *inv)
{
	int i;
	int x0, x1;
	int y0, y1;
	int z0, z1, z2;

	for (i = 0; i < 14; i++) {
		x0 = ((int) chan.real) << i;
		x1 = ((int) chan.imag) << i;

		y0 = x0 * x0;
		y1 = x1 * x1;
		z2 = (y0 + y1);

		y0 = (int) ang[tsc].real * x0;
		y1 = (int) ang[tsc].imag * x1;
		z0 = (y0 + y1);

		y0 = (int) ang[tsc].real * x1;
		y1 = (int) ang[tsc].imag * x0;
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
}

/*
 * Detect and decode a normal burst.
 *
 * The timing correction procedures are TSC correlation, interpolating peak
 * detection, and fractional delay compensation on the input burst. The sequence
 * is then phase corrected, scaled, and sent to the GMSK demodulator.
 */
int detect_tsc(struct cxvec *in, struct test_hdr *hdr)
{
	int rc;
	int tsc = USE_TSC;
	struct vec_peak peak;
	complex conj;

	/* Check input vector validity */
	if ((in->len > DEF_MAXLEN) ||
	    (cxvec_headrm(in) < (MIDAMBL_LEN / 2)) ||
	    (cxvec_tailrm(in) < (MIDAMBL_LEN / 2 - 1))) {
		return -1;
	}
	
	corr_out.len = in->len;
	delay_brst.len = in->len;
	phase_brst.len = in->len;
	demod_vec.len = in->len;
	memset(corr_out.buf, 0, corr_out.buf_len * sizeof(complex));

	rc = cxvec_correlate(in, &midambl_cvecs[tsc], &corr_out, CONV_NO_DELAY);
	if (rc < 0)
		return -1;

	rc = cxvec_peak_detect(&corr_out, &peak);
	if (rc < 0)
		return -1;

	/* Bound peak and gain to sane values */
	if (((peak.gain.real * peak.gain.real) + 
	    (peak.gain.imag * peak.gain.imag)) < 800)
		return -1;

	if ((peak.whole < 72) || (peak.whole > 77))
		return -1;

	/* Delay and phase offset compensation */
	cxvec_advance(in, &delay_brst, peak.whole - NORM_CORR_PEAK, peak.frac);
	invert_chan(peak.gain, tsc, &conj);
	cxvec_scale(&delay_brst, &phase_brst, conj, rc);

	/* Rotate and slice in GMSK demod */
	gmsk_demod(&phase_brst, NULL, &demod_vec);

	/* Hack: Output demodulated sequence through the debug path */
	//memcpy(((float *) dbg) + 44, demod_vec.data, 156 * sizeof(char));
	DSP_q15tofl(demod_vec.data, ((float *) dbg) + 44, 156);


	reset_norm();
	return (peak.whole - NORM_CORR_PEAK) * 32 + peak.frac; 
}

void init_norm(float *flt_pls)
{
	init_tsc(flt_pls);
	cxvec_init(&corr_out, DEF_MAXLEN, DEF_MAXLEN, 0, corr_out_data);
	cxvec_init(&delay_brst, DEF_MAXLEN-64, DEF_MAXLEN, 32, delay_data);
	cxvec_init(&phase_brst, DEF_MAXLEN, DEF_MAXLEN, 0, phase_data);

	demod_vec.data = demod_data;
}
