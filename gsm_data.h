/*
 * dsp.c
 *
 * TI c64x+ GSM signal processing
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

#ifndef _GSM_DATA_H_
#define _GSM_DATA_H_

#include "sigvec.h"

#ifndef NULL
#  define NULL          0
#endif

#define DEF_BUFLEN		256

/*
 * GSM Pulse filter
 */
#define GSM_PLS_LEN		4
complex gsm_pls_data[GSM_PLS_LEN];
struct cxvec gsm_pls;

/*
 * Training sequences and midambles
 */
#define NUM_TSC			8
#define TSC_LEN			26
const int tsc_seqs[NUM_TSC][TSC_LEN] = {
        { 0,0,1,0,0,1,0,1,1,1,0,0,0,0,1,0,0,0,1,0,0,1,0,1,1,1 },
        { 0,0,1,0,1,1,0,1,1,1,0,1,1,1,1,0,0,0,1,0,1,1,0,1,1,1 },
        { 0,1,0,0,0,0,1,1,1,0,1,1,1,0,1,0,0,1,0,0,0,0,1,1,1,0 },
        { 0,1,0,0,0,1,1,1,1,0,1,1,0,1,0,0,0,1,0,0,0,1,1,1,1,0 },
        { 0,0,0,1,1,0,1,0,1,1,1,0,0,1,0,0,0,0,0,1,1,0,1,0,1,1 },
        { 0,1,0,0,1,1,1,0,1,0,1,1,0,0,0,0,0,1,0,0,1,1,1,0,1,0 },
        { 1,0,1,0,0,1,1,1,1,1,0,1,1,0,0,0,1,0,1,0,0,1,1,1,1,1 },
        { 1,1,1,0,1,1,1,1,0,0,0,1,0,0,1,0,1,1,1,0,1,1,1,1,0,0 },
};

complex tsc_cvec_data[NUM_TSC][DEF_BUFLEN];
struct cxvec tsc_cvecs[NUM_TSC];

#define MIDAMBL_STRT		5
#define MIDAMBL_LEN             16
complex midambl_cvec_data[NUM_TSC][DEF_BUFLEN];
struct cxvec midambl_cvecs[NUM_TSC];
struct vec_peak midambl_peaks[NUM_TSC];

/*
 * RACH sync sequence
 */
#define RACH_SYNC_LEN		41
const int rach_seq[RACH_SYNC_LEN] = {
	0,1,0,0,1,0,1,1,0,1,1,1,1,1,1,1,
	1,0,0,1,1,0,0,1,1,0,1,0,1,0,1,0,
	0,0,1,1,1,1,0,0,0
};

complex rach_cvec_data[DEF_BUFLEN];
struct cxvec rach_cvec;

#endif /* _DSP_DATA_H_ */
