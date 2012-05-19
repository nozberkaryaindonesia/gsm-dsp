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

/*
 * Floating point complex-real convolution used for initialization.
 * Start convolve at tap number 0.
 */
int flt_conv_start(float *in, float *h, float *out, int h_len, int in_len)
{
	int i, n;

	memset(out, 0, in_len * 2 * sizeof(float));

	for (i = 0; i < in_len; i++) {
		for (n = 0; n < h_len; n++) {
			out[2 * i + 0] += in[2 * (i - n) + 0] * h[2 * n];
			out[2 * i + 1] += in[2 * (i - n) + 1] * h[2 * n];
		}
	}

	return in_len;
}

/*
 * Floating point complex-real convolution used for initialization.
 * Start convolve at center tap (number of taps / 2 - 1) where the number of
 * taps is even.
 */
int flt_conv_no_delay(float *in, float *h, float *out, int h_len, int in_len)
{
	flt_conv_start(in + (h_len / 2 - 1) * 2, h, out, h_len, in_len);

	return in_len;
}

static int cxvec_conv_no_delay(struct cxvec *restrict in,
			       struct cxvec *restrict h,
			       struct cxvec *restrict out)
{
	if ((cxvec_tailrm(in) < (h->len / 2 - 1)) ||
	    (cxvec_headrm(in) < (h->len / 2))) {
		return -1;
	}

	cxvec_clear_head(in);
	cxvec_clear_tail(in);

	DSP_fir_cplx((short *) (in->data + h->len / 2 - 1),
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

/*
 * Fixed point Q15 Complex-complex convolution
 */
int cxvec_convolve(struct cxvec *restrict in,
		   struct cxvec *restrict h,
		   struct cxvec *restrict out,
		   enum conv_type type)
{
	int rc;

	//if ((in->len % 4) || (h->len % 2) || (out->len != in->len))
	if ((in->len % 4) || (h->len % 2))
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

/*
 * Special case Q15 point interpolation
 */
int cx_conv2(complex *restrict in, struct cxvec *restrict h,
	     complex *restrict out, enum conv_type type)
{
	complex conv_out[8];

	if (h->len % 2)
		return -1;

	switch (type) {
	case CONV_NO_DELAY:
		DSP_fir_cplx((short *) (&in[h->len / 2 - 1]),
			     (short *) h->data,
			     (short *) conv_out,
			     h->len, 8);
		break;
	case CONV_START:
		DSP_fir_cplx((short *) in,
			     (short *) h->data,
			     (short *) conv_out,
			     h->len, 4);
		break;
	default:
		return -1;
	}

	out[0] = conv_out[0];
	out[1] = conv_out[1];

	return 0;
}

/*
 * Fixed point Q15 complex correlation
 * Second vector 'h' must be stored in reverse
 */
int cxvec_correlate(struct cxvec *restrict in,
		    struct cxvec *restrict h,
		    struct cxvec *restrict out,
		    enum conv_type type)
{
	if (!(h->flags & CXVEC_FLG_REVERSE))
		return -1;

	return cxvec_convolve(in, h, out, type);
}
