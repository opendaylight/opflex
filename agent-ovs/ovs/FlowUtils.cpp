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
#include "eth.h"
#include "ovs-shim.h"

#include <modelgbp/l2/EtherTypeEnumT.hpp>
#include <modelgbp/l4/TcpFlagsEnumT.hpp>
#include <modelgbp/arp/OpcodeEnumT.hpp>

#include <boost/asio/ip/address.hpp>

#include <vector>
#include <functional>

namespace opflexagent {
namespace flowutils {

using std::vector;
using modelgbp::gbpe::L24Classifier;

void match_rdId(FlowBuilder& f, uint32_t rdId)
{
    f.reg(6, rdId);
}

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

static uint16_t match_protocol(FlowBuilder& f, L24Classifier& classifier) {
    using modelgbp::arp::OpcodeEnumT;
    using modelgbp::l2::EtherTypeEnumT;

    uint8_t arpOpc =
            classifier.getArpOpc(OpcodeEnumT::CONST_UNSPECIFIED);
    uint16_t ethT =
            classifier.getEtherT(EtherTypeEnumT::CONST_UNSPECIFIED);
    if (arpOpc != OpcodeEnumT::CONST_UNSPECIFIED) {
        f.proto(arpOpc);
    }
    if (ethT != EtherTypeEnumT::CONST_UNSPECIFIED) {
        f.ethType(ethT);
    }
    if (classifier.isProtSet()) {
        f.proto(classifier.getProt().get());
    }
    return ethT;
}

static void match_tcp_flags(FlowBuilder& f, uint32_t tcpFlags) {
    using modelgbp::l4::TcpFlagsEnumT;
    uint16_t flags = 0;
    if (tcpFlags & TcpFlagsEnumT::CONST_FIN) flags |= 0x01;
    if (tcpFlags & TcpFlagsEnumT::CONST_SYN) flags |= 0x02;
    if (tcpFlags & TcpFlagsEnumT::CONST_RST) flags |= 0x04;
    if (tcpFlags & TcpFlagsEnumT::CONST_ACK) flags |= 0x10;
    f.tcpFlags(flags, flags);
}

static network::subnets_t
compute_eff_sub(boost::optional<const network::subnets_t&> sub) {
    static const network::subnet_t ALL("", 0);

    network::subnets_t eff;
    if (sub) {
        eff.insert(sub.get().begin(), sub.get().end());
    } else {
        eff.insert(ALL);
    }

    return eff;
}

typedef std::function<void(FlowBuilder*,
                             boost::asio::ip::address&,
                             uint8_t)> FlowBuilderFunc;
static bool applyRemoteSub(FlowBuilder& fb, const FlowBuilderFunc& func,
                           boost::asio::ip::address addr,
                           uint8_t prefixLen, uint16_t ethType) {
    if (addr.is_v4() && ethType != eth::type::ARP && ethType != eth::type::IP)
        return false;
    if (addr.is_v6() && ethType != eth::type::IPV6)
        return false;

    func(&fb, addr, prefixLen);
    return true;
}

typedef std::function<bool(FlowBuilder&, uint16_t)> flow_func;
static flow_func make_flow_functor(const network::subnet_t& ss,
                                   const FlowBuilderFunc& func) {
    using std::placeholders::_1;
    using std::placeholders::_2;

    boost::system::error_code ec;
    if (ss.first.empty()) return NULL;
    boost::asio::ip::address addr =
        boost::asio::ip::address::from_string(ss.first, ec);
    if (ec) return NULL;
    return std::bind(applyRemoteSub, _1, func, addr, ss.second, _2);
}

void add_classifier_entries(L24Classifier& clsfr, ClassAction act,
                            boost::optional<const network::subnets_t&> sourceSub,
                            boost::optional<const network::subnets_t&> destSub,
                            uint8_t nextTable, uint16_t priority,
                            uint32_t flags, uint64_t cookie,
                            uint32_t svnid, uint32_t dvnid,
                            /* out */ FlowEntryList& entries) {
    using modelgbp::l4::TcpFlagsEnumT;

    ovs_be64 ckbe = ovs_htonll(cookie);
    MaskList srcPorts;
    MaskList dstPorts;
    if (clsfr.getProt(0) == 1 &&
        (clsfr.isIcmpTypeSet() || clsfr.isIcmpCodeSet())) {
        if (clsfr.isIcmpTypeSet()) {
            srcPorts.push_back(Mask(clsfr.getIcmpType(0), ~0));
        }
        if (clsfr.isIcmpCodeSet()) {
            dstPorts.push_back(Mask(clsfr.getIcmpCode(0), ~0));
        }
    } else {
        RangeMask::getMasks(clsfr.getSFromPort(), clsfr.getSToPort(), srcPorts);
        RangeMask::getMasks(clsfr.getDFromPort(), clsfr.getDToPort(), dstPorts);
    }

    /* Add a "ignore" mask to empty ranges - makes the loop later easy */
    if (srcPorts.empty()) {
        srcPorts.push_back(Mask(0x0, 0x0));
    }
    if (dstPorts.empty()) {
        dstPorts.push_back(Mask(0x0, 0x0));
    }

    vector<uint32_t> tcpFlagsVec;
    uint32_t tcpFlags = clsfr.getTcpFlags(TcpFlagsEnumT::CONST_UNSPECIFIED);
    if (tcpFlags & TcpFlagsEnumT::CONST_ESTABLISHED) {
        tcpFlagsVec.push_back(0 + TcpFlagsEnumT::CONST_ACK);
        tcpFlagsVec.push_back(0 + TcpFlagsEnumT::CONST_RST);
    } else {
        tcpFlagsVec.push_back(tcpFlags);
    }

    network::subnets_t effSourceSub(compute_eff_sub(sourceSub));
    network::subnets_t effDestSub(compute_eff_sub(destSub));

    for (const network::subnet_t& ss : effSourceSub) {
        flow_func src_func(make_flow_functor(ss, &FlowBuilder::ipSrc));

        for (const network::subnet_t& ds : effDestSub) {
            flow_func dst_func(make_flow_functor(ds, &FlowBuilder::ipDst));

            for (const Mask& sm : srcPorts) {
                for (const Mask& dm : dstPorts) {
                    for (uint32_t flagMask : tcpFlagsVec) {
                        FlowBuilder f;
                        f.cookie(ckbe);
                        f.flags(flags);

                        switch (act) {
                        case flowutils::CA_REFLEX_FWD_TRACK:
                        case flowutils::CA_REFLEX_REV_TRACK:
                            f.conntrackState(0, FlowBuilder::CT_TRACKED);
                            break;
                        case flowutils::CA_REFLEX_REV_ALLOW:
                            f.conntrackState(FlowBuilder::CT_TRACKED |
                                             FlowBuilder::CT_ESTABLISHED |
                                             FlowBuilder::CT_REPLY,
                                             FlowBuilder::CT_TRACKED |
                                             FlowBuilder::CT_ESTABLISHED |
                                             FlowBuilder::CT_REPLY |
                                             FlowBuilder::CT_INVALID |
                                             FlowBuilder::CT_NEW |
                                             FlowBuilder::CT_RELATED);
                            break;
                        case flowutils::CA_REFLEX_REV_RELATED:
                            f.conntrackState(FlowBuilder::CT_TRACKED |
                                             FlowBuilder::CT_RELATED,
                                             FlowBuilder::CT_TRACKED |
                                             FlowBuilder::CT_RELATED |
                                             FlowBuilder::CT_ESTABLISHED |
                                             FlowBuilder::CT_INVALID |
                                             FlowBuilder::CT_NEW);
                            break;
                        default:
                            // nothing
                            break;
                        }

                        flowutils::match_group(f, priority, svnid, dvnid);
                        uint16_t etht = match_protocol(f, clsfr);

                        switch (act) {
                        case flowutils::CA_DENY:
                        case flowutils::CA_ALLOW:
                        case flowutils::CA_REFLEX_FWD_TRACK:
                        case flowutils::CA_REFLEX_FWD:
                        case flowutils::CA_REFLEX_FWD_EST:
                            if (tcpFlags != TcpFlagsEnumT::CONST_UNSPECIFIED)
                                match_tcp_flags(f, flagMask);

                            if (src_func && !src_func(f, etht)) continue;
                            if (dst_func && !dst_func(f, etht)) continue;

                            f.tpSrc(sm.first, sm.second)
                                .tpDst(dm.first, dm.second);
                            break;
                        default:
                            // nothing
                            break;
                        }

                        switch (act) {
                        case flowutils::CA_REFLEX_FWD_TRACK:
                        case flowutils::CA_REFLEX_REV_TRACK:
                            f.action().conntrack(0, MFF_REG6, 0, nextTable);
                            break;
                        case flowutils::CA_REFLEX_FWD:
                            f.conntrackState(FlowBuilder::CT_TRACKED |
                                             FlowBuilder::CT_NEW,
                                             FlowBuilder::CT_TRACKED |
                                             FlowBuilder::CT_NEW);
                            f.action().conntrack(ActionBuilder::CT_COMMIT,
                                                 MFF_REG6).go(nextTable);
                            break;
                        case CA_REFLEX_FWD_EST:
                            f.conntrackState(FlowBuilder::CT_TRACKED |
                                             FlowBuilder::CT_ESTABLISHED,
                                             FlowBuilder::CT_TRACKED |
                                             FlowBuilder::CT_ESTABLISHED);
                            f.action().go(nextTable);
                            break;
                        case flowutils::CA_REFLEX_REV_ALLOW:
                        case flowutils::CA_REFLEX_REV_RELATED:
                        case flowutils::CA_ALLOW:
                            f.action().go(nextTable);
                            break;
                        case flowutils::CA_DENY:
                        default:
                            // nothing
                            break;
                        }
                        entries.push_back(f.build());
                    }
                }
            }
        }
    }
}

FlowBuilder& match_dhcp_req(FlowBuilder& fb, bool v4) {
    fb.proto(17);
    if (v4) {
        fb.ethType(eth::type::IP);
        fb.tpSrc(68);
        fb.tpDst(67);
    } else {
        fb.ethType(eth::type::IPV6);
        fb.tpSrc(546);
        fb.tpDst(547);
    }
    return fb;
}

} // namespace flowutils
} // namespace opflexagent
