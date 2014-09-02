/*
* Copyright (c) 2014 Cisco Systems, Inc. and others.
*               All rights reserved.
*
* This program and the accompanying materials are made available under
* the terms of the Eclipse Public License v1.0 which accompanies this
* distribution, and is available at
* http://www.eclipse.org/legal/epl-v10.html
*/

/*
 * This module translates MODB data into an OVS command list, which
 * is then passed to the pe_command module.
 */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "vlog.h"
#include "vlog-opflex.h"

VLOG_DEFINE_THIS_MODULE(pe_translator);

/* ============================================================
 *
 * \brief pe_translate()
 *        Incoming item to be translated. Item is inspected and
 *        appropriate module is called for translation.
 *
 * @param[void *]
 *          void * to the item
 *
 * \return { void }
 *
 **/
void pe_translate(void *item) {

    VLOG_ENTER(__func__);

    VLOG_INFO("Received work item %p\n",item);
/* NB: this module is multi-threaded, but it will pass its
 *     work in the form of a pe_ovs_command_t *list into
 *     a ring buffer for the pe_command module to process.
 *     The pe_command_module is a single thread because it
 *     is executing commands that must be processed sequentially.
 */

    VLOG_LEAVE(__func__);
}
