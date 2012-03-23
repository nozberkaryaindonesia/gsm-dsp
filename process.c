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
	MSGQ_Attrs msgq_attrs;
	SEM_Handle proc_sem_hdl;

	proc_sem_hdl = SEM_create(0, NULL);

	if (proc_sem_hdl == NULL) {
		SYS_abort("Semaphore creation failed");
	}

	msgq_attrs = MSGQ_ATTRS;
	msgq_attrs.notifyHandle = (Ptr) proc_sem_hdl;
	msgq_attrs.pend = (MSGQ_Pend) SEM_pendBinary;
	msgq_attrs.post = (MSGQ_Post) SEM_postBinary;

	status = MSGQ_open("processDSP", &msgq_proc, &msgq_attrs);
	if (status != SYS_OK) {
		SYS_abort("Failed to open the process message queue");
	}
}

void write_test(float *buf, int len, int val)
{
	int i;

	for (i=0; i < len; i++) {
		buf[2*i + 0] = (float) val;
		buf[2*i + 1] = (float) val;
	}
}

void proc_thrd()
{	
	struct cmsg *msg, *msg_dsp_in, *msg_dsp_out;
	int status;
	MSGQ_Queue msgq_dsp_out;
	MSGQ_Queue msgq_dsp_in;
	int msg_id;
	int msg_cnt = 0;

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
	
	status = MSGQ_put(msgq_dsp_in, (MSGQ_Msg) msg_dsp_in);
	if (status != SYS_OK) {
		SYS_abort("Failed to send a message");
	}

	msg_dsp_in = NULL;
	msg_dsp_out = NULL;

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
				//write_test((float *) msg_dsp_out->data, 32 * 2, msg_cnt);
			}
			msg_cnt++;

			MSGQ_setMsgId((MSGQ_Msg)msg_dsp_in, DSP_PROCESSMSGID);
			status = MSGQ_put(msgq_dsp_in, (MSGQ_Msg) msg_dsp_in);

			MSGQ_setMsgId((MSGQ_Msg)msg_dsp_out, DSP_PROCESSMSGID);
			status = MSGQ_put(msgq_dsp_out, (MSGQ_Msg) msg_dsp_out);
			LOG_printf(&trace, "Process and passed msg with buffer");

			msg_dsp_in = NULL;
			msg_dsp_out = NULL;
		}
	}
}
