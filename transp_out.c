#include <std.h>
#include <log.h>
#include <tsk.h>
#include <sem.h>
#include <mem.h>
#include <sio.h>
#include <sys.h>
#include <msgq.h>
#include <bcache.h>

#include "common.h"

#define MEMALIGN 128
#define NUM_BUFS 4

static MSGQ_Queue msgq_out;
extern LOG_Obj trace;

void init_output_thrd()
{
	int status;
	MSGQ_Attrs attrs;
	SEM_Handle output_sem_hdl;

	output_sem_hdl = SEM_create(0, NULL);
	
	if (output_sem_hdl == NULL) {
		SYS_abort("Semaphore creation failed");
	}

	attrs = MSGQ_ATTRS;
	attrs.notifyHandle = (Ptr) output_sem_hdl;
	attrs.pend = (MSGQ_Pend) SEM_pendBinary;
	attrs.post = (MSGQ_Post) SEM_postBinary;

	status = MSGQ_open("outputDSP", &msgq_out, &attrs);
	if (status != SYS_OK) {
		SYS_abort("Failed to open the DSP output message queue");
	}
}

void output_thrd()
{
	struct link_msg *msg, *msg_gpp, *msg_proc;
	int i, status, msg_id, msg_cnt;
	MSGQ_Queue msgq_proc_dsp;
	MSGQ_Queue msgq_gpp_in;
	MSGQ_LocateAttrs attrs;

	status = MSGQ_locate("processDSP", &msgq_proc_dsp, NULL);

	if (status != SYS_OK) {
		SYS_abort("Failed to locate the reply message queue");
	}

	status = SYS_ENOTFOUND;
	while ((status == SYS_ENOTFOUND) || (status == SYS_ENODEV)) {
		attrs.timeout = SYS_FOREVER;
		status = MSGQ_locate("inputGPP", &msgq_gpp_in, &attrs);
		if ((status == SYS_ENOTFOUND) || (status == SYS_ENODEV)) {
			TSK_sleep(1000);
		}
		else if (status != SYS_OK) {
			SYS_abort("Failed to locate the inputGPP message queue");
		}
	}

	for (i = 0; i < 1; i++) {
		status = MSGQ_alloc(0, (MSGQ_Msg *) &msg, APPMSGSIZE);
		if (status != SYS_OK) {
			SYS_abort("Failed to allocate a message");
		}

		MSGQ_setMsgId((MSGQ_Msg) msg, DSP_OUTPUTMSGID);
		msg->data = NULL;

		status = MSGQ_put(msgq_proc_dsp, (MSGQ_Msg) msg);
		if (status != SYS_OK) {
			SYS_abort("Failed to send a message");
		}
	}

	msg_proc = NULL;
	msg_gpp = NULL;
	msg_cnt = 0;

	for (;;) {
		status = MSGQ_get(msgq_out, (MSGQ_Msg *)&msg, SYS_FOREVER);
		if (status != SYS_OK) {
			SYS_abort("Failed to get a message");
		}

		msg_id = MSGQ_getMsgId((MSGQ_Msg)msg);
		switch (msg_id) {
		case GPP_INPUTMSGID:
			msg_gpp = msg;
			break;
		case DSP_PROCESSMSGID:
			msg_proc = msg;
			break;
		default:
			SYS_abort("Unknown message received");
			break;
		}

		if ((msg_proc != NULL) && (msg_gpp != NULL)) {
			MSGQ_setMsgId((MSGQ_Msg) msg_gpp, DSP_OUTPUTMSGID);
		
			status = MSGQ_put(msgq_proc_dsp, (MSGQ_Msg) msg_gpp);
			if (status != SYS_OK) {
				SYS_abort("Failed to send a message");
			}

			if (msg_cnt > 0) {
				BCACHE_wb(msg_proc->data, BUFSIZE, TRUE);
			}
			msg_cnt++;

			MSGQ_setMsgId((MSGQ_Msg) msg_proc, DSP_OUTPUTMSGID);

			status = MSGQ_put(msgq_gpp_in, (MSGQ_Msg) msg_proc);
			if (status != SYS_OK) {
				SYS_abort("Failed to send a message");
			}

			msg_proc = NULL;
			msg_gpp = NULL;
		}
	}
}
