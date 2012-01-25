/*
 * filter.h
 *
 * TI c64x+ GSM signal processing filters
 *
 * Copyright (C) 2012  Thomas Tsou <ttsou@vt.edu>
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

/*
 * Put common vectors in the global store.
 * There should only be single global copies so do not define statically.
 * If this header is included twice something is wrong.
 * Find a better way that doesn't clutter...
 */

#ifndef _DSP_FILTER_H_
#define _DSP_FILTER_H_

#include "sigvec.h"

#define DEF_BUFLEN			256
#define GSMRATE				(1625e3 / 6)

/* 
 * Fractional delay filterbank
 * Complex input with real taps
 */
#define DELAY_FILT_M                    16
#define DELAY_FILT_WIDTH                96
#define DELAY_FILT_RATE                 (DELAY_FILT_M * GSMRATE)
#define DELAY_FILT_MIN                  4
complex delay_filt_data[DELAY_FILT_M][DELAY_FILT_WIDTH];
struct cxvec delay_filt[DELAY_FILT_M];

/*
 * Interpolating filterbank
 * Real input with multiple of multiple of 8 taps
 */
#define INTERP_FILT_M                   DELAY_FILT_M
#define INTERP_FILT_WIDTH               64
#define INTERP_FILT_RATE                (INTERP_FILT_M * GSMRATE)
#define INTERP_FILT_MIN                 4
short interp_filt_data[INTERP_FILT_M][INTERP_FILT_WIDTH];
struct rvec interp_filt[INTERP_FILT_M];

/*
 * 1/4 sample rate frequency shift vector
 */
#define FREQ_SHFT_LEN			DEF_BUFLEN
complex freq_shft_up_data[FREQ_SHFT_LEN];
complex freq_shft_dn_data[FREQ_SHFT_LEN];
struct cxvec freq_shft_up;
struct cxvec freq_shft_dn;

#endif /* _DSP_FILTER_H_ */
