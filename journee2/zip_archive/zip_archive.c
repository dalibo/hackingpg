/* PostgreSQL headers */
#include "postgres.h"
#include "fmgr.h"
#include "utils/builtins.h"
#include "postmaster/pgarch.h"
#include "utils/guc.h"
#include "funcapi.h"
#include "utils/timestamp.h"

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
PG_FUNCTION_INFO_V1(get_archive_stats);

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

Datum
get_archive_stats(PG_FUNCTION_ARGS)
{
  TupleDesc       tupdesc;
  Datum           values[5];
  bool            nulls[5];
  HeapTuple       tuple;
  Datum           result;
  char            destination[MAXPGPATH];
  zip_t          *ziparchive;
  int             error;
  int             entries_count;
  struct zip_stat zipstat;

  if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
    ereport(ERROR,
        (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
         errmsg("function returning record called in context that cannot accept type record")));

  if (cluster_name != NULL && cluster_name[0] != '\0')
  {
    snprintf(destination, MAXPGPATH, "%s/%s.zip", archive_directory, cluster_name);
  }
  else
  {
    snprintf(destination, MAXPGPATH, "%s/%s.zip", archive_directory, "zip_archive");
  }

  ziparchive = zip_open(destination, ZIP_CREATE, &error);
  if (!ziparchive)
  {
    zip_error_t ziperror;
    zip_error_init_with_code(&ziperror, error);
    elog(ERROR, "cannot open zip archive '%s': %s\n", destination, zip_error_strerror(&ziperror));
    // ne va pas être exécuté
    zip_error_fini(&ziperror);
  }

  entries_count = zip_get_num_entries(ziparchive, 0);

  values[0] = Int32GetDatum(entries_count);
  nulls[0] = false;

  values[1] = CStringGetTextDatum(zip_get_name(ziparchive, 0, ZIP_FL_ENC_GUESS));
  nulls[1] = false;

  nulls[2] = entries_count < 0;
  if (!nulls[2])
  {
    values[2] = CStringGetTextDatum(zip_get_name(ziparchive, entries_count-1, ZIP_FL_ENC_GUESS));
  }

  zip_stat_index(ziparchive, 0, 0, &zipstat);
  nulls[3] = !(zipstat.valid && ZIP_STAT_MTIME);
  if (!nulls[3])
  {
    values[3] = TimestampTzGetDatum(time_t_to_timestamptz(zipstat.mtime));
  }

  zip_stat_index(ziparchive, entries_count-1, 0, &zipstat);
  nulls[4] = !(zipstat.valid && ZIP_STAT_MTIME);
  if (!nulls[4])
  {
    values[4] = TimestampTzGetDatum(time_t_to_timestamptz(zipstat.mtime));
  }

  tuple = heap_form_tuple(tupdesc, values, nulls);
  result = HeapTupleGetDatum(tuple);

  PG_RETURN_DATUM(result);
}
