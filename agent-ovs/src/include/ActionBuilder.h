/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef _OPFLEX_ENFORCER_FLOW_ACTIONBUILDER_H_
#define _OPFLEX_ENFORCER_FLOW_ACTIONBUILDER_H_

#include "ovs.h"

struct ofputil_flow_stats;

namespace opflex {
namespace enforcer {
namespace flow {

/**
 * Class to help construct the actions part of a table entry incrementally.
 */
class ActionBuilder {
public:
    ActionBuilder();
    ~ActionBuilder();

    /**
     * Construct and install the action structure to 'dstEntry'
     */
    void Build(ofputil_flow_stats *dstEntry);

    void SetRegLoad(mf_field_id regId, uint32_t regValue);
    void SetRegLoad(mf_field_id regId, const uint8_t *macValue);
    void SetRegMove(mf_field_id srcRegId, mf_field_id dstRegId);
    void SetEthSrcDst(const uint8_t *srcMac, const uint8_t *dstMac);
    void SetDecNwTtl();
    void SetGotoTable(uint8_t tableId);
    void SetOutputToPort(uint32_t port);
    void SetOutputReg(mf_field_id srcRegId);

private:
    struct ofpbuf buf;
};

}   // namespace flow
}   // namespace enforcer
}   // namespace opflex




#endif /* _OPFLEX_ENFORCER_FLOW_ACTIONBUILDER_H_ */
