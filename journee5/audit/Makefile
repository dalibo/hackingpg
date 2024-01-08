# Plugin de décodage logique
MODULES = plugin_audit
PGFILEDESC = "Logical decoding plugin for auditing purpose"

# Client utilisant ce plugin
PROGRAMS = audit
PG_CPPFLAGS = -I$(libpq_srcdir)
PG_LIBS = $(libpq_pgport)

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

all: $(PROGRAMS)

%: %.o $(WIN32RES)
	$(CC) $(CFLAGS) $^ $(libpq_pgport) $(LDFLAGS) -lpgfeutils -lpgcommon -lpgport -o $@$(X)

audit: audit.o
