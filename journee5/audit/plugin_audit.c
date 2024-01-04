#include "postgres.h"

#include "catalog/pg_type.h"

#include "replication/logical.h"
#include "replication/origin.h"

#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/rel.h"

PG_MODULE_MAGIC;

typedef struct
{
	MemoryContext context;
} AuditDecodingData;

/*
 * Maintain the per-transaction level variables to track whether the
 * transaction and or streams have written any changes. In streaming mode the
 * transaction can be decoded in streams so along with maintaining whether the
 * transaction has written any changes, we also need to track whether the
 * current stream has written any changes. This is required so that if user
 * has requested to skip the empty transactions we can skip the empty streams
 * even though the transaction has written some changes.
 */
typedef struct
{
	bool		xact_wrote_changes;
	bool		stream_wrote_changes;
} AuditDecodingTxnData;

static void pg_decode_startup(LogicalDecodingContext *ctx,
               OutputPluginOptions *opt,
							 bool is_init);
static void pg_decode_shutdown(LogicalDecodingContext *ctx);
static void pg_decode_begin_txn(LogicalDecodingContext *ctx,
							ReorderBufferTXN *txn);
static void pg_decode_commit_txn(LogicalDecodingContext *ctx,
							ReorderBufferTXN *txn, XLogRecPtr commit_lsn);
static void pg_decode_change(LogicalDecodingContext *ctx,
							 ReorderBufferTXN *txn, Relation relation,
							 ReorderBufferChange *change);

void
_PG_init(void)
{
	/* other plugins can perform things here */
}

/* specify output plugin callbacks */
void
_PG_output_plugin_init(OutputPluginCallbacks *cb)
{
	cb->startup_cb = pg_decode_startup;
	cb->begin_cb = pg_decode_begin_txn;
	cb->change_cb = pg_decode_change;
	cb->commit_cb = pg_decode_commit_txn;
	cb->shutdown_cb = pg_decode_shutdown;
}


/* initialize this plugin */
static void
pg_decode_startup(LogicalDecodingContext *ctx, OutputPluginOptions *opt,
				  bool is_init)
{
	AuditDecodingData *data;

	data = palloc0(sizeof(AuditDecodingData));
	data->context = AllocSetContextCreate(ctx->context,
										  "plugin_audit context",
										  ALLOCSET_DEFAULT_SIZES);
	ctx->output_plugin_private = data;

	opt->output_type = OUTPUT_PLUGIN_TEXTUAL_OUTPUT;
	opt->receive_rewrites = false;

  /* we don't have any options yet, but that would be the place to do it */
}

/* cleanup this plugin's resources */
static void
pg_decode_shutdown(LogicalDecodingContext *ctx)
{
	AuditDecodingData *data = ctx->output_plugin_private;

	/* cleanup our own resources via memory context reset */
	MemoryContextDelete(data->context);
}

/* BEGIN callback */
static void
pg_decode_begin_txn(LogicalDecodingContext *ctx, ReorderBufferTXN *txn)
{
}

/* COMMIT callback */
static void
pg_decode_commit_txn(LogicalDecodingContext *ctx, ReorderBufferTXN *txn,
					 XLogRecPtr commit_lsn)
{
}

/*
 * callback for individual changed tuples
 */
static void
pg_decode_change(LogicalDecodingContext *ctx, ReorderBufferTXN *txn,
				 Relation relation, ReorderBufferChange *change)
{
	AuditDecodingData *data;
	MemoryContext old;
	Form_pg_class class_form;

	data = ctx->output_plugin_private;

	old = MemoryContextSwitchTo(data->context);

	OutputPluginPrepareWrite(ctx, true);

	class_form = RelationGetForm(relation);
	appendStringInfoString(ctx->out,
		quote_qualified_identifier(get_namespace_name(get_rel_namespace(RelationGetRelid(relation))),
		class_form->relrewrite ?
			get_rel_name(class_form->relrewrite) :
			NameStr(class_form->relname)));

	MemoryContextSwitchTo(old);
	MemoryContextReset(data->context);

	OutputPluginWrite(ctx, true);
}

