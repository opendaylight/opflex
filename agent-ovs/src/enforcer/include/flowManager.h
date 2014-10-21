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

#include <internal/modb.h>
#include <inventory.h>
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
class FlowManager {
public:
	/**
	 * Constructor with references to inventory and queue of change
	 * notifications.
	 */
    FlowManager(Inventory& im, ChangeList& cii);
    ~FlowManager() {}

    /**
     * Module start
     */
    void Start();

    /**
     * Module stop
     */
    void Stop();

    /**
     * Compute flow modifications and execute them.
     */
    void Generate();

    void SetExecutor(FlowExecutor *e) {
        executor.reset(e);
    }

    uint32_t GetTunnelPort() { return tunnelPort; }
    uint32_t GetTunnelDstIpv4() { return tunnelDstIpv4; }
    const uint8_t *GetRouterMacAddr() { return routerMac; }

private:
    void HandleEndpoint(ChangeInfo& chgInfo);
    void HandleEndpointGroup(ChangeInfo& chgInfo);
    void HandleSubnet(ChangeInfo& chgInfo);
    void HandleFloodDomain(ChangeInfo& chgInfo);
    void HandlePolicy(ChangeInfo& chgInfo);

    bool WriteFlow(const modb::URI& uri, flow::TableState& tab, flow::EntryList& el);
    bool WriteFlow(const modb::URI& uri, flow::TableState& tab, flow::Entry *e);

    static bool ParseMacAddr(const std::string& str, uint8_t *mac);
    static bool ParseIpv4Addr(const std::string& str, uint32_t *ip);

    Inventory& invtManager;
    ChangeList& changeInfoIn;
    boost::scoped_ptr<FlowExecutor> executor;

    uint32_t tunnelPort;
    uint32_t tunnelDstIpv4;
    uint8_t routerMac[6];
    flow::TableState sourceTable;
    flow::TableState destinationTable;
    flow::TableState policyTable;
    flow::TableState portSecurityTable;
};

}   // namespace enforcer
}   // namespace opflex

#endif // _OPFLEX_ENFORCER_FLOWMANAGER_H_
