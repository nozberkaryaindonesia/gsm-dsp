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

extern LOG_Obj trace;
static MSGQ_Queue msgq_in;

void init_input_thrd() 
{
	int status;
	MSGQ_Attrs attrs;
	SEM_Handle input_sem_hdl;

	input_sem_hdl = SEM_create(0, NULL);

	if (input_sem_hdl == NULL) {
		SYS_abort("Semaphore creation failed");
	}

	attrs = MSGQ_ATTRS;
	attrs.notifyHandle = (Ptr) input_sem_hdl;
	attrs.pend = (MSGQ_Pend) SEM_pendBinary;
	attrs.post = (MSGQ_Post) SEM_postBinary;

	status = MSGQ_open("inputDSP", &msgq_in, &attrs);
	if (status != SYS_OK) {
		SYS_abort("Failed to open the inputDSP message queue");
	}
}

void input_thrd()
{
	struct link_msg *msg, *proc_msg, *gpp_msg;
	int status, msg_id;
	MSGQ_Queue proc_msgq;
	MSGQ_Queue gpp_msgq;
	MSGQ_LocateAttrs attrs;

	status = MSGQ_locate("processDSP", &proc_msgq, NULL);
	if (status != SYS_OK) {
		SYS_abort("Failed to locate the process message queue");
	 }
	
	status = SYS_ENOTFOUND;
	while ((status == SYS_ENOTFOUND) || (status == SYS_ENODEV)) {
		attrs.timeout = SYS_FOREVER;
		status = MSGQ_locate("outputGPP", &gpp_msgq, &attrs);
		if ((status == SYS_ENOTFOUND) || (status == SYS_ENODEV)) {
			TSK_sleep(1000);
		}
		else if (status != SYS_OK) {
			SYS_abort("Failed to locate the outputGPP message queue");
		}
	}

	status = MSGQ_alloc(0, (MSGQ_Msg *) &proc_msg, APPMSGSIZE);
	if (status != SYS_OK) {
		SYS_abort("Failed to allocate a message");
	}

	MSGQ_setMsgId((MSGQ_Msg) proc_msg, DSP_INPUTMSGID);
	proc_msg->data = NULL;

	status = MSGQ_put(gpp_msgq, (MSGQ_Msg) proc_msg);
	if (status != SYS_OK) {
		SYS_abort("Failed to send a message");
	}

	proc_msg = NULL;
	gpp_msg = NULL;

	for (;;) {
		status = MSGQ_get(msgq_in, (MSGQ_Msg *) &msg, SYS_FOREVER);
		if (status != SYS_OK) {
			SYS_abort("Failed to get a message from GPP");
		}
		
		msg_id = MSGQ_getMsgId((MSGQ_Msg) msg);
		
		switch (msg_id) {
		case GPP_OUTPUTMSGID:
			BCACHE_inv(msg->data, BUFSIZE, TRUE);
			gpp_msg = msg;
			break;
		case DSP_PROCESSMSGID:
			proc_msg = msg;
			break;
		case TERMINATEMSGID:
			proc_msg = msg;
			status = MSGQ_put(gpp_msgq, (MSGQ_Msg) proc_msg);
			if (status != SYS_OK) {
				SYS_abort("Failed to send a GPP message back");
			}
		default:
			SYS_abort("Unknown message received");
			break;
		}
		
		if ((proc_msg != NULL) && (gpp_msg != NULL)) {
			status = MSGQ_put(gpp_msgq, (MSGQ_Msg) proc_msg);
			if (status != SYS_OK) {
				SYS_abort("Failed to send a GPP message back");
			}

			MSGQ_setMsgId((MSGQ_Msg) gpp_msg, DSP_INPUTMSGID);

			status = MSGQ_put(proc_msgq, (MSGQ_Msg) gpp_msg);
			if (status != SYS_OK) {
				SYS_abort("Failed to send a message to process function");
			}

			proc_msg = NULL;
			gpp_msg = NULL;
		}
	}
}
