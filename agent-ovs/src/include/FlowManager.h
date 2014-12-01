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

#include "Agent.h"
#include "PortMapper.h"
#include "FlowExecutor.h"

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
        executor = e;
    }

    void SetPortMapper(PortMapper *m) {
        portMapper = m;
    }

    void SetTunnelType(std::string& tunnelType) { /* XXX TODO */ }
    void SetTunnelIface(std::string& tunnelIface) { /* XXX TODO */ }
    void SetTunnelRemoteIp(std::string& tunnelRemoteIp) { /* XXX TODO */ }

    uint32_t GetTunnelPort() { return tunnelPort; }
    uint32_t GetTunnelDstIpv4() { return tunnelDstIpv4; }
    const uint8_t *GetRouterMacAddr() { return routerMac; }

    /** Interface: EndpointListener */
    void endpointUpdated(const std::string& uuid);

    /** Interface: PolicyListener */
    void egDomainUpdated(const opflex::modb::URI& egURI);
    void contractUpdated(const opflex::modb::URI& contractURI);

    /**
     * Get the VNIDs for the specified endpoint groups.
     *
     * @param egURIs URIs of endpoint groups to search for
     * @egVnids VNIDs of the endpoint groups for those which have one
     */
    void GetGroupVnids(const boost::unordered_set<opflex::modb::URI>& egURIs,
            /* out */boost::unordered_set<uint32_t>& egVnids);

    /**
     * Get or generate a unique ID for a given object for use with flows.
     *
     * @param cid Class ID of the object
     * @param uri URI of the object
     * @return A unique ID for the object
     */
    uint32_t GetId(opflex::modb::class_id_t cid, const opflex::modb::URI& uri);

    /**
     * Maximum flow priority of the entries in policy table.
     */
    static const uint16_t MAX_POLICY_RULE_PRIORITY;
private:
    bool GetGroupForwardingInfo(const opflex::modb::URI& egUri, uint32_t& vnid,
            uint32_t& rdId, uint32_t& bdId, uint32_t& fdId);
    void UpdateGroupSubnets(const opflex::modb::URI& egUri,
            uint32_t routingDomainId);
    bool WriteFlow(const std::string& objId, flow::TableState& tab,
                   flow::FlowEntryList& el);
    bool WriteFlow(const std::string& objId, flow::TableState& tab,
                   flow::FlowEntry *e);

    /**
     * Create flow entries for the classifier specified and append them
     * to the provided list.
     *
     * @param classifier Classifier object to get matching rules from
     * @param priority Priority of the entry created
     * @param cookie Cookie of the entry created
     * @param svnid VNID of the source endpoint group for the entry
     * @param dvnid VNID of the destination endpoint group for the entry
     * @param entries List to append entry to
     */
    void AddEntryForClassifier(modelgbp::gbpe::L24Classifier *classifier,
            uint16_t priority, uint64_t cookie,
            uint32_t& svnid, uint32_t& dvnid,
            flow::FlowEntryList& entries);

    static bool ParseIpv4Addr(const std::string& str, uint32_t *ip);
    static bool ParseIpv6Addr(const std::string& str, in6_addr *ip);

    ovsagent::Agent& agent;
    FlowExecutor* executor;
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
    IdMap contractIds;
};

}   // namespace enforcer
}   // namespace opflex

#endif // _OPFLEX_ENFORCER_FLOWMANAGER_H_
