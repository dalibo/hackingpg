\echo Ne pas exécuter ce script, mais passer par CREATE EXTENSION

CREATE OR REPLACE FUNCTION incremente(int)
RETURNS int
AS '$libdir/monextension', 'incremente'
STRICT
LANGUAGE C;
