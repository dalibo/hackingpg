\echo Ne pas exécuter ce script, mais passer par CREATE EXTENSION

CREATE OR REPLACE FUNCTION division_sans_erreur(numeric, numeric)
RETURNS numeric
LANGUAGE sql
AS $$
SELECT CASE WHEN $2=0 THEN NULL ELSE $1/$2 END;
$$;

CREATE OPERATOR //
  (FUNCTION=division_sans_erreur,
   LEFTARG=numeric,
   RIGHTARG=numeric);
