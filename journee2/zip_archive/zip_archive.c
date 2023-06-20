/* PostgreSQL headers */
#include "postgres.h"
#include "fmgr.h"
#include "utils/builtins.h"
#include "postmaster/pgarch.h"
#include "utils/guc.h"

/* libzip header */
#include <zip.h>

/* module declaration */
PG_MODULE_MAGIC;

/* structure definitions */
typedef enum CompressionMethods
{
  UNCOMPRESSED,
  BZIP2,
  ZLIB,
  XZ,
  ZSTD
} CompressionMethod;

static const struct config_enum_entry compression_methods[] = {
  {"uncompressed", UNCOMPRESSED, false},
  {"bzip2", BZIP2, false},
  {"zlib", ZLIB, false},
  {"xz", XZ, false},
  {"zstd", ZSTD, false},
  {NULL, 0, false}
};

/* variable definitions */
static char *archive_directory = NULL;
static int   compression_method = ZLIB;

/* function definitions */
void        _PG_init(void);
void        _PG_archive_module_init(ArchiveModuleCallbacks *cb);
static bool zip_archive_configured(void);
static bool zip_archive_file(const char *file, const char *path);
PG_FUNCTION_INFO_V1(get_libzip_version);

/* function code */

void
_PG_init(void)
{
  DefineCustomStringVariable("zip_archive.archive_directory",
    gettext_noop("Répertoire contenant le fichier de l'archive ZIP."),
    NULL,
    &archive_directory,
    "",
    PGC_SIGHUP,
    0,
    NULL, NULL, NULL);

  DefineCustomEnumVariable("zip_archive.compression_method",
    "Méthode utilisée pour la compression.",
    NULL,
    &compression_method,
    ZLIB,
    compression_methods,
    PGC_SIGHUP,
    0,
    NULL, NULL, NULL);

  MarkGUCPrefixReserved("zip_archive");
}

void
_PG_archive_module_init(ArchiveModuleCallbacks *cb)
{
  cb->check_configured_cb = zip_archive_configured;
  cb->archive_file_cb = zip_archive_file;
}

static bool
zip_archive_configured(void)
{
  return archive_directory != NULL && archive_directory[0] != '\0';
}

static bool
zip_archive_file(const char *file, const char *path)
{
  char          destination[MAXPGPATH];
  zip_t        *ziparchive;
  zip_source_t *zipsource;
  int           error;
  int           index;
  int           compression;

  elog(LOG, "archiving \"%s\"", file);

  if (cluster_name != NULL && cluster_name[0] != '\0')
  {
    snprintf(destination, MAXPGPATH, "%s/%s.zip", archive_directory, cluster_name);
  }
  else
  {
    snprintf(destination, MAXPGPATH, "%s/%s.zip", archive_directory, "zip_archive");
  }

  ziparchive = zip_open(destination, ZIP_CREATE, &error);
  zipsource = zip_source_file(ziparchive, path, 0, 0);
  index = zip_file_add(ziparchive, file, zipsource, ZIP_FL_ENC_GUESS);

  switch(compression_method)
  {
    case UNCOMPRESSED:
      compression = ZIP_CM_STORE;
      break;
    case BZIP2:
      compression = ZIP_CM_BZIP2;
      break;
    case ZLIB:
      compression = ZIP_CM_DEFLATE;
      break;
    case XZ:
      compression = ZIP_CM_XZ;
      break;
    case ZSTD:
      compression = ZIP_CM_ZSTD;
      break;
  }
  zip_set_file_compression(ziparchive, index, compression, 1);

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
