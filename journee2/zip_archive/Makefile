EXTENSION = zip_archive
MODULE_big = zip_archive
OBJS = zip_archive.o
DATA = zip_archive--1.0.sql
PGFILEDESC = "zip_archive - zip archive module"
SHLIB_LINK = -lzip

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
