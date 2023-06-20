\echo Ne pas exécuter ce script, mais passer par CREATE EXTENSION

CREATE OR REPLACE FUNCTION get_libzip_version()
RETURNS text
AS '$libdir/zip_archive', 'get_libzip_version'
LANGUAGE C;

CREATE OR REPLACE FUNCTION get_archive_stats(
  OUT entries_count integer,
  OUT first_wal_name text,
  OUT last_wal_name text,
  OUT first_wal_mtime timestamptz,
  OUT last_wal_mtime timestamptz)
AS '$libdir/zip_archive', 'get_archive_stats'
LANGUAGE C;
