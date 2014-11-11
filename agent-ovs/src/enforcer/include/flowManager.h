/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef _OPFLEX_ENFORCER_FLOWMANAGER_H_
#define _OPFLEX_ENFORCER_FLOWMANAGER_H_

#include <boost/scoped_ptr.hpp>

#include <Agent.h>
#include <portMapper.h>
#include <flowExecutor.h>

namespace opflex {
namespace enforcer {

/**
 * @brief Makes changes to OpenFlow tables to be in sync with state
 * of MOs.
 * Main function is to handling change notifications, generate a set
 * of flow modifications that represent the changes and apply these
 * modifications.
 */
class FlowManager : public ovsagent::EndpointListener,
                    public ovsagent::PolicyListener {
public:
    FlowManager(ovsagent::Agent& agent);
    ~FlowManager() {}

    /**
     * Module start
     */
    void Start();

    /**
     * Module stop
     */
    void Stop();

    void SetExecutor(FlowExecutor *e) {
        executor.reset(e);
    }

    void SetPortMapper(PortMapper *m) {
        portMapper = m;
    }

    uint32_t GetTunnelPort() { return tunnelPort; }
    uint32_t GetTunnelDstIpv4() { return tunnelDstIpv4; }
    const uint8_t *GetRouterMacAddr() { return routerMac; }

    /** Interface: EndpointListener */
    void endpointUpdated(const std::string& uuid);

    /** Interface: PolicyListener */
    void egDomainUpdated(const opflex::modb::URI& egURI);

private:
    bool GetGroupForwardingInfo(const opflex::modb::URI& egUri, uint32_t& vnid,
            uint32_t& rdId, uint32_t& bdId, uint32_t& fdId);
    uint32_t GetId(opflex::modb::class_id_t cid, const opflex::modb::URI& uri);
    void UpdateGroupSubnets(const opflex::modb::URI& egUri,
            uint32_t routingDomainId);
    bool WriteFlow(const std::string& objId, flow::TableState& tab,
                   flow::FlowEntryList& el);
    bool WriteFlow(const std::string& objId, flow::TableState& tab,
                   flow::FlowEntry *e);

    static bool ParseIpv4Addr(const std::string& str, uint32_t *ip);
    static bool ParseIpv6Addr(const std::string& str, in6_addr *ip);

    ovsagent::Agent& agent;
    boost::scoped_ptr<FlowExecutor> executor;
    PortMapper *portMapper;

    uint32_t tunnelPort;
    uint32_t tunnelDstIpv4;
    uint8_t routerMac[6];
    flow::TableState sourceTable;
    flow::TableState destinationTable;
    flow::TableState policyTable;
    flow::TableState portSecurityTable;

    /**
     * Helper class to assign unique numeric IDs to some object-types.
     */
    class IdMap {
    public:
        IdMap() : lastUsedId(0) {}
        uint32_t FindOrGenerate(const opflex::modb::URI& uri);
    private:
        boost::unordered_map<opflex::modb::URI, uint32_t> ids;
        unsigned long lastUsedId;
    };
    IdMap routingDomainIds, bridgeDomainIds, floodDomainIds;
};

}   // namespace enforcer
}   // namespace opflex

#endif // _OPFLEX_ENFORCER_FLOWMANAGER_H_
