/*
 * Gearman PostgreSQL Functions
 * Copyright (C) 2009 Eric Day, Selena Deckelmann
 *
 * Use and distribution licensed under the BSD license.  See
 * the COPYING file in the parent directory for full text.
 */

#ifndef PGGEARMAN_H
#define PGGEARMAN_H

#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION
#include <libgearman/gearman.h>
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION

void _PG_init(void);
void _PG_fini(void);

Datum gman_servers_set(PG_FUNCTION_ARGS);
Datum gman_do(PG_FUNCTION_ARGS);
Datum gman_do_high(PG_FUNCTION_ARGS);
Datum gman_do_low(PG_FUNCTION_ARGS);
Datum gman_do_background(PG_FUNCTION_ARGS);
Datum gman_do_high_background(PG_FUNCTION_ARGS);
Datum gman_do_low_background(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(gman_servers_set);
PG_FUNCTION_INFO_V1(gman_do);
PG_FUNCTION_INFO_V1(gman_do_high);
PG_FUNCTION_INFO_V1(gman_do_low);
PG_FUNCTION_INFO_V1(gman_do_background);
PG_FUNCTION_INFO_V1(gman_do_high_background);
PG_FUNCTION_INFO_V1(gman_do_low_background);

#endif
