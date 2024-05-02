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

/* Required in all loadable module */
PG_MODULE_MAGIC;

/*
 * Declare main function of the bgworker and make sure it is callable from
 * external code using "PGDLLEXPORT"
 */
PGDLLEXPORT void cpg_main(Datum main_arg);

/*
 * The bgworker main function.
 *
 * This is called from the bgworker backend freshly forked and setup by
 * postmaster.
 */
void
cpg_main(Datum main_arg)
{
	elog(LOG, "[cpg] Starting…");
	elog(LOG, "[cpg] …and leaving.");

	exit(0);
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

	/* Start the bgworker as soon as the postmaster can */
	worker.bgw_start_time = BgWorkerStart_PostmasterStart;

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
