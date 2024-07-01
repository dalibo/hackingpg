/*-------------------------------------------------------------------------
 * cpg.c: A Corosync background worker for PostgreSQL
 *
 * Its only goal is to detect which node is the primary and dynamically
 * set the primary conninfo in accordance.
 *
 * Base code inspired by src/test/modules/worker_spi/worker_spi.c
 * Documentation: https://www.postgresql.org/docs/current/bgworker.html
 *
 * This program is open source, licensed under the PostgreSQL license.
 * For license terms, see the LICENSE file.
 *
 * Copyright (c) 2023-2024, Jehan-Guillaume de Rorthais
 *-------------------------------------------------------------------------
 */

#include <limits.h>					// INT_MAX

#include "postgres.h"				// definitions useful for PostgreSQL procs
#include "postmaster/bgworker.h"	// for a bgworker
#include "miscadmin.h"				// various variables, macros and funcs
#include "fmgr.h"					// needed for PG_MODULE_MAGIC
#include "storage/latch.h"			// for *Latch()
#include "utils/wait_event.h"		// for *Latch()
#include "utils/guc.h"				// for GUC
#include "postmaster/interrupt.h"	// for HandleMainLoopInterrupts() & co
#include "utils/ps_status.h"		// rename process
#include "access/xlog.h"			// for RecoveryInProgress()

/* Required in all loadable module */
PG_MODULE_MAGIC;

/*
 * Declare main function of the bgworker and make sure it is callable from
 * external code using "PGDLLEXPORT"
 */
PGDLLEXPORT void cpg_main(Datum main_arg);

/*
 * Global variables
 */

/* Maximum interval between bgworker wakeups */
static int interval;
/* Remember recovery state */
static bool in_recovery;

/*
 * Functions
 */

/*
 * Update processus title
 */
static void
update_ps_display()
{
	char ps_display[128];

	if (in_recovery)
		snprintf(ps_display, 128, "Hello!");
	else
		snprintf(ps_display, 128, "I'm the primary!");

	set_ps_display(ps_display);
}

/*
 * Signal handler for SIGTERM
 *
 * Say good bye!
 */
static void
cpg_sigterm(SIGNAL_ARGS)
{
	elog(LOG, "[cpg] …and leaving");

	exit(0);
}

/*
 * The bgworker main function.
 *
 * This is called from the bgworker backend freshly forked and setup by
 * postmaster.
 */
void
cpg_main(Datum main_arg)
{
	/*
	 * By default, signals are blocked when the background worker starts.
	 * This allows to set up some signal handlers during the worker
	 * startup.
	 */

	/*
	 * Install signal handlers
	 */
	/* Leave gracefully on shutdown */
	pqsignal(SIGTERM, cpg_sigterm);
	/* Core facility to handle config reload */
	pqsignal(SIGHUP, SignalHandlerForConfigReload);

	/* We can now unblock signals */
	BackgroundWorkerUnblockSignals();

	/*
	 * GUC declarations
	 */

	/* interval */
	DefineCustomIntVariable("cpg.interval",	// name
							"Defines the maximal interval in seconds between "
							"wakeups",		// description
							NULL,			// long description
							&interval,		// variable to set
							10,				// default value
							1,				// min value
							INT_MAX / 1000,	// max value
							PGC_SIGHUP,		// context
							GUC_UNIT_S,		// flags
							NULL,			// check hook
							NULL,			// assign hook
							NULL);			// show hook

	/*
	 * Lock namespace "cpg". Forbid creating any other GUC after here. Useful
	 * to remove unknown or mispelled GUCs from this domain.
	 */
#if PG_VERSION_NUM >= 150000
	MarkGUCPrefixReserved("cpg");
#else
	/* Note that this still exists in v15+ as a macro calling
	 * MarkGUCPrefixReserved */
	EmitWarningsOnPlaceholders("cpg");
#endif

	elog(LOG, "[cpg] Starting…");

	/* Set initial status and proc title (in this order) */
	in_recovery = RecoveryInProgress();

	update_ps_display();

	/* Event loop */
	for (;;)
	{
		CHECK_FOR_INTERRUPTS();

		if (RecoveryInProgress() != in_recovery)
		{
			elog(LOG, "[cpg] I've been promoted!");
			in_recovery = RecoveryInProgress();
			update_ps_display();
		}
		else
			elog(LOG, "[cpg] Hi!");

		/* Facility from core. checks if some smiple and common interrupts,
		 * including SIGHUP to reload conf files, and process them. */
		HandleMainLoopInterrupts();

		/* Wait for an event or timeout */
		WaitLatch(MyLatch,
				  WL_LATCH_SET | WL_TIMEOUT | WL_EXIT_ON_PM_DEATH,
				  interval*1000, /* convert to milliseconds */
				  PG_WAIT_EXTENSION);
		ResetLatch(MyLatch);
	}

	elog(LOG, "[cpg] oops, the event loop broke");

	exit(1);
}

/*
 * Module init.
 *
 * This is called from the Postmaster process.
 */
void
_PG_init(void)
{
	BackgroundWorker worker;

	/*
	 * This bgw can only be loaded from shared_preload_libraries.
	 */
	if (!process_shared_preload_libraries_in_progress)
		return;

	/*
	 * Setup the bgworker
	 */

	/* Memory init */
	memset(&worker, 0, sizeof(worker));

	/* The bgw _currently_ don't need shmem access neither database connection.
	 * However, documentation states that BGWORKER_SHMEM_ACCESS is always
	 * required. If omitted, the bgw is fully ignored by postmaster. */
	worker.bgw_flags = BGWORKER_SHMEM_ACCESS;

	/* Start the bgworker as soon as a consistant state has been reached */
	worker.bgw_start_time = BgWorkerStart_ConsistentState;

	/* Set the name of the bgwriter library */
	snprintf(worker.bgw_library_name, BGW_MAXLEN, "cpg");

	/* Set the name of main function to start the bgworker */
	snprintf(worker.bgw_function_name, BGW_MAXLEN, "cpg_main");

	/* Set the name of the bgworker */
	snprintf(worker.bgw_name, BGW_MAXLEN, "cpg");

	/* For this demo, just never come back if the bgw exits */
	worker.bgw_restart_time = BGW_NEVER_RESTART;

	/* No arg to pass to the bgworker's main function */
	worker.bgw_main_arg = (Datum) 0;

	/* This bgworker is not loaded dynamicaly, no backend to notify after
	 * successful start */
	worker.bgw_notify_pid = 0;

	/*
	 * Register the bgworker with the postmaster
	 */
	RegisterBackgroundWorker(&worker);
}