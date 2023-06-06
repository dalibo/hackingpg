\echo Ne pas exécuter ce script, mais passer par CREATE EXTENSION

CREATE OR REPLACE FUNCTION incremente(int)
RETURNS int
LANGUAGE sql
AS 'SELECT $1+1';
