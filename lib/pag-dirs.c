/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */


#include <config.h>
#include "dirs.h"
#include <stdlib.h>
#include "util.h"
#include "debug.h"
#include "config-file.h"

static char *section = "global";

const char *pag_sysconfdir(void)
{
    return (conf_get_value(section, "sysconfig"));
}

const char *pag_pkgdatadir(void)
{
    return (conf_get_value(section, "pkgdatadir"));
}

const char *pag_rundir(void)
{
    return (conf_get_value(section, "rundir"));
}

const char *pag_logdir(void)
{
    return (conf_get_value(section, "logdir"));
}

const char *pag_dbdir(void)
{
    return (conf_get_value(section, "dbdir"));
}

const char *pag_bindir(void)
{
    return (conf_get_value(section, "bindir"));
}
