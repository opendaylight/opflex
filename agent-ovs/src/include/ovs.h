/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

/* Convenience include file that includes actual libopenvswitch headers */

#ifndef OVSAGENT_OVS_H_
#define OVSAGENT_OVS_H_

#include <boost/static_assert.hpp>
#include <boost/shared_ptr.hpp>

#ifdef __GNUG__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-fpermissive"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <openvswitch/vlog.h>
#include <openvswitch/lib/dirs.h>
#include <openvswitch/vconn.h>
#include <openvswitch/lib/ofpbuf.h>
#include <openvswitch/lib/ofp-msgs.h>
#include <openvswitch/lib/flow.h>
#include <openvswitch/lib/match.h>
#include <openvswitch/lib/ofp-actions.h>
#include <openvswitch/lib/socket-util.h>
#include <openvswitch/lib/ofp-util.h>
#include <openvswitch/lib/poll-loop.h>
#include <openvswitch/lib/ofp-print.h>
#include <openvswitch/lib/dynamic-string.h>
#include <openvswitch/list.h>
#include <openvswitch/lib/json.h>
#include <openvswitch/lib/jsonrpc.h>
#include <openvswitch/lib/stream.h>
#include "openvswitch/lib/ofp-parse.h"

#ifdef __cplusplus
}
#endif

#ifdef __GNUG__
#pragma GCC diagnostic pop
#endif

#endif /* OVSAGENT_OVS_H_ */
