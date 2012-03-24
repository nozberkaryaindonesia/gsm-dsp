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
#include "sigvec.h"
#include "convolve.h"

extern char *dbg;

static int cxvec_conv_no_delay(struct cxvec *restrict in,
			       struct cxvec *restrict h,
			       struct cxvec *restrict out)
{
	if ((cxvec_tailrm(in) < (h->len / 2)) ||
	    (cxvec_headrm(in) < (h->len / 2 - 1))) {
		return -1;
	}

	cxvec_clear_head(in);
	cxvec_clear_tail(in);

	/* FIXME: Mysterious 2 sample offset that we need to correct */
	DSP_fir_cplx((short *) in->data + h->len + 4,
		     (short *) h->data,
		     (short *) out->data,
		     h->len, in->len);

	return 0;
}

static int cxvec_conv_start(struct cxvec *restrict in,
			    struct cxvec *restrict h,
			    struct cxvec *restrict out)
{
	if (cxvec_headrm(in) < (h->len - 1))
		return -1;

	cxvec_clear_head(in);

	DSP_fir_cplx((short *) in->data,
		     (short *) h->data,
		     (short *) out->data,
		     h->len, in->len);
	return 0;
}

int cxvec_convolve(struct cxvec *restrict in,
		   struct cxvec *restrict h,
		   struct cxvec *restrict out,
		   enum conv_type type)
{
	int rc;

	if ((in->len % 4) || (h->len % 2) || (out->len != in->len))
		return -1;

	switch (type) {
	case CONV_NO_DELAY:
		rc = cxvec_conv_no_delay(in, h, out);
		break;
	case CONV_START:
		rc = cxvec_conv_start(in, h, out);
		break;
	default:
		rc = -1;
	}

	return rc;
}

static int rvec_conv_no_delay(struct rvec *restrict in,
		       struct rvec *restrict h,
		       struct rvec *restrict out)
{
	if ((rvec_tailrm(in) < (h->len / 2)) ||
	    (rvec_headrm(in) < (h->len / 2 - 1))) {
		return -1;
	}

	rvec_clear_head(in);
	rvec_clear_tail(in);

	DSP_fir_r4(in->data - h->len / 2,
		   h->data,
		   out->data,
		   h->len, in->len);
	return 0;
}

static int rvec_conv_start(struct rvec *restrict in,
			   struct rvec *restrict h,
			   struct rvec *restrict out)
{
	if (rvec_headrm(in) < (h->len - 1))
		return -1;

	rvec_clear_head(in);

	DSP_fir_r4((short *) in->data,
		   (short *) h->data,
		   (short *) out->data,
		   h->len, in->len);
	return 0;
}

/*
 * Taps multiple of 4 and >= 8. Data multiples of 4.
 */
int rvec_convolve(struct rvec *restrict in,
		  struct rvec *restrict h,
		  struct rvec *restrict out,
		  enum conv_type type)
{
	int rc;

	if ((in->len % 4) || (h->len % 4) ||
	    (h->len < 8) || (out->len != in->len)) {
		return -1;
	}

	switch (type) {
	case CONV_NO_DELAY:
		rc = rvec_conv_no_delay(in, h, out);
		break;
	case CONV_START:
		rc = rvec_conv_start(in, h, out);
		break;
	default:
		rc = -1;
	}

	return rc;
}

/*
 * Special case for point interpolation
 */
int real_convolve2(short *restrict in,
		   struct rvec *restrict h,
		   short *restrict out,
		   enum conv_type type)
{
	short conv_out[4];

	if ((h->len % 4) || (h->len < 8))
		return -1;

	switch (type) {
	case CONV_NO_DELAY:
		DSP_fir_r4(in - (h->len / 2), h->data, &conv_out, h->len, 4);
		break;
	case CONV_START:
		DSP_fir_r4(in, h->data, &conv_out, h->len, 4);
		break;
	default:
		return -1;
	}

	out[0] = conv_out[0];
	out[1] = conv_out[1];

	return 0;
}

int cxvec_correlate(struct cxvec *restrict in,
		    struct cxvec *restrict h,
		    struct cxvec *restrict out,
		    enum conv_type type)
{
	if (!(h->flags & CXVEC_FLG_REVERSE))
		return -1;

	return cxvec_convolve(in, h, out, type);
}

