/*
* Copyright (c) 2014 Cisco Systems, Inc. and others.
*               All rights reserved.
*
* This program and the accompanying materials are made available under
* the terms of the Eclipse Public License v1.0 which accompanies this
* distribution, and is available at
* http://www.eclipse.org/legal/epl-v10.html
*/

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <errno.h>

//#define DBUG_OFF 1

#include "pag-thread.h"
#include "dbug.h"
#include "util.h"
#include "ring_buffer.h"

void pe_workers_init() {

    DBUG_PUSH("d:F:i:L:n:t");
    DBUG_PROCESS("pe_workers");
    DBUG_ENTER("pe_workers_init");

    DBUG_LEAVE;
}
