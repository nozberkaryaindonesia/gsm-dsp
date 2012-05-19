#include <std.h>
#include <msgq.h>
#include <pool.h>
#include <zcpy_mqt.h>
#include <sma_pool.h>
#include "common.h"

#define NUMALLOCATORS		1
#define NUMMSGQUEUES		6

static ZCPYMQT_Params mqt_params = {APP_POOL_ID};

static SMAPOOL_Params sma_pool_params = { 0, TRUE };

static MSGQ_Obj msgqs[NUMMSGQUEUES];

static MSGQ_TransportObj transports[MAX_PROCESSORS] = {
	MSGQ_NOTRANSPORT,
	{
		ZCPYMQT_init,
		&ZCPYMQT_FXNS,
		&mqt_params,
		NULL,
		ID_GPP
	}
};

MSGQ_Config MSGQ_config = {
	msgqs,
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
		&sma_pool_params,
		NULL,
	}
};

POOL_Config POOL_config = {allocators, NUMALLOCATORS};
