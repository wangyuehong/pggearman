/*
 * Gearman PostgreSQL Functions
 * Copyright (C) 2009 Eric Day, Selena Deckelmann
 *
 * Use and distribution licensed under the BSD license.  See
 * the COPYING file in the parent directory for full text.
 */

#include "postgres.h"
#include "fmgr.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "utils/guc.h"
#include "utils/elog.h"

#include "pggearman.h"

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

/*
 * PostgreSQL <= 8.2 provided VARATT_SIZEP(p), but later versions use
 * SET_VARSIZE() (and also define the VARATT_SIZEP_DEPRECATED
 * symbol). Therefore, define the latter in terms of the former if
 * necessary.
 */
#ifndef SET_VARSIZE
#define SET_VARSIZE(ptr, size) VARATT_SIZEP(ptr) = (size)
#endif

enum
{
     pg_gman_do,
     pg_gman_do_high,
     pg_gman_do_low,
     pg_gman_do_background,
     pg_gman_do_high_background,
     pg_gman_do_low_background
};


/* Per-backend global state. */
struct gearman_global
{
    gearman_client_st *client;
    text *result;
    MemoryContext pg_ctx;
};

static struct gearman_global globals;

char *gearman_servers;

static Datum gman_run_cmd(int type, PG_FUNCTION_ARGS);
static void *gman_malloc(size_t size, void *arg);
static void gman_free(void *ptr, void *arg);

static GucStringAssignHook *assign_gearman_servers_guc(const char *newval, 
                                                       bool doit,
                                                       GucSource source);
static GucShowHook *show_gearman_servers_guc(void);
static gearman_return_t do_servers_add(char *host_str);

#if PG_VERSION_NUM >= 90600
/* GetConfigOptionByName has a new signature from 9.6 on */
#define GetConfigOptionByName(name, varname) GetConfigOptionByName(name, varname, false)
#endif  /* PG_VERSION_NUM */

void _PG_init(void)
{
    MemoryContext old_ctx;
    char *new_value = NULL;

    globals.pg_ctx = AllocSetContextCreate(TopMemoryContext,
                                            "pggearman global context",
                                            ALLOCSET_SMALL_MINSIZE,
                                            ALLOCSET_SMALL_INITSIZE,
                                            ALLOCSET_SMALL_MAXSIZE);

    old_ctx = MemoryContextSwitchTo(globals.pg_ctx);

    PG_TRY();
    {
        new_value = GetConfigOptionByName("pggearman.default_servers", NULL);
    }
    PG_CATCH();
    {
        DefineCustomStringVariable("pggearman.default_servers",
                                   "Comma-separated list of gearman servers "
                                   "to connect to.",
                                   "Specified as a comma-separated list of "
                                   "host:port (port is optional).",
                                   &gearman_servers,
#if defined(PG_VERSION_NUM) && (80400 <= PG_VERSION_NUM)
                                   NULL,
#endif
                                   PGC_USERSET,
#if defined(PG_VERSION_NUM) && (80400 <= PG_VERSION_NUM)
                                   GUC_LIST_INPUT,
#endif

#if defined(PG_VERSION_NUM) && (90100 <= PG_VERSION_NUM)
                                   NULL, // GucStringCheckHook parameter for postgres 9.1+ <<== here additional parameter
#endif
                                   (GucStringAssignHook) assign_gearman_servers_guc,
                                   (GucShowHook) show_gearman_servers_guc);
    }
    PG_END_TRY();

    globals.client = gearman_client_create(NULL);
    gearman_client_set_workload_malloc_fn(globals.client, gman_malloc, NULL);
    gearman_client_set_workload_free_fn(globals.client, gman_free, NULL);

    if (new_value != NULL) {
        do_servers_add(new_value);
    }

    MemoryContextSwitchTo(old_ctx);
}

GucShowHook *show_gearman_servers_guc(void)
{
    return (GucShowHook *) gearman_servers;
}

GucStringAssignHook *assign_gearman_servers_guc(const char *newval,
                                                bool doit, GucSource source)
{
    bool ret;
    ret = do_servers_add((char *) newval);
    if (ret)
        return (GucStringAssignHook *) newval;
    else
        return NULL;
}

static gearman_return_t do_servers_add(char *host_str)
{
    gearman_return_t ret;
    MemoryContext old_ctx;

    old_ctx = MemoryContextSwitchTo(globals.pg_ctx);
    gearman_client_remove_servers(globals.client);
#if defined(PG_VERSION_NUM) && (90100 <= PG_VERSION_NUM)
    // for version 9.1+ u cannot call gearman_client_add_servers if host is null
    if(host_str) ret = gearman_client_add_servers(globals.client, host_str);
    else ret = GEARMAN_SUCCESS; // ret success anyway, but dont call
#else
    ret = gearman_client_add_servers(globals.client, host_str);
#endif
    MemoryContextSwitchTo(old_ctx);

    if (ret != GEARMAN_SUCCESS)
        elog(ERROR, "%s", gearman_client_error(globals.client));

    PG_RETURN_BOOL(ret == GEARMAN_SUCCESS);
}

