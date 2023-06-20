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
  zip_t        *ziparchive;
  zip_source_t *zipsource;
  int           error;

  elog(LOG, "archiving \"%s\"", file);

  ziparchive = zip_open("/tmp/test.zip", ZIP_CREATE, &error);
  zipsource = zip_source_file(ziparchive, path, 0, 0);
  zip_file_add(ziparchive, file, zipsource, ZIP_FL_ENC_GUESS);
  zip_close(ziparchive);

  elog(LOG, "\"%s\" is archived!", file);

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
