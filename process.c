#include <std.h>
#include <log.h>
#include <tsk.h>
#include <sem.h>
#include <sys.h>
#include <msgq.h>

#include "common.h"
#include "msg.h"

extern LOG_Obj trace;
static MSGQ_Queue msgq_proc;

void init_proc_thrd()
{
	int status;
	MSGQ_Attrs attrs;
	SEM_Handle proc_sem_hdl;

	proc_sem_hdl = SEM_create(0, NULL);

	if (proc_sem_hdl == NULL) {
		SYS_abort("Semaphore creation failed");
	}

	attrs = MSGQ_ATTRS;
	attrs.notifyHandle = (Ptr) proc_sem_hdl;
	attrs.pend = (MSGQ_Pend) SEM_pendBinary;
	attrs.post = (MSGQ_Post) SEM_postBinary;

	status = MSGQ_open("processDSP", &msgq_proc, &attrs);
	if (status != SYS_OK) {
		SYS_abort("Failed to open the process message queue");
	}
}

void proc_thrd()
{	
	struct link_msg *msg, *msg_dsp_in, *msg_dsp_out;
	int status, msg_id, msg_cnt;
	MSGQ_Queue msgq_dsp_out;
	MSGQ_Queue msgq_dsp_in;

	status = MSGQ_locate("outputDSP", &msgq_dsp_out, NULL);
	if (status != SYS_OK) {
		SYS_abort("Failed to locate the output message queue");
	}

	status = MSGQ_locate("inputDSP", &msgq_dsp_in, NULL);
	if (status != SYS_OK) {
		SYS_abort("Failed to locate the output message queue");
	}

	status = MSGQ_alloc(0, (MSGQ_Msg *) &msg_dsp_in, APPMSGSIZE);
	if (status != SYS_OK) {
		SYS_abort("Failed to allocate a message");
	}

	MSGQ_setMsgId( (MSGQ_Msg) msg_dsp_in, DSP_PROCESSMSGID);
	msg_dsp_in->data = NULL;
	
	status = MSGQ_put(msgq_dsp_in, (MSGQ_Msg) msg_dsp_in);
	if (status != SYS_OK) {
		SYS_abort("Failed to send a message");
	}

	msg_dsp_in = NULL;
	msg_dsp_out = NULL;
	msg_cnt = 0;

	for (;;) {
		status = MSGQ_get(msgq_proc, (MSGQ_Msg *) &msg, SYS_FOREVER);
		if (status != SYS_OK) {
			SYS_abort("Failed to get a message");
		}

		msg_id = MSGQ_getMsgId((MSGQ_Msg) msg);

		switch (msg_id) {
		case DSP_INPUTMSGID:
			msg_dsp_in = msg;
			break;
		case DSP_OUTPUTMSGID:
			msg_dsp_out = msg;
			break;
		default:
			SYS_abort("Unknown message received");
			break;
		}

		if ((msg_dsp_in != NULL) && (msg_dsp_out != NULL)) {
			if (msg_cnt > 0) {
				handle_msg(msg_dsp_in->data, 0, msg_dsp_out->data, 0);
			}
			msg_cnt++;

			MSGQ_setMsgId((MSGQ_Msg)msg_dsp_in, DSP_PROCESSMSGID);
			status = MSGQ_put(msgq_dsp_in, (MSGQ_Msg) msg_dsp_in);

			MSGQ_setMsgId((MSGQ_Msg)msg_dsp_out, DSP_PROCESSMSGID);
			status = MSGQ_put(msgq_dsp_out, (MSGQ_Msg) msg_dsp_out);

			msg_dsp_in = NULL;
			msg_dsp_out = NULL;
		}
	}
}
