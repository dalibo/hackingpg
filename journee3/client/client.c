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
  char     *conninfo;
  PGconn   *conn;
  char     *password = NULL;
  bool      new_password;
  char     *query;
  PGresult *res;

  int       nfields;
  int       nrows;
  int       i, j, l;
  int      *length;
  char     *pad;

  pg_logging_init(argv[0]);
  pg_logging_set_level(PG_LOG_DEBUG);

  // Trying to connect
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

  pg_log_debug("Connection successfull! (backend PID is %d)", PQbackendPID(conn));

  if (argc > 2)
    query = argv[2];
  else
    query = "SELECT version()";

  // Trying to execute query
  while (true)
  {
    res = PQexec(conn, query);

    if (PQresultStatus(res) != PGRES_TUPLES_OK)
    {
      if (PQstatus(conn) != CONNECTION_OK)
      {
        pg_log_info("resetting connection\n");
        PQreset(conn);
        if (PQstatus(conn) != CONNECTION_BAD)
          pg_log_debug("Reset successfull! (backend PID is %d)", PQbackendPID(conn));
      }
      else
      {
        pg_log_error("query failed: %s", PQerrorMessage(conn));
      }
    }
    else
    {
      fprintf(stdout, "\n");

      /* get the number of fields */
      nrows = PQntuples(res);
      nfields = PQnfields(res);

      /* for each field, get the needed width */
      length = (int *) pg_malloc(sizeof(int) * nfields);
      for (j = 0; j < nfields; j++)
        length[j] = strlen(PQfname(res, j));

      for (i = 0; i < nrows; i++)
      {
        for (j = 0; j < nfields; j++)
        {
          l = strlen(PQgetvalue(res, i, j));
          if (l > length[j])
            length[j] = strlen(PQgetvalue(res, i, j));
        }
      }

      /* print a header */
      for (j = 0, l = 0; j < nfields; j++)
      {
        fprintf(stdout, "%*s", length[j] + 2, PQfname(res, j));
        l += length[j] + 2;
      }
      fprintf(stdout, "\n");
      pad = (char *) pg_malloc(l + 1);
      memset(pad, '-', l);
      pad[l] = '\0';
      fprintf(stdout, "%s\n", pad);
      free(pad);

      /* for each row, dump the information */
      for (i = 0; i < nrows; i++)
      {
        for (j = 0; j < nfields; j++)
          fprintf(stdout, "%*s", length[j] + 2, PQgetvalue(res, i, j));
        fprintf(stdout, "\n");
      }

      /* cleanup */
      PQclear(res);
      free(length);
    }

    PQclear(res);

    sleep(1);
  }

  PQfinish(conn);

  return 0;
}
