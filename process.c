#include <std.h>
#include <log.h>
#include <tsk.h>
#include <sem.h>
#include <sys.h>
#include <msgq.h>

#include "common.h"
#include "gsm.h"

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
	msgq_attrs.notifyHandle = (Ptr)proc_sem_hdl;
	msgq_attrs.pend = (MSGQ_Pend)SEM_pendBinary;
	msgq_attrs.post = (MSGQ_Post)SEM_postBinary;

	status = MSGQ_open("proc", &msgq_proc, &msgq_attrs);
	if (status != SYS_OK) {
		SYS_abort("Failed to open the process message queue");
	}
}

void write_teste(short *buf, int len)
{
	int i;

	for (i=0; i < len; i++) {
		buf[2*i + 0] = i;
		buf[2*i + 1] = -i;
	}
}

void proc_thrd()
{	
	MyMsg *msg, *msg_in, *msg_out;
	int status;
	MSGQ_Queue msgq_pass;
	MSGQ_Queue msgq_reply;
	int msg_id;

	status = MSGQ_locate("output", &msgq_pass, NULL);
	if (status != SYS_OK) {
		SYS_abort("Failed to locate the output message queue");
	}

	status = MSGQ_locate("inputDSP", &msgq_reply, NULL);
	if (status != SYS_OK) {
		SYS_abort("Failed to locate the output message queue");
	}

	status = MSGQ_alloc(0, (MSGQ_Msg *)&msg_in, APPMSGSIZE);
	if (status != SYS_OK) {
		SYS_abort("Failed to allocate a message");
	}

	MSGQ_setMsgId( (MSGQ_Msg)msg_in, DSP_PROCESSMSGID);
	
	status = MSGQ_put(msgq_reply, (MSGQ_Msg)msg_in);
	if (status != SYS_OK) {
		SYS_abort("Failed to send a message");
	 }

	msg_in = NULL;
	msg_out = NULL;

	for (;;) {
		status = MSGQ_get(msgq_proc, (MSGQ_Msg *)&msg, SYS_FOREVER);
		if (status != SYS_OK) {
			SYS_abort("Failed to get a message");
		}

		msg_id = MSGQ_getMsgId((MSGQ_Msg)msg);

		switch (msg_id) {
		case DSP_INPUTMSGID:
			msg_in = msg;
			break;
		case DSP_OUTPUTMSGID:
			msg_out = msg;
			break;
		default:
			SYS_abort("Unknown message received");
			break;
		}

		if ((msg_in != NULL) && (msg_out != NULL)) {
			if (msg_out->dataBuffer != NULL) {
				//write_test(msg_out->dataBuffer, 1024 * 2);
				handle_msg(msg_in->dataBuffer, 0, msg_out->procBuffer, 0,
				msg_out->dataBuffer, 0);
			}

			MSGQ_setMsgId((MSGQ_Msg)msg_in, DSP_PROCESSMSGID);
			status = MSGQ_put(msgq_reply, (MSGQ_Msg)msg_in);

			MSGQ_setMsgId((MSGQ_Msg)msg_out, DSP_PROCESSMSGID);
			status = MSGQ_put(msgq_pass, (MSGQ_Msg)msg_out);
			LOG_printf(&trace, "Process and passed msg with buffer");

			msg_in = NULL;
			msg_out = NULL;
		}
	}
}

