/*
 * Copyright (c) 2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "FlowConstants.h"
#include "FlowUtils.h"
#include "RangeMask.h"
#include "FlowBuilder.h"
#include "ovs.h"

#include <modelgbp/l2/EtherTypeEnumT.hpp>
#include <modelgbp/l4/TcpFlagsEnumT.hpp>
#include <modelgbp/arp/OpcodeEnumT.hpp>

#include <boost/foreach.hpp>

#include <vector>

namespace ovsagent {
namespace flowutils {

using std::vector;
using modelgbp::gbpe::L24Classifier;

void match_group(FlowBuilder& f, uint16_t prio,
                 uint32_t svnid, uint32_t dvnid) {
    f.priority(prio);
    if (svnid != 0)
        f.reg(0, svnid);
    if (dvnid != 0)
        f.reg(2, dvnid);
}

FlowEntryPtr default_out_flow() {
    return FlowBuilder()
        .priority(1)
        .metadata(0, flow::meta::out::MASK)
        .action().outputReg(MFF_REG7)
        .parent().build();
}

static void match_protocol(FlowBuilder& f, L24Classifier *classifier) {
    using namespace modelgbp::arp;
    using namespace modelgbp::l2;

    uint8_t arpOpc =
            classifier->getArpOpc(OpcodeEnumT::CONST_UNSPECIFIED);
    uint16_t ethT =
            classifier->getEtherT(EtherTypeEnumT::CONST_UNSPECIFIED);
    if (arpOpc != OpcodeEnumT::CONST_UNSPECIFIED) {
        f.proto(arpOpc);
    }
    if (ethT != EtherTypeEnumT::CONST_UNSPECIFIED) {
        f.ethType(ethT);
    }
    if (classifier->isProtSet()) {
        f.proto(classifier->getProt().get());
    }
}

static void match_tcp_flags(FlowBuilder& f, uint32_t tcpFlags) {
    using namespace modelgbp::l4;
    ovs_be16 flags = 0;
    if (tcpFlags & TcpFlagsEnumT::CONST_FIN) flags |= 0x01;
    if (tcpFlags & TcpFlagsEnumT::CONST_SYN) flags |= 0x02;
    if (tcpFlags & TcpFlagsEnumT::CONST_RST) flags |= 0x04;
    if (tcpFlags & TcpFlagsEnumT::CONST_ACK) flags |= 0x10;
    f.tcpFlags(flags, flags);
}

const uint16_t MAX_POLICY_RULE_PRIORITY = 8192;     // arbitrary

void add_classifier_entries(L24Classifier *clsfr,
                            bool allow, uint8_t nextTable,
                            uint16_t priority, uint64_t cookie,
                            uint32_t svnid, uint32_t dvnid,
                            /* out */ FlowEntryList& entries) {
    using namespace modelgbp::l4;

    ovs_be64 ckbe = htonll(cookie);
    MaskList srcPorts;
    MaskList dstPorts;
    RangeMask::getMasks(clsfr->getSFromPort(), clsfr->getSToPort(), srcPorts);
    RangeMask::getMasks(clsfr->getDFromPort(), clsfr->getDToPort(), dstPorts);

    /* Add a "ignore" mask to empty ranges - makes the loop later easy */
    if (srcPorts.empty()) {
        srcPorts.push_back(Mask(0x0, 0x0));
    }
    if (dstPorts.empty()) {
        dstPorts.push_back(Mask(0x0, 0x0));
    }

    vector<uint32_t> tcpFlagsVec;
    uint32_t tcpFlags = clsfr->getTcpFlags(TcpFlagsEnumT::CONST_UNSPECIFIED);
    if (tcpFlags & TcpFlagsEnumT::CONST_ESTABLISHED) {
        tcpFlagsVec.push_back(0 + TcpFlagsEnumT::CONST_ACK);
        tcpFlagsVec.push_back(0 + TcpFlagsEnumT::CONST_RST);
    } else {
        tcpFlagsVec.push_back(tcpFlags);
    }

    BOOST_FOREACH (const Mask& sm, srcPorts) {
        BOOST_FOREACH(const Mask& dm, dstPorts) {
            BOOST_FOREACH(uint32_t flagMask, tcpFlagsVec) {
                FlowBuilder f;
                f.cookie(ckbe);

                flowutils::match_group(f, priority, svnid, dvnid);
                match_protocol(f, clsfr);
                if (tcpFlags != TcpFlagsEnumT::CONST_UNSPECIFIED)
                    match_tcp_flags(f, flagMask);

                f.tpSrc(sm.first, sm.second)
                    .tpDst(dm.first, dm.second);

                if (allow) {
                    f.action().go(nextTable);
                }
                entries.push_back(f.build());
            }
        }
    }
}

} // namespace flowutils
} // namespace ovsagent
