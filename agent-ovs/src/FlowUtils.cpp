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
#include "ActionBuilder.h"
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

void set_match_group(FlowEntry& fe, uint8_t tableId,
                     uint16_t prio, uint32_t svnid,
                     uint32_t dvnid) {
    fe.entry->table_id = tableId;
    fe.entry->priority = prio;
    if (svnid != 0)
        match_set_reg(&fe.entry->match, 0 /* REG0 */, svnid);
    if (dvnid != 0)
        match_set_reg(&fe.entry->match, 2 /* REG2 */, dvnid);

}

FlowEntryPtr default_out_flow(uint8_t tableId) {
    FlowEntryPtr outputReg(new FlowEntry());
    outputReg->entry->table_id = tableId;
    outputReg->entry->priority = 1;
    match_set_metadata_masked(&outputReg->entry->match,
                              0, htonll(flow::meta::out::MASK));
    ActionBuilder ab;
    ab.SetOutputReg(MFF_REG7);
    ab.Build(outputReg->entry);
    return outputReg;
}

static void set_entry_protocol(FlowEntry& fe, L24Classifier *classifier) {
    using namespace modelgbp::arp;
    using namespace modelgbp::l2;

    match *m = &(fe.entry->match);
    uint8_t arpOpc =
            classifier->getArpOpc(OpcodeEnumT::CONST_UNSPECIFIED);
    uint16_t ethT =
            classifier->getEtherT(EtherTypeEnumT::CONST_UNSPECIFIED);
    if (arpOpc != OpcodeEnumT::CONST_UNSPECIFIED) {
        match_set_nw_proto(m, arpOpc);
    }
    if (ethT != EtherTypeEnumT::CONST_UNSPECIFIED) {
        match_set_dl_type(m, htons(ethT));
    }
    if (classifier->isProtSet()) {
        match_set_nw_proto(m, classifier->getProt().get());
    }
}

static void set_entry_tcp_flags(FlowEntry& fe, uint32_t tcpFlags) {
    using namespace modelgbp::l4;
    ovs_be16 flags = 0;
    if (tcpFlags & TcpFlagsEnumT::CONST_FIN) flags |= 0x01;
    if (tcpFlags & TcpFlagsEnumT::CONST_SYN) flags |= 0x02;
    if (tcpFlags & TcpFlagsEnumT::CONST_RST) flags |= 0x04;
    if (tcpFlags & TcpFlagsEnumT::CONST_ACK) flags |= 0x10;
    match_set_tcp_flags_masked(&fe.entry->match,
                               htons(flags), htons(flags));
}

const uint16_t MAX_POLICY_RULE_PRIORITY = 8192;     // arbitrary

void add_classifier_entries(uint8_t tableId, L24Classifier *clsfr,
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
                FlowEntryPtr e0(new FlowEntry());
                e0->entry->cookie = ckbe;

                flowutils::set_match_group(*e0, tableId, priority,
                                           svnid, dvnid);
                set_entry_protocol(*e0, clsfr);
                if (tcpFlags != TcpFlagsEnumT::CONST_UNSPECIFIED)
                    set_entry_tcp_flags(*e0, flagMask);
                match *m = &(e0->entry->match);
                match_set_tp_src_masked(m, htons(sm.first), htons(sm.second));
                match_set_tp_dst_masked(m, htons(dm.first), htons(dm.second));

                if (allow) {
                    ActionBuilder ab;
                    ab.SetGotoTable(nextTable);
                    ab.Build(e0->entry);
                }
                entries.push_back(e0);
            }
        }
    }
}

} // namespace flowutils
} // namespace ovsagent
