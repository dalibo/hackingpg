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

typedef struct
{
  TupleDesc  tupdesc;
  zip_t     *ziparchive;
} ZipArchiveContext;

/* variable definitions */
static char *archive_directory = NULL;
static int   compression_method = ZLIB;
static char  destination[MAXPGPATH];

/* function definitions */
void        _PG_init(void);
void        _PG_archive_module_init(ArchiveModuleCallbacks *cb);
static bool zip_archive_configured(void);
static bool zip_archive_file(const char *file, const char *path);
PG_FUNCTION_INFO_V1(get_libzip_version);
PG_FUNCTION_INFO_V1(get_archive_stats);
PG_FUNCTION_INFO_V1(get_archived_wals);

/* function code */

/*
 * _PG_init
 *
 * Declares our GUCs.
 */
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

  zip_archive_configured();
}

/*
 * _PG_archive_module_init
 *
 * Returns the module's archiving callbacks.
 */
void
_PG_archive_module_init(ArchiveModuleCallbacks *cb)
{
  AssertVariableIsOfType(&_PG_archive_module_init, ArchiveModuleInit);

  cb->check_configured_cb = zip_archive_configured;
  cb->archive_file_cb = zip_archive_file;
  //cb->shutdown_cb
}

/*
 * zip_archive_configured
 *
 * Checks that archive_directory is not blank.
 */
static bool
zip_archive_configured(void)
{
  if (cluster_name != NULL && cluster_name[0] != '\0')
  {
    snprintf(destination, MAXPGPATH, "%s/%s.zip", archive_directory, cluster_name);
  }
  else
  {
    snprintf(destination, MAXPGPATH, "%s/%s.zip", archive_directory, "zip_archive");
  }

  elog(DEBUG1, "zip_archive destination is %s", destination);

  return archive_directory != NULL && archive_directory[0] != '\0';
}

/*
 * zip_archive_file
 *
 * Archives one file in a ZIP file.
 * Sets comments on the archive.
 * Configures compression method for the file.
 */
