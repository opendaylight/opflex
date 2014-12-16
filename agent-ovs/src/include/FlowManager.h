/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef _OPFLEX_ENFORCER_FLOWMANAGER_H_
#define _OPFLEX_ENFORCER_FLOWMANAGER_H_

#include <utility>

#include <boost/asio.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/optional.hpp>
#include <boost/unordered_map.hpp>

#include "Agent.h"
#include "WorkQueue.h"
#include "SwitchConnection.h"
#include "PortMapper.h"
#include "FlowReader.h"
#include "FlowExecutor.h"
#include "IdGenerator.h"

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
                    public ovsagent::PolicyListener,
                    public OnConnectListener,
                    public MessageHandler {
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

    /**
     * Set the object used for reading flows and groups from the switch.
     *
     * @param r The reader object
     */
    void SetFlowReader(ovsagent::FlowReader *r) {
        reader = r;
    }

    /**
     * Register the given connection with the learning switch. Installs
     * all the necessary listeners on the connection.
     *
     * @param connection the connection to use for learning
     */
    void registerConnection(opflex::enforcer::SwitchConnection* connection);

    /**
     * Unregister the given connection.
     *
     * @param conn the connection to unregister
     */
    void unregisterConnection(SwitchConnection *conn);

    /**
     * Installs listeners for receiving updates to MODB state.
     */
    void registerModbListeners();

    /**
     * Unregisters listeners that receive updates to MODB state.
     */
    void unregisterModbListeners();

    /**
     * How to behave on an unknown unicast destination when the flood
     * domain is not configured to flood unknown
     */
    enum FallbackMode {
        /**
         * Drop unknown unicast traffic
         */
        FALLBACK_DROP,
        /**
         * Send the unknown unicast traffic out the tunnel interface
         * to be handled by an upstream fabric.
         */
        FALLBACK_PROXY
    };

    /**
     * Set the unknown unicast fallback mode to the specified value
     */
    void SetFallbackMode(FallbackMode fallbackMode);

    /**
     * Encap types supported by the flow manager
     */
    enum EncapType {
        /**
         * No encapsulation; traffic can be forwarded only on the
         * local switch.
         */
        ENCAP_NONE,
        /**
         * Encapsulate using VLAN tags, with a (at least)
         * locally-significant VLAN for each endpoint group.
         */
        ENCAP_VLAN,
        /**
         * Encapsulate using a VXLAN tunnel, with a VNID for each
         * endpoint group.
         */
        ENCAP_VXLAN
    };

    void SetEncapType(EncapType encapType);
    void SetEncapIface(const std::string& encapIface);

    void SetTunnelRemoteIp(const std::string& tunnelRemoteIp);
    void SetVirtualRouter(bool virtualRouterEnabled);
    void SetVirtualRouterMac(const std::string& mac);

    uint32_t GetTunnelPort();
    uint32_t GetTunnelDstIpv4() { return tunnelDstIpv4; }
    const uint8_t *GetRouterMacAddr() { return routerMac; }

    /**
     * Set the delay after which flow-manager will attempt to reconcile
     * cached memory state with the flow tables on the switch once connection
     * is established to the switch. Applies only to the next time
     * connection is established.
     *
     * @param delayMsec the delay in milliseconds, setting it to 0 will
     * disable the delay
     */
    void SetSyncDelayOnConnect(long delayMsec);

    /** Interface: EndpointListener */
    void endpointUpdated(const std::string& uuid);

    /** Interface: PolicyListener */
    void egDomainUpdated(const opflex::modb::URI& egURI);
    void contractUpdated(const opflex::modb::URI& contractURI);

    /** Interface: OnConnectListener */
    void Connected(SwitchConnection *swConn);

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
     * Get the cookie used for flow entries that are learnt reactively.
     *
     * @return flow-cookie for learnt entries
     */
    static ovs_be64 GetLearnEntryCookie();

    /**
     * Maximum flow priority of the entries in policy table.
     */
    static const uint16_t MAX_POLICY_RULE_PRIORITY;

    // see: MessageHandler
    void Handle(opflex::enforcer::SwitchConnection *swConn, 
                ofptype type, ofpbuf *msg);

    /**
     * Indices of tables managed by the flow-manager.
     */
    enum { SEC_TABLE_ID, SRC_TABLE_ID, DST_TABLE_ID, LEARN_TABLE_ID,
           POL_TABLE_ID, NUM_FLOW_TABLES };

private:
    /**
     * Compare and update flow/group tables due to changes in an endpoint.
     *
     * @param uuid UUID of the changed endpoint
     */
    void HandleEndpointUpdate(const std::string& uuid);

    /**
     * Compare and update flow/group tables due to changes in an
     * endpoint group.
     *
     * @param egURI URI of the changed endpoint group
     */
    void HandleEndpointGroupDomainUpdate(const opflex::modb::URI& egURI);

    /**
     * Compare and update flow/group tables due to changes in a contract
     *
     * @param contractURI URI of the changed contract
     */
    void HandleContractUpdate(const opflex::modb::URI& contractURI);

    /**
     * Handle establishment of connection to a switch by reconciling
     * cached memory state with switch state.
     *
     * @param sw Connection to the switch
     */
    void HandleConnection(SwitchConnection *sw);

    bool GetGroupForwardingInfo(const opflex::modb::URI& egUri, uint32_t& vnid,
            uint32_t& rdId, uint32_t& bdId,
            boost::optional<opflex::modb::URI>& fdURI, uint32_t& fdId);
    void UpdateGroupSubnets(const opflex::modb::URI& egUri,
            uint32_t routingDomainId);
    bool WriteFlow(const std::string& objId, int tableId,
            flow::FlowEntryList& el);
    bool WriteFlow(const std::string& objId, int tableId, flow::FlowEntry *e);
    /**
     * Write a group-table change to the switch
     *
     * @param entry Change to the group-table entry
     * @return true is successful, false otherwise
     */
    bool WriteGroupMod(const flow::GroupEdit::Entry& entry);

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

    /**
     * Update flow-tables to associate an endpoint with a flood-domain.
     *
     * @param fdURI URI of flood-domain
     * @param endpoint The endpoint to update
     * @param epPort Port number of endpoint
     * @param isPromiscuous whether the endpoint port is promiscuous
     */
    void UpdateEndpointFloodDomain(const opflex::modb::URI& fdURI,
                                   const ovsagent::Endpoint& endPoint, 
                                   uint32_t epPort, 
                                   bool isPromiscuous, 
                                   boost::optional<boost::shared_ptr<
                                       modelgbp::gbp::FloodDomain> >& fd);

    /**
     * Update flow-tables to dis-associate an endpoint from any flood-domain.
     *
     * @param epUUID UUID of endpoint
     */
    void RemoveEndpointFromFloodDomain(const std::string& epUUID);

    /*
     * Map of endpoint to the port it is using.
     */
    typedef boost::unordered_map<std::string, std::pair<uint32_t, bool> > Ep2PortMap;

    /**
     * Construct a group-table modification.
     *
     * @param type The modification type
     * @param groupId Identifier for the flow group to edit
     * @param ep2port Ports to be associated with this flow group
     * @param onlyPromiscuous only include promiscuous endpoints and
     * uplinks in the group
     * @return Group-table modification entry
     */
    flow::GroupEdit::Entry CreateGroupMod(uint16_t type, uint32_t groupId,
                                          const Ep2PortMap& ep2port,
                                          bool onlyPromiscuous = false);

    ovsagent::Agent& agent;
    SwitchConnection* connection;
    FlowExecutor* executor;
    PortMapper *portMapper;
    ovsagent::FlowReader *reader;

    FallbackMode fallbackMode;
    EncapType encapType;
    std::string encapIface;
    uint32_t tunnelDstIpv4;
    bool virtualRouterEnabled;
    uint8_t routerMac[6];
    flow::TableState flowTables[NUM_FLOW_TABLES];

    /*
     * Map of flood-domain URI to the endpoints associated with it.
     */
    typedef boost::unordered_map<opflex::modb::URI, Ep2PortMap> FdMap;
    FdMap fdMap;

    ovsagent::WorkQueue workQ;

    const char * GetIdNamespace(opflex::modb::class_id_t cid);
    ovsagent::IdGenerator idGen;

    bool isSyncing;

    /**
     * Class to reconcile flow/group table state with cached memory state
     */
    class FlowSyncer {
    public:
        FlowSyncer(FlowManager& fm);

        /**
         * Reconcile flow/group table state with cached memory state
         */
        void Sync();
    private:
        /**
         * Begin reconciliation by reading all the flows and groups from the
         * switch.
         */
        void InitiateSync();

        /**
         * Compare flows/groups read from switch to determine the differences
         * and make modification to eliminate those differences.
         */
        void CompleteSync();

        /**
         * Callback function provided to FlowReader to process received
         * flow table entries.
         */
        void GotFlows(int tableNum, const flow::FlowEntryList& flows,
            bool done);

        /**
         * Callback function provided to FlowReader to process received
         * group table entries.
         */
        void GotGroups(const flow::GroupEdit::EntryList& groups,
            bool done);

        /**
         * Determine if all flows/groups were received; starts reconciliation
         * if so.
         */
        void CheckRecvDone();

        /**
         * Compare flows read from switch and make modification to eliminate
         * differences.
         */
        void ReconcileFlows();

        /**
         * Compare flows read from switch and make modification to eliminate
         * differences.
         */
        void ReconcileGroups();

        /**
         * Check if a group with given ID and endpoints is present
         * in the received groups, and update the given group-edits if the
         * group is not found or is different. If a group is found, it is
         * removed from the set of received groups.
         *
         * @param groupId ID of the group to check
         * @param epMap endpoints in the group to check
         * @param prom if promiscuous mode endpoints only are to be considered
         * @param ge Container to append the changes
         */
        void CheckGroupEntry(uint32_t groupId,
                const Ep2PortMap& epMap, bool prom, flow::GroupEdit& ge);

        FlowManager& flowManager;
        flow::FlowEntryList recvFlows[FlowManager::NUM_FLOW_TABLES];
        bool tableDone[FlowManager::NUM_FLOW_TABLES];

        typedef boost::unordered_map<uint32_t, flow::GroupEdit::Entry> GroupMap;
        GroupMap recvGroups;
        bool groupsDone;

        bool syncInProgress;
        bool syncPending;
    };
    friend class FlowSyncer;

    FlowSyncer flowSyncer;

    /**
     * Timer callback that begins reconciliation.
     */
    void OnConnectTimer(const boost::system::error_code& ec);
    boost::scoped_ptr<boost::asio::deadline_timer> connectTimer;
};

}   // namespace enforcer
}   // namespace opflex

#endif // _OPFLEX_ENFORCER_FLOWMANAGER_H_
