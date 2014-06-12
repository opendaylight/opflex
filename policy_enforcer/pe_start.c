/*
* Copyright (c) 2014 Cisco Systems, Inc. and others.
*               All rights reserved.
*
* This program and the accompanying materials are made available under
* the terms of the Eclipse Public License v1.0 which accompanies this
* distribution, and is available at
* http://www.eclipse.org/legal/epl-v10.html
*/


#include <stdbool.h>
#include <stdint.h>

#include "dbug.h"
#include "pag-thread.h"
#include "policy_enforcer.h"

/* private */
static pe_state_t pe_current_state = STOPPED;
static void pe_set_state(pe_state_t);


/* ============================================================
 *
 * \brief pe_start()
 *        Remove an entry from the ring buffer.
 *
 * @param[]
 *          none
 *
 * \return { bool - true or false }
 *
 **/
bool pe_start(void) {

    pe_set_state(INITIALIZING);
    /* call function to collect data (pe_scan.c) */

    pe_set_state(DATA_PUSH);
    /* push data up to MODB (pe_push.c)*/

    /* start monitoring thread (pe_monitor.c)*/
    pe_set_state(MONITORING);

    /* initialize ring buffer/thread pool (pe_workers.c)*/
    pe_set_state(WORKERS_RUNNING);

    /* initialize translator (pe_translator.c)*/
    pe_set_state(READY);

    /* pthread_join calls */
    pe_set_state(TERMINATING);

    /* call clean up routing */
    return(true);

}

/* ============================================================
 *
 * \brief pe_get_state()
 *        Return the current state of PE.
 *
 * @param[]
 *          none
 *
 * \return { current state }
 *
 **/
pe_state_t pe_get_state() {
   return(pe_current_state);
}

/* ============================================================
 *
 * \brief pe_initialize()
 *        Initialization routinge called from pagend.c
 *
 * @param[]
 *          none
 *
 * \return { true or false }
 *
 **/
bool pe_initialize() {
    bool retval = true;

    /* initialize configuration params */

    /* start pe thread by invoking pe_start via pxthread_create() */

    return(retval);
}

/* private function(s) */
static void pe_set_state(pe_state_t state) {
   pe_current_state = state;
}
