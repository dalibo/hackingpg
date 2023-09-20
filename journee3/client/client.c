/*
 * client, testing software
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
#include "common/string.h"

int
main(int argc, char **argv)
{
  char   *conninfo;
  PGconn *conn;
  char   *password = NULL;
  bool    new_password;
  PGresult   *res;

  pg_logging_init(argv[0]);

  do
  {
    if (argc > 1)
      conninfo = argv[1];
    else
      conninfo = "";

    if (password)
      sprintf(conninfo, "%s password=%s", conninfo, password);

    new_password = false;
    conn = PQconnectdb(conninfo);

    if (!conn)
    {
      pg_fatal("could not connect");
    }

    if (PQstatus(conn) == CONNECTION_BAD &&
        PQconnectionNeedsPassword(conn)  &&
        !password)
    {
      PQfinish(conn);
      password = simple_prompt("Password: ", false);
      new_password = true;
    }
  } while (new_password);

  if (PQstatus(conn) == CONNECTION_BAD)
  {
      pg_log_error("could not connect: %s", PQerrorMessage(conn));
      return 2;
  }

  pg_log_debug("Connection successfull!");

  res = PQexec(conn, "SELECT version()");

  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    pg_log_error("query failed: %s", PQerrorMessage(conn));
  }
  else
  {
    printf("PostgreSQL Release: %s\n", PQgetvalue(res, 0, 0));
  }

  PQclear(res);

  PQfinish(conn);

  return 0;
}