/*
 * This is called when we're being unloaded from a process. Note that
 * this only happens when we're being replaced by a LOAD (e.g. it
 * doesn't happen on process exit), so we can't depend on it being
 * called.
 */
void _PG_fini(void)
{
    gearman_client_free(globals.client);
    MemoryContextDelete(globals.pg_ctx);
}


Datum gman_servers_set(PG_FUNCTION_ARGS)
{
    text *get_servers = PG_GETARG_TEXT_P(0);
    char *servers;
    
    servers = DatumGetCString(DirectFunctionCall1(textout, PointerGetDatum(get_servers)));

    return do_servers_add(servers);
}


Datum gman_do(PG_FUNCTION_ARGS)
{
    PG_RETURN_TEXT_P(gman_run_cmd(pg_gman_do, fcinfo)); 
}

Datum gman_do_high(PG_FUNCTION_ARGS)
{
    PG_RETURN_TEXT_P(gman_run_cmd(pg_gman_do_high, fcinfo)); 
}

Datum gman_do_low(PG_FUNCTION_ARGS)
{
    PG_RETURN_TEXT_P(gman_run_cmd(pg_gman_do_low, fcinfo)); 
}

Datum gman_do_background(PG_FUNCTION_ARGS)
{
    PG_RETURN_TEXT_P(gman_run_cmd(pg_gman_do_background, fcinfo)); 
}

Datum gman_do_low_background(PG_FUNCTION_ARGS)
{
    PG_RETURN_TEXT_P(gman_run_cmd(pg_gman_do_low_background, fcinfo)); 
}

Datum gman_do_high_background(PG_FUNCTION_ARGS)
{
    PG_RETURN_TEXT_P(gman_run_cmd(pg_gman_do_high_background, fcinfo)); 
}

Datum gman_run_cmd(int type, PG_FUNCTION_ARGS) {

    text *get_function;
    char *function;
    text *get_workload;
    char *workload;
    size_t workload_length;
    text *result;
    size_t result_length;
    gearman_return_t ret;
    char job_handle[GEARMAN_JOB_HANDLE_SIZE];
    int background = 0;

    if (PG_ARGISNULL(0))
        elog(ERROR, "gearman function cannot be NULL");

    if (PG_ARGISNULL(1))
        elog(ERROR, "gearman workload cannot be NULL");

    get_function = PG_GETARG_TEXT_P(0);
    function = DatumGetCString(DirectFunctionCall1(textout, PointerGetDatum(get_function)));

    get_workload = PG_GETARG_TEXT_P(1);
    workload = DatumGetCString(DirectFunctionCall1(textout, PointerGetDatum(get_workload)));
    workload_length = strlen(workload);

    switch (type)
    {
        case pg_gman_do: 
            (void)gearman_client_do(globals.client, function, NULL, workload,
                                    workload_length, &result_length, &ret);
            break;

        case pg_gman_do_high:
            (void)gearman_client_do_high(globals.client, function, NULL,
                                         workload, workload_length,
                                         &result_length, &ret);
            break;

        case pg_gman_do_low:
            (void)gearman_client_do_low(globals.client, function, NULL,
                                        workload, workload_length,
                                        &result_length, &ret);
            break;

        case pg_gman_do_background:
            ret = gearman_client_do_background(globals.client, function, NULL,
                                               workload, workload_length,
                                               job_handle);
            background = 1;
            break;

        case pg_gman_do_low_background:
            ret = gearman_client_do_low_background(globals.client, function,
                                                   NULL, workload,
                                                   workload_length, job_handle);
            background = 1;
            break;

        case pg_gman_do_high_background:
            ret = gearman_client_do_high_background(globals.client, function,
                                                    NULL, workload,
                                                    workload_length,
                                                    job_handle);
            background = 1;
            break;

        default:
            elog(ERROR, "Gearman function type is undefined! Scary. Bailing out.");
    }

    if (ret != GEARMAN_SUCCESS)
        elog(ERROR, "%s", gearman_client_error(globals.client));

    if (background) 
    {
        result_length = strlen(job_handle);
        result = (text *) palloc(result_length + VARHDRSZ);
        SET_VARSIZE(result, result_length + VARHDRSZ);
        memcpy(VARDATA(result), job_handle, result_length);
    }
    else if (globals.result == NULL)
        PG_RETURN_NULL();
    else
    {
        result = globals.result;
        globals.result = NULL;
    }

    PG_RETURN_TEXT_P(result);
}

static void *gman_malloc(size_t size, void *arg)
{
    globals.result = (text *) palloc(size + VARHDRSZ);
    SET_VARSIZE(globals.result, size + VARHDRSZ);
    return VARDATA(globals.result);
}

static void gman_free(void *ptr, void *arg)
{
    pfree(globals.result);
    globals.result = NULL;
}
