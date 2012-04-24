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
#include "../dsp/dsp.h"

#define RACH_SYNC_LEN		41

extern char *dbg;

/*
 * RACH sync sequence
 */
int rach_seq[RACH_SYNC_LEN] = {
	0, 1, 0, 0, 1, 0, 1, 1,
	0, 1, 1, 1, 1, 1, 1, 1,
	1, 0, 0, 1, 1, 0, 0, 1,
	1, 0, 1, 0, 1, 0, 1, 0,
	0, 0, 1, 1, 1, 1, 0, 0,
	0
};

/*
 * Modulated RACH sequence
 */
complex rach_mod_seq_data[DEF_MAXLEN];
struct cxvec rach_mod_seq;

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
 * Bit output
 */
short demod_data[DEF_MAXLEN];
struct rvec demod_vec;

int init_rach_seq(struct cxvec *pls)
{
	int rc;
	struct bitvec rach_bvec;

	rach_bvec.len = RACH_SYNC_LEN;
	rach_bvec.data = rach_seq;

	cxvec_init(&rach_mod_seq, RACH_SYNC_LEN + 3,
		   DEF_MAXLEN, 0, rach_mod_seq_data);

	rc = gmsk_mod(&rach_bvec, pls, &rach_mod_seq);
	if (rc < 0)
		return -1;

	cxvec_scale_h(&rach_mod_seq, SCALE_DC_GAIN);

	cxvec_rvrs(&rach_mod_seq);
        rach_mod_seq.flags |= CXVEC_FLG_REVERSE;

	return 0;
}

static void reset()
{
	cxvec_init(&delay_brst, DEF_MAXLEN-64, DEF_MAXLEN, 32, delay_data);
	memset(delay_brst.buf, 0, delay_brst.buf_len * sizeof(complex));
}

int detect_rach(struct cxvec *in)
{
	int rc;
	struct vec_peak peak;

	if ((in->len > DEF_MAXLEN) ||
	    (cxvec_headrm(in) < (RACH_SYNC_LEN / 2 - 1)) ||
	    (cxvec_tailrm(in) < (RACH_SYNC_LEN / 2))) {
		return -1;
	}

	corr_out.len = in->len;
	memset(corr_out.buf, 0, corr_out.buf_len * sizeof(complex));

	rc = cxvec_correlate(in, &rach_mod_seq, &corr_out, CONV_NO_DELAY);
	if (rc < 0)
		return -1;

	//DSP_q15tofl((short *) corr_out.data, (float *) dbg, 157 * 2);
	//return 0;

	rc = cxvec_peak_detect(&corr_out, &peak);
	if (rc < 0)
		return -1;

	//memcpy(dbg, &peak, sizeof(peak));
	//return 0;

	//if ((peak.whole < 28) || (peak.whole > 30))
	//	return -1;

	if ((peak.whole < 25) || (peak.whole > 32))
		return -1;

	delay_brst.len = in->len;
	//cxvec_advance(in, &delay_brst, peak.whole - 29, peak.frac);
	cxvec_advance(in, &delay_brst, peak.whole - 29, peak.frac);

	demod_vec.len = 156;
	rach_demod(&delay_brst, NULL, &demod_vec);
	//gmsk_demod(in, NULL, &demod_vec);
	//return 0;

	DSP_q15tofl(demod_vec.data, ((float *) dbg) + 44, 156);

	//DSP_q15tofl((short *) delay_brst.data, (float *) dbg, 156 * 2);
	//DSP_q15tofl((short *) rach_mod_seq.data, ((float *) dbg) + 156 * 2, 44 * 2); 

	reset();

	return (peak.whole - 32) * 256 + (peak.frac * 8);
}

void init_rach(struct cxvec *pls)
{
	init_rach_seq(pls);
	cxvec_init(&corr_out, DEF_MAXLEN, DEF_MAXLEN, 0, corr_out_data);
	cxvec_init(&delay_brst, DEF_MAXLEN-64, DEF_MAXLEN, 32, delay_data);

	demod_vec.data = demod_data;
	//DSP_q15tofl(rach_mod_seq.data, (float *) dbg, RACH_SYNC_LEN * 2);
}