static bool
zip_archive_file(const char *file, const char *path)
{
  zip_t        *ziparchive;
  zip_source_t *zipsource;
  int           error;
  int           index;
  int           compression;
  char          comment[200];

  elog(LOG, "archiving \"%s\" via zip_archive", file);

  snprintf(comment, 12, "WAL archive");
  if (cluster_name != NULL && cluster_name[0] != '\0')
  {
    snprintf(comment, strlen(comment)+14+strlen(cluster_name), "%s for %s cluster", comment, cluster_name);
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

  error = zip_set_archive_comment(ziparchive, comment, strlen(comment));
  if (error)
  {
    elog(ERROR, "cannot set archive comment %s: %s\n", comment, zip_strerror(ziparchive));
  }

  // arg3, start at index 0
  // arg4, len 0 for the whole file
  zipsource = zip_source_file(ziparchive, path, 0, 0);
  if (!zipsource)
  {
    elog(ERROR, "cannot source file '%s': %s\n", path, zip_strerror(ziparchive));
  }

  index = zip_file_add(ziparchive, file, zipsource, ZIP_FL_ENC_GUESS);
  if (index < 0)
  {
    elog(ERROR, "cannot add file '%s': %s\n", file, zip_strerror(ziparchive));
  }

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
  error = zip_set_file_compression(ziparchive, index, compression, 1);
  if (error)
  {
    elog(ERROR, "cannot set compression method '%s': %s\n",
      (compression_methods[compression_method]).name,
      zip_strerror(ziparchive));
  }

  error = zip_close(ziparchive);
  if (error)
  {
    elog(ERROR, "cannot close zip archive '%s': %s\n", destination, zip_strerror(ziparchive));
  }

  elog(LOG, "archived \"%s\" via zip_archive", file);

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

/*
 * get_archive_stats
 *
 * Returns statistiques on the ZIP archive:
 * - number of archived WAL
 * - name of first and last WAL
 * - modification date of first and last WAL
 */
Datum
get_archive_stats(PG_FUNCTION_ARGS)
{
  TupleDesc       tupdesc;
  Datum           values[5];
  bool            nulls[5];
  HeapTuple       tuple;
  Datum           result;
  zip_t          *ziparchive;
  int             error;
  zip_int64_t     entries_count;
  struct zip_stat zipstat;

  /* construct tuple descriptor */
  if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
    ereport(ERROR,
        (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
         errmsg("function returning record called in context that cannot accept type record")));

  /* open ZIP archive */
  ziparchive = zip_open(destination, ZIP_CREATE, &error);
  if (!ziparchive)
  {
    zip_error_t ziperror;
    zip_error_init_with_code(&ziperror, error);
    elog(ERROR, "cannot open zip archive '%s': %s\n", destination, zip_error_strerror(&ziperror));
    // ne va pas être exécuté
    zip_error_fini(&ziperror);
  }

  /* get number of files in ZIP archive */
  entries_count = zip_get_num_entries(ziparchive, 0);

  /* column 1 is number of files */
  values[0] = Int64GetDatum(entries_count);
  nulls[0] = false;

  /* column 2 is first WAL file name */
  values[1] = CStringGetTextDatum(zip_get_name(ziparchive, 0, ZIP_FL_ENC_GUESS));
  nulls[1] = false;

  /* column 3 is last WAL file name */
  nulls[2] = entries_count < 0;
  if (!nulls[2])
  {
    values[2] = CStringGetTextDatum(zip_get_name(ziparchive, entries_count-1, ZIP_FL_ENC_GUESS));
  }

  /* column 4 is first WAL modification time */
  zip_stat_index(ziparchive, 0, 0, &zipstat);
  nulls[3] = !(zipstat.valid && ZIP_STAT_MTIME);
  if (!nulls[3])
  {
    values[3] = TimestampTzGetDatum(time_t_to_timestamptz(zipstat.mtime));
  }

  /* column 5 is last WAL modification time */
  zip_stat_index(ziparchive, entries_count-1, 0, &zipstat);
  nulls[4] = !(zipstat.valid && ZIP_STAT_MTIME);
  if (!nulls[4])
  {
    values[4] = TimestampTzGetDatum(time_t_to_timestamptz(zipstat.mtime));
  }

  /* build tuple */
  tuple = heap_form_tuple(tupdesc, values, nulls);
  result = HeapTupleGetDatum(tuple);

  /* return tuple */
  PG_RETURN_DATUM(result);
}

/*
 * get_archived_wals
 *
 * Returns statistiques on each WAL stored in ZIP archive
 */
Datum
get_archived_wals(PG_FUNCTION_ARGS)
{
  FuncCallContext   *funcctx;
  MemoryContext      oldcontext;
  ZipArchiveContext *fctx;
  int                call_cntr;
  int                max_calls;
  int                error;
  struct zip_stat    zipstat;

  if (SRF_IS_FIRSTCALL())
  {
    TupleDesc tupdesc;

    /* create a function context for cross-call persistence */
    funcctx = SRF_FIRSTCALL_INIT();

    /* switch to memory context appropriate for multiple function calls */
    oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

    /* create a user function context for cross-call persistence */
    fctx = (ZipArchiveContext *) palloc(sizeof(ZipArchiveContext));

    /* construct tuple descriptor */
    if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
      ereport(ERROR,
          (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
           errmsg("function returning record called in context that cannot accept type record")));
    fctx->tupdesc = BlessTupleDesc(tupdesc);

    /* open ZIP archive */
    fctx->ziparchive = zip_open(destination, ZIP_CREATE, &error);
    if (!(fctx->ziparchive))
    {
      zip_error_t ziperror;
      zip_error_init_with_code(&ziperror, error);
      elog(ERROR, "cannot open zip archive '%s': %s\n", destination, zip_error_strerror(&ziperror));
      // ne va pas être exécuté
      zip_error_fini(&ziperror);
    }

    /* set max_calls as a count of files in ZIP archive */
    max_calls = zip_get_num_entries(fctx->ziparchive, 0);

    if (max_calls > 0)
    {
      /* got results, keep track of them */
      funcctx->max_calls = max_calls;
      funcctx->user_fctx = fctx;
    }
    else
    {
      /* fast track when no results */
      MemoryContextSwitchTo(oldcontext);
      SRF_RETURN_DONE(funcctx);
    }

    /* switch back to old memory context */
    MemoryContextSwitchTo(oldcontext);
  }

  /* stuff done on every call of the function */
  funcctx = SRF_PERCALL_SETUP();

  /* initialize per-call variables */
  call_cntr = funcctx->call_cntr;
  max_calls = funcctx->max_calls;
  fctx = funcctx->user_fctx;

  /* do while there are more left to send */
  if (call_cntr < max_calls)
  {
    Datum     values[8];
    bool      nulls[8];
    HeapTuple tuple;
    Datum     result;
    int       compression;

    /* get file stats in ZIP archive */
    zip_stat_index(fctx->ziparchive, call_cntr, 0, &zipstat);

    /* column 1 is index number */
    nulls[0] = !(zipstat.valid && ZIP_STAT_INDEX);
    if (!nulls[0])
    {
      values[0] = Int64GetDatum(zipstat.index);
    }

    /* column 2 is WAL file name */
    nulls[1] = !(zipstat.valid && ZIP_STAT_NAME);
    if (!nulls[1])
    {
      values[1] = CStringGetTextDatum(zipstat.name);
    }

    /* column 3 is WAL uncompressed size */
    nulls[2] = !(zipstat.valid && ZIP_STAT_SIZE);
    if (!nulls[2])
    {
      values[2] = Int64GetDatum(zipstat.size);
    }

    /* column 4 is WAL compressed size */
    nulls[3] = !(zipstat.valid && ZIP_STAT_COMP_SIZE);
    if (!nulls[3])
    {
      values[3] = Int64GetDatum(zipstat.comp_size);
    }

    /* column 5 is WAL modification time */
    nulls[4] = !(zipstat.valid && ZIP_STAT_MTIME);
    if (!nulls[4])
    {
      values[4] = TimestampTzGetDatum(time_t_to_timestamptz(zipstat.mtime));
    }

    /* column 6 is WAL CRC */
    nulls[5] = !(zipstat.valid && ZIP_STAT_CRC);
    if (!nulls[5])
    {
      values[5] = Int32GetDatum(zipstat.crc);
    }

    /* column 7 is WAL compression method */
    nulls[6] = !(zipstat.valid && ZIP_STAT_COMP_METHOD);
    if (!nulls[6])
    {
      switch(zipstat.comp_method)
      {
        case ZIP_CM_STORE:
          compression = UNCOMPRESSED;
          break;
        case ZIP_CM_BZIP2:
          compression = BZIP2;
          break;
        case ZIP_CM_DEFLATE:
          compression = ZLIB;
          break;
        case ZIP_CM_XZ:
          compression = XZ;
          break;
        case ZIP_CM_ZSTD:
          compression = ZSTD;
          break;
      }
      values[6] = CStringGetTextDatum((compression_methods[compression]).name);
    }

    /* column 8 is WAL encryption method */
    nulls[7] = !(zipstat.valid && ZIP_STAT_ENCRYPTION_METHOD);
    if (!nulls[7])
    {
      values[7] = Int16GetDatum(zipstat.encryption_method);
    }

    /* build tuple */
    tuple = heap_form_tuple(fctx->tupdesc, values, nulls);
    result = HeapTupleGetDatum(tuple);

    /* return tuple */
    SRF_RETURN_NEXT(funcctx, result);
  }

  /* all done */
  SRF_RETURN_DONE(funcctx);
}
