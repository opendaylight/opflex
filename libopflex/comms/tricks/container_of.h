/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef _NOIRO__TRICKS__CONTAINER_OF_H
#define _NOIRO__TRICKS__CONTAINER_OF_H

#define container_of(ptr, type, member) \
  ((type *) ((char *) (ptr) - offsetof(type, member)))

#endif/*_NOIRO__TRICKS__CONTAINER_OF_H*/
