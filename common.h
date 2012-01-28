/*
 * common.h
 *
 * DSP/BIOS Link MSGQ transport
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

#ifndef _COMMON_H_
#define _COMMON_H_

#include <msgq.h>
#include <dsplink.h>

#define APP_POOL_ID	0
#define BUFLEN		1024
#define BUFSIZE		(DSPLINK_ALIGN(BUFLEN, DSPLINK_BUF_ALIGN))
#define APPMSGSIZE	(DSPLINK_ALIGN(sizeof(struct cmsg), DSPLINK_BUF_ALIGN)) 

enum {
	GPP_INPUTMSGID = 0,
	GPP_OUTPUTMSGID,
	DSP_INPUTMSGID,
	DSP_PROCESSMSGID,
	DSP_OUTPUTMSGID,
	TERMINATEMSGID
};

struct cmsg {
	MSGQ_MsgHeader header;
	void *data;
};

enum sigproc_state {
	SIGPROC_ENTRY
};

#endif /* _COMMON_H_ */
