/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */
#ifndef POL_MGMT_H
#define POL_MGMT_H 1

#include "config-file.h"
#include "ovs-thread.h"
#include "shash.h"

#define PM_SECTION "policy-management"
#define PM_FNAME "pol-mgmt.dat"


/* The initializer for the INIT_JMP_TABLE, see pagentd.c 
 */
#define PM_INITIALIZE {"pm_init", pm_initialize}

/* global dcls */
struct option_ele pm_config_defaults[];

/* 
 * Protos
 */
extern bool pm_initialize(void);
extern bool pm_crash_recovery(const char *dbfile);
extern bool pm_is_initialized(void);
extern void pe_cleanup(void);

#endif /* POL_MGMT_H */
