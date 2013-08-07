# Gearman PostgreSQL Functions
# Copyright (C) 2009 Eric Day, Selena Deckelmann
#
# Use and distribution licensed under the BSD license.  See
# the COPYING file in the parent directory for full text.

MODULE_big = pggearman
PGMC_VERSION = 0.1

OBJS = pggearman.o
DATA_built = $(MODULE_big).sql
SHLIB_LINK = -lgearman

PGXS := $(shell pg_config --pgxs)
include $(PGXS)
