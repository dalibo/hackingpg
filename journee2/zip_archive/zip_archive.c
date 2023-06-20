/* PostgreSQL headers */
#include "postgres.h"
#include "fmgr.h"
#include "utils/builtins.h"
#include "postmaster/pgarch.h"

/* libzip header */
#include <zip.h>

/* module declaration */
PG_MODULE_MAGIC;

/* function definitions */
void        _PG_archive_module_init(ArchiveModuleCallbacks *cb);
static bool zip_archive_file(const char *file, const char *path);
PG_FUNCTION_INFO_V1(get_libzip_version);

/* function code */

void
_PG_archive_module_init(ArchiveModuleCallbacks *cb)
{
  cb->archive_file_cb = zip_archive_file;
}

static bool
zip_archive_file(const char *file, const char *path)
{
  elog(LOG, "file is \"%s\"", file);
  elog(LOG, "path is \"%s\"", path);

  return true;
}

/*
 * get_libzip_version
 *
 * Returns libzip version.
 */
Datum
get_libzip_version(PG_FUNCTION_ARGS)
{
  PG_RETURN_TEXT_P(cstring_to_text(zip_libzip_version()));
}
