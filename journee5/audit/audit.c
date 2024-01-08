/*
 * audit, auditing one table
 *
 * This software is released under the PostgreSQL Licence.
 *
 * Guillaume Lelarge, guillaume@lelarge.info, 2024.
 *
 */

// #include
#include "libpq-fe.h"
#include "postgres_fe.h"
#include "common/logging.h"
#include "fe_utils/connect_utils.h"
#include "fe_utils/option_utils.h"
#include "fe_utils/string_utils.h"
#include "getopt_long.h"

static void help(const char *progname);

int
main(int argc, char **argv)
{
  const char   *progname;
  PGconn     *conn;
  ConnParams  cparams;
  static struct option long_options[] = {
    {"dbname", required_argument, NULL, 'd'},
    {"host", required_argument, NULL, 'h'},
    {"port", required_argument, NULL, 'p'},
    {"username", required_argument, NULL, 'U'},
    {"echo", no_argument, NULL, 'e'},
    {NULL, 0, NULL, 0}
  };
  int           optindex;
  int           c;
  char         *dbname = NULL;
  char         *host = NULL;
  char         *port = NULL;
  char         *username = NULL;
  char         *table = NULL;
  bool          echo = false;

  pg_logging_init(argv[0]);
  progname = get_progname(argv[0]);

  handle_help_version_opts(argc, argv, "audit", help);

  // Get options

  while ((c = getopt_long(argc, argv, "d:eh:p:U:", long_options, &optindex)) != -1)
  {
    switch (c)
    {
      case 'd':
        dbname = pg_strdup(optarg);
        break;
      case 'e':
        echo = true;
        break;
      case 'h':
        host = pg_strdup(optarg);
        break;
      case 'p':
        port = pg_strdup(optarg);
        break;
      case 'U':
        username = pg_strdup(optarg);
        break;
      case 0:
        /* this covers the long options */
        break;
      default:
        /* getopt_long already emitted a complaint */
        pg_log_error_hint("Try \"%s --help\" for more information.", progname);
        exit(1);
    }
  }

  if (!dbname)
    dbname = "postgres";

  switch (argc - optind)
  {
    case 0:
      pg_log_error("missing required argument table");
      pg_log_error_hint("Try \"%s --help\" for more information.", progname);
      exit(1);
    case 1:
      table = argv[optind];
      break;
    default:
      pg_log_error("too many command-line arguments (first is \"%s\")",
             argv[optind + 1]);
      pg_log_error_hint("Try \"%s --help\" for more information.", progname);
      exit(1);
  }

  // Connect to the database

  cparams.dbname = dbname;
  cparams.pghost = host;
  cparams.pgport = port;
  cparams.pguser = username;
  cparams.prompt_password = TRI_DEFAULT;
  cparams.override_dbname = NULL;

  conn = connectDatabase(&cparams, progname, echo, false, false);

  // Main Stuff

  pg_log_info("table is %s", table);

  // Disconnect

  PQfinish(conn);

  exit(0);
}

static void
help(const char *progname)
{
	printf("%s records PostgreSQL statistics.\n\n", progname);
	printf("Usage:\n");
	printf("  %s TABLE [OPTION]...\n", progname);
	printf("\nOptions:\n");
	printf("  -e, --echo                show the commands being sent to the server\n");
	printf("  -V, --version             output version information, then exit\n");
	printf("  -?, --help                show this help, then exit\n");
	printf("\nConnection options:\n");
	printf("  -d, --dbname=DBNAME       database name\n");
	printf("  -h, --host=HOSTNAME       database server host or socket directory\n");
	printf("  -p, --port=PORT           database server port\n");
	printf("  -U, --username=USERNAME   user name to connect as\n");
	printf("\nReport bugs to <guillaume@lelarge.info>.\n");
}
