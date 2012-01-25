#include <std.h>
#include <msgq.h>
#include <pool.h>
#include <zcpy_mqt.h>
#include <sma_pool.h>
#include "common.h"

#define NUMALLOCATORS		1
#define NUMMSGQUEUES		6

static ZCPYMQT_Params mqtParams = {APP_POOL_ID};

static SMAPOOL_Params smaPoolParams = {	0, TRUE };

static MSGQ_Obj msgQueues[NUMMSGQUEUES];

static MSGQ_TransportObj transports[MAX_PROCESSORS] = {
	MSGQ_NOTRANSPORT,
	{
		ZCPYMQT_init,
		&ZCPYMQT_FXNS,
		&mqtParams,
		NULL,
		ID_GPP
	}
};

MSGQ_Config MSGQ_config = {
	msgQueues,
	transports,
	NUMMSGQUEUES,
	MAX_PROCESSORS,
	0,
	MSGQ_INVALIDMSGQ,
	POOL_INVALIDID,
};

static POOL_Obj allocators[NUMALLOCATORS] = {
	{
		SMAPOOL_init,
		(POOL_Fxns *)&SMAPOOL_FXNS,
		&smaPoolParams,
		NULL,
	}
};

POOL_Config POOL_config = {allocators, NUMALLOCATORS};
