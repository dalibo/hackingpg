/*
 * client, testing software
 *
 * This software is released under the PostgreSQL Licence.
 *
 * Guillaume Lelarge, guillaume@lelarge.info, 2023.
 *
 */

// #include système
#include <stdio.h>

// #include PostgreSQL
#include "libpq-fe.h"

int
main(int argc, char **argv)
{
  const char *conninfo;
  PGconn     *conn;

  if (argc > 1)
    conninfo = argv[1];
  else
    conninfo = "";

  conn = PQconnectdb(conninfo);

  if (PQstatus(conn) != CONNECTION_OK)
  {
    fprintf(stderr, "%s", PQerrorMessage(conn));
  }

  PQfinish(conn);

  return 0;
}
