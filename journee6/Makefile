EXTENSION    = cpg
EXTVERSION   = 0.1

PG_CONFIG ?= pg_config

OBJS       = cpg.o
MODULE_big = cpg

all:

ifdef NO_PGXS
subdir = contrib/$(MODULE)
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
else
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
endif

SHLIB_LINK += -lcpg
