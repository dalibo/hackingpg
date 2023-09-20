/*
 * drop, dropping a database
 *
 * This software is released under the PostgreSQL Licence.
 *
 * Guillaume Lelarge, guillaume@lelarge.info, 2023.
 *
 */

// #include
#include "libpq-fe.h"
#include "postgres_fe.h"
#include "common/logging.h"
#include "fe_utils/connect_utils.h"

int
main(int argc, char **argv)
{
  const char   *progname;
  PGconn	   *conn;
  ConnParams	cparams;

  pg_logging_init(argv[0]);
  progname = get_progname(argv[0]);

  cparams.dbname = NULL;
  cparams.pghost = NULL;
  cparams.pgport = NULL;
  cparams.pguser = NULL;
  cparams.override_dbname = NULL;

  conn = connectMaintenanceDatabase(&cparams, progname, false);
  PQfinish(conn);

  exit(0);
}
