#include "postgres.h"
#include "fmgr.h"

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(incremente);

Datum
incremente(PG_FUNCTION_ARGS)
{
  int32   valeur = PG_GETARG_INT32(0);
  PG_RETURN_INT32(valeur + 1);
}
