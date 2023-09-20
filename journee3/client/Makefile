PROGRAMS = client dropdb

PG_CPPFLAGS = -I$(libpq_srcdir)
PG_LIBS = $(libpq_pgport)

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

all: $(PROGRAMS)

%: %.o $(WIN32RES)
	$(CC) $(CFLAGS) $^ $(libpq_pgport) $(LDFLAGS) -lpgfeutils -lpgcommon -o $@$(X)

client: client.o
dropdb: dropdb.o
