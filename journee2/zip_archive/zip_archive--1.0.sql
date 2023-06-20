\echo Ne pas exécuter ce script, mais passer par CREATE EXTENSION

CREATE OR REPLACE FUNCTION get_libzip_version()
RETURNS text
AS '$libdir/zip_archive', 'get_libzip_version'
LANGUAGE C;
