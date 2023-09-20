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
#include "common/string.h"

int
main(int argc, char **argv)
{
  char   *conninfo;
  PGconn *conn;
  char   *password = NULL;
  bool    new_password;

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
      fprintf(stderr, "could not connect\n");
      return 1;
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
      fprintf(stderr, "could not connect: %s\n", PQerrorMessage(conn));
      return 2;
  }

  fprintf(stderr, "Connection successfull!\n");

  PQfinish(conn);

  return 0;
}
