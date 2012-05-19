#include <std.h>
#include <tsk.h>
#include <log.h>
#include <dsplink.h>
#include <failure.h>

extern void init_proc_thrd();
extern void init_input_thrd();
extern void init_output_thrd();

extern void input_thrd();
extern void proc_thrd();
extern void output_thrd();

extern LOG_Obj trace;
static void init_thrds();

void main() 
{
	TSK_Attrs attrs = TSK_ATTRS;

	DSPLINK_init() ;

	init_input_thrd();
	init_output_thrd();
	init_proc_thrd();

	attrs.name = "init_thrds";
	attrs.stacksize = 0x1000;
	if (TSK_create((Fxn) init_thrds, &attrs) == NULL) {
		SYS_abort("Failed create echo thread");
	}

	return;
}

void init_thrds ( void )
{
	TSK_Attrs attrs = TSK_ATTRS;
	
	attrs.name = "input";
	attrs.stacksize = 0x1000;
	if (TSK_create((Fxn) input_thrd, &attrs) == NULL) {
		SYS_abort("Failed create input thread");
	}

	attrs.name = "proc";
	attrs.stacksize = 0x1000;
	if (TSK_create((Fxn) proc_thrd, &attrs) == NULL) {
		SYS_abort("Failed create process thread");
	}

	attrs.name = "output";
	attrs.stacksize = 0x1000;
	if (TSK_create((Fxn) output_thrd, &attrs) == NULL) {
		SYS_abort("Failed create output thread");
	}
}
