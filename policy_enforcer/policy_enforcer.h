/*
* Copyright (c) 2014 Cisco Systems, Inc. and others.
*               All rights reserved.
*
* This program and the accompanying materials are made available under
* the terms of the Eclipse Public License v1.0 which accompanies this
* distribution, and is available at
* http://www.eclipse.org/legal/epl-v10.html
*/

#ifndef POLICY_ENFORCER_H
#define POLICY_ENFORCER_H 1

#include <stdbool.h>

#include "config-file.h"

/* initialization for configuration data */
#define PE_INITIALIZE {"pe_initialize",pe_initialize}
#define PE_SECTION "policy_enforcer"

/* data */
typedef enum {
    STOPPED,
    INITIALIZING,
    DATA_PUSH,
    MONITORING,
    WORKERS_RUNNING,
    READY,
    TERMINATING
} pe_state_t;

static struct option_ele pe_config_defaults[] = {
    {PE_SECTION, "pe_ring_buffer_length", "1000"},
    {PE_SECTION, "pe_test_pop_thread_count", "20"},
    {PE_SECTION, "pe_test_max_pop_count", "50"},
    {PE_SECTION, "pe_max_worker_count", "50"},
    {NULL, NULL, NULL}
};

/* prototypes */
void *pe_start(void *);
pe_state_t pe_get_state(void);
extern bool pe_initialize(void);

#endif //POLICY_ENFORCER_H
