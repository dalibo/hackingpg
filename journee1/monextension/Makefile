EXTENSION = monextension
MODULE_big = monextension
OBJS = monextension.o
DATA = monextension--1.0.sql
DATA += monextension--1.0--2.0.sql
DATA += monextension--2.0--1.0.sql
DATA += monextension--2.0--3.0.sql
REGRESS = incremente

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
