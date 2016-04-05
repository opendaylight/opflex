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
#include <boost/function.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/bind.hpp>

#include <vector>

namespace ovsagent {
namespace flowutils {

using std::vector;
using modelgbp::gbpe::L24Classifier;

std::ostream & operator<<(std::ostream &os, const subnet_t& subnet) {
    os << subnet.first << "/" << (int)subnet.second;
    return os;
}

std::ostream & operator<<(std::ostream &os, const subnets_t& subnets) {
    bool first = true;
    BOOST_FOREACH(subnet_t s, subnets) {
        if (first) first = false;
        else os << ",";
        os << s;
    }
    return os;
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
    ovs_be16 flags = 0;
    if (tcpFlags & TcpFlagsEnumT::CONST_FIN) flags |= 0x01;
    if (tcpFlags & TcpFlagsEnumT::CONST_SYN) flags |= 0x02;
    if (tcpFlags & TcpFlagsEnumT::CONST_RST) flags |= 0x04;
    if (tcpFlags & TcpFlagsEnumT::CONST_ACK) flags |= 0x10;
    f.tcpFlags(flags, flags);
}

const uint16_t MAX_POLICY_RULE_PRIORITY = 8192;     // arbitrary

static void do_add_entries(uint32_t tcpFlags, vector<uint32_t>& tcpFlagsVec,
                           MaskList& srcPorts, MaskList& dstPorts,
                           L24Classifier& clsfr, bool allow,
                           uint8_t nextTable, uint16_t priority,
                           ovs_be64 ckbe, uint32_t svnid, uint32_t dvnid,
                           /* out */ FlowEntryList& entries) {
    using modelgbp::l4::TcpFlagsEnumT;

}

static subnets_t compute_eff_sub(boost::optional<const subnets_t&> sub) {
    static const subnet_t ALL("", 0);

    subnets_t eff;
    if (sub) {
        eff.insert(sub.get().begin(), sub.get().end());
    } else {
        eff.insert(ALL);
    }

    return eff;
}

typedef boost::function<void(FlowBuilder*,
                             boost::asio::ip::address&,
                             uint8_t)> FlowBuilderFunc;
static bool applyRemoteSub(FlowBuilder& fb, FlowBuilderFunc func,
                           boost::asio::ip::address addr,
                           uint8_t prefixLen, uint16_t ethType) {
    if (addr.is_v4() && ethType != ETH_TYPE_ARP && ethType != ETH_TYPE_IP)
        return false;
    if (addr.is_v6() && ethType != ETH_TYPE_IPV6)
        return false;

    func(&fb, addr, prefixLen);
    return true;
}

typedef boost::function<bool(FlowBuilder&, uint16_t)> flow_func;
static flow_func make_flow_functor(const subnet_t& ss, FlowBuilderFunc func) {
    boost::system::error_code ec;
    if (ss.first == "") return NULL;
    boost::asio::ip::address addr =
        boost::asio::ip::address::from_string(ss.first, ec);
    if (ec) return NULL;
    return boost::bind(applyRemoteSub, _1, func, addr, ss.second, _2);
}

void add_classifier_entries(L24Classifier& clsfr, bool allow,
                            boost::optional<const subnets_t&> sourceSub,
                            boost::optional<const subnets_t&> destSub,
                            uint8_t nextTable, uint16_t priority,
                            uint64_t cookie, uint32_t svnid, uint32_t dvnid,
                            /* out */ FlowEntryList& entries) {
    using modelgbp::l4::TcpFlagsEnumT;

    ovs_be64 ckbe = htonll(cookie);
    MaskList srcPorts;
    MaskList dstPorts;
    RangeMask::getMasks(clsfr.getSFromPort(), clsfr.getSToPort(), srcPorts);
    RangeMask::getMasks(clsfr.getDFromPort(), clsfr.getDToPort(), dstPorts);

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

    subnets_t effSourceSub(compute_eff_sub(sourceSub));
    subnets_t effDestSub(compute_eff_sub(destSub));

    BOOST_FOREACH (const subnet_t& ss, effSourceSub) {
        flow_func src_func(make_flow_functor(ss, &FlowBuilder::ipSrc));

        BOOST_FOREACH (const subnet_t& ds, effDestSub) {
        flow_func dst_func(make_flow_functor(ds, &FlowBuilder::ipDst));

            BOOST_FOREACH (const Mask& sm, srcPorts) {
                BOOST_FOREACH(const Mask& dm, dstPorts) {
                    BOOST_FOREACH(uint32_t flagMask, tcpFlagsVec) {
                        FlowBuilder f;
                        f.cookie(ckbe);

                        flowutils::match_group(f, priority, svnid, dvnid);
                        uint16_t etht = match_protocol(f, clsfr);
                        if (tcpFlags != TcpFlagsEnumT::CONST_UNSPECIFIED)
                            match_tcp_flags(f, flagMask);

                        if (src_func && !src_func(f, etht)) continue;
                        if (dst_func && !dst_func(f, etht)) continue;

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
    }

    do_add_entries(tcpFlags, tcpFlagsVec, srcPorts, dstPorts, clsfr, allow,
                   nextTable, priority, ckbe, svnid, dvnid, entries);
}

} // namespace flowutils
} // namespace ovsagent
