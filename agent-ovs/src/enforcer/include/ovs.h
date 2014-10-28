/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

/* Convenience include file that includes actual libopenvswitch headers */

#ifndef OVS_H_
#define OVS_H_

#include <boost/static_assert.hpp>

#ifdef __cplusplus
extern "C" {
#endif

#include <ovs/lib/dirs.h>
#include <ovs/lib/vconn.h>
#include <ovs/lib/rconn.h>
#include <ovs/lib/ofpbuf.h>
#include <ovs/lib/ofp-msgs.h>
#include <ovs/lib/flow.h>
#include <ovs/lib/match.h>
#include <ovs/lib/ofp-actions.h>
#include <ovs/lib/socket-util.h>
#include <ovs/lib/ofp-util.h>

#ifdef __cplusplus
}
#endif

#endif /* OVS_H_ */
