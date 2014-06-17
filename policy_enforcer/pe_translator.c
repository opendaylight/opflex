/*
* Copyright (c) 2014 Cisco Systems, Inc. and others.
*               All rights reserved.
*
* This program and the accompanying materials are made available under
* the terms of the Eclipse Public License v1.0 which accompanies this
* distribution, and is available at
* http://www.eclipse.org/legal/epl-v10.html
*/
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#define DBUG_OFF 1

#include "dbug.h"
#include "vlog.h"

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
    static char mod[] = "pe_translate";

    DBUG_ENTER(mod);

    DBUG_PRINT("DEBUG",("Received work item %p\n",item));
    VLOG_INFO("Received work item %p\n",item);

    DBUG_LEAVE;

}
