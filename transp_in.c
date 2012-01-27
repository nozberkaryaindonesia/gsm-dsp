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
	int msgqStatus;
	MSGQ_Attrs msgqAttrs;
	SEM_Handle inputSemHandle;

	inputSemHandle = SEM_create(0, NULL);

	if (inputSemHandle == NULL) {
		SYS_abort("Semaphore creation failed");
	}

	msgqAttrs = MSGQ_ATTRS;
	msgqAttrs.notifyHandle = (Ptr)inputSemHandle;
	msgqAttrs.pend = (MSGQ_Pend)SEM_pendBinary;
	msgqAttrs.post = (MSGQ_Post)SEM_postBinary;

	msgqStatus = MSGQ_open("inputDSP", &msgq_in, &msgqAttrs);
	if (msgqStatus != SYS_OK) {
		SYS_abort("Failed to open the input message queue");
	}
}

void input_thrd()
{
	struct cmsg *msg, *inputMsg, *outputMsg;
	int msgqStatus;
	MSGQ_Queue dstMsgQueue;
	MSGQ_Queue replyMsgQueue;
	MSGQ_LocateAttrs syncLocateAttrs;
	Int msgId;

	msgqStatus = MSGQ_locate("process", &dstMsgQueue, NULL);
	if (msgqStatus != SYS_OK) {
		SYS_abort("Failed to locate the process message queue");
	 }
	
	msgqStatus = SYS_ENOTFOUND;
	while ((msgqStatus == SYS_ENOTFOUND) || (msgqStatus == SYS_ENODEV)) {
		syncLocateAttrs.timeout = SYS_FOREVER;
		msgqStatus = MSGQ_locate("outputGPP", &replyMsgQueue, &syncLocateAttrs);
		if ((msgqStatus == SYS_ENOTFOUND) || (msgqStatus == SYS_ENODEV)) {
			TSK_sleep(1000);
		}
		else if (msgqStatus != SYS_OK) {
			SYS_abort("Failed to locate the outputGPP message queue");
		}
	}

	msgqStatus = MSGQ_alloc(0, (MSGQ_Msg *)&inputMsg, APPMSGSIZE);
	if (msgqStatus != SYS_OK) {
		SYS_abort("Failed to allocate a message");
	}

	MSGQ_setMsgId((MSGQ_Msg)inputMsg, DSP_INPUTMSGID);
	msgqStatus = MSGQ_put(replyMsgQueue, (MSGQ_Msg)inputMsg);
	if (msgqStatus != SYS_OK) {
		SYS_abort("Failed to send a message");
	}

	inputMsg = NULL;
	outputMsg = NULL;

	for (;;) {
		msgqStatus = MSGQ_get(msgq_in, (MSGQ_Msg *)&msg, SYS_FOREVER);
		if (msgqStatus != SYS_OK) {
			SYS_abort("Failed to get a message from GPP");
		}
		
		msgId = MSGQ_getMsgId((MSGQ_Msg)msg);
		
		switch (msgId) {
		case GPP_OUTPUTMSGID:
			BCACHE_inv(msg->data, BUFSIZE, TRUE);
			outputMsg = msg;
			break;
		case DSP_PROCESSMSGID:
			inputMsg = msg;
			break;
		case TERMINATEMSGID:
			inputMsg = msg;
			msgqStatus = MSGQ_put(replyMsgQueue,(MSGQ_Msg)inputMsg);
			if (msgqStatus != SYS_OK) {
				SYS_abort("Failed to send a GPP message back");
			}
		default:
			SYS_abort("Unknown message received");
			break;
		} 
		
		if ((inputMsg != NULL) && (outputMsg != NULL)) {
			msgqStatus = MSGQ_put(replyMsgQueue, (MSGQ_Msg)inputMsg);
			if (msgqStatus != SYS_OK) {
				SYS_abort("Failed to send a GPP message back");
			}

			MSGQ_setMsgId((MSGQ_Msg)outputMsg, DSP_INPUTMSGID);

			msgqStatus = MSGQ_put(dstMsgQueue, (MSGQ_Msg)outputMsg);
			if (msgqStatus != SYS_OK) {
				SYS_abort("Failed to send a message to process function");
			}
			LOG_printf(&trace, "Sending input msg to Processing TSK");

			inputMsg = NULL;
			outputMsg = NULL;
		}
	}
}
