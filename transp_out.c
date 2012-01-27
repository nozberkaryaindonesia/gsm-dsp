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
	MSGQ_Attrs msgq_attrs;
	SEM_Handle ouput_sem_hdl;

	ouput_sem_hdl = SEM_create(0, NULL);
	
	if (ouput_sem_hdl == NULL) {
		SYS_abort("Semaphore creation failed");
	}

	msgq_attrs = MSGQ_ATTRS;
	msgq_attrs.notifyHandle = (Ptr) ouput_sem_hdl;
	msgq_attrs.pend = (MSGQ_Pend) SEM_pendBinary;
	msgq_attrs.post = (MSGQ_Post) SEM_postBinary;

	status = MSGQ_open("outputDSP", &msgq_out, &msgq_attrs);
	if (status != SYS_OK) {
		SYS_abort("Failed to open the DSP output message queue");
	}
}

void output_thrd()
{
	struct cmsg *msg, *msg_gpp, *msg_proc;
	int status, msg_id;
	MSGQ_Queue msgq_reply;
	MSGQ_Queue msgq_gpp;
	MSGQ_LocateAttrs sync_locate_attrs;

	status = MSGQ_locate("processDSP", &msgq_reply, NULL);

	if (status != SYS_OK) {
		SYS_abort("Failed to locate the reply message queue");
	}

	status = SYS_ENOTFOUND;
	while ((status == SYS_ENOTFOUND) || (status == SYS_ENODEV)) {
		sync_locate_attrs.timeout = SYS_FOREVER;
		status = MSGQ_locate("inputGPP", &msgq_gpp, &sync_locate_attrs);
		if ((status == SYS_ENOTFOUND) || (status == SYS_ENODEV)) {
			TSK_sleep(1000);
		}
		else if (status != SYS_OK) {
			SYS_abort("Failed to locate the inputGPP message queue");
		}
	}

	status = MSGQ_alloc(0, (MSGQ_Msg *) &msg, APPMSGSIZE);
	if (status != SYS_OK) {
		SYS_abort("Failed to allocate a message");
	}

	MSGQ_setMsgId((MSGQ_Msg) msg, DSP_OUTPUTMSGID);

	msg->data = NULL;

	status = MSGQ_put(msgq_reply, (MSGQ_Msg) msg);
	if (status != SYS_OK) {
		SYS_abort("Failed to send a message");
	}

	msg_proc = NULL;
	msg_gpp = NULL;

	for (;;) {
		status = MSGQ_get(msgq_out, (MSGQ_Msg *)&msg, SYS_FOREVER);
		if (status != SYS_OK) {
			SYS_abort("Failed to get a message");
		}

		msg_id = MSGQ_getMsgId((MSGQ_Msg)msg);
		switch (msg_id) {
		case GPP_OUTPUTMSGID:
			msg_gpp = msg;
			break;
		case DSP_PROCESSMSGID:
			msg_proc = msg;
			break;
		default:
			SYS_abort("Unknown message received");
			break;
		}

		LOG_printf(&trace, "Received message from TSK");

		if ((msg_proc != NULL) && (msg_gpp != NULL)) {
			MSGQ_setMsgId((MSGQ_Msg) msg_gpp, DSP_OUTPUTMSGID);
		
			status = MSGQ_put(msgq_reply, (MSGQ_Msg) msg_gpp);
			if (status != SYS_OK) {
				SYS_abort("Failed to send a message");
			}
		
			BCACHE_wb(msg_proc->data, BUFSIZE, TRUE);

			MSGQ_setMsgId((MSGQ_Msg) msg_proc, DSP_OUTPUTMSGID);

			status = MSGQ_put(msgq_gpp, (MSGQ_Msg) msg_proc);
			if (status != SYS_OK) {
				SYS_abort("Failed to send a message");
			}

			msg_proc = NULL;
			msg_gpp = NULL;
		}
	}
}
