/* PostgreSQL headers */
#include "postgres.h"
#include "fmgr.h"
#include "utils/builtins.h"

/* libzip header */
#include <zip.h>

/* module declaration */
PG_MODULE_MAGIC;

/* function code */

PG_FUNCTION_INFO_V1(get_libzip_version);

/*
 *  * get_libzip_version
 *   *
 *    * Returns libzip version.
 *     */
Datum
get_libzip_version(PG_FUNCTION_ARGS)
{
  PG_RETURN_TEXT_P(cstring_to_text(zip_libzip_version()));
}
