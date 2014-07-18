/*
* Copyright (c) 2014 Cisco Systems, Inc. and others.
*               All rights reserved.
*
* This program and the accompanying materials are made available under
* the terms of the Eclipse Public License v1.0 which accompanies this
* distribution, and is available at
* http://www.eclipse.org/legal/epl-v10.html
*/

#ifndef PEOVS_MODB_INTERFACE_H
#define PEOVS_MODB_INTERFACE_H 1

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "config-file.h"

/* protos */
void pe_set_modb_iface_pull_quit(bool);
void *peovs_modb_interface_pull(void *args);


#endif //PEOVS_MODB_INTERFACE_H
