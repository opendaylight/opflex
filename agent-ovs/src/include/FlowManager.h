/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef OVSAGENT_FLOWMANAGER_H_
#define OVSAGENT_FLOWMANAGER_H_

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

namespace ovsagent {

/**
 * @brief Makes changes to OpenFlow tables to be in sync with state
 * of MOs.
 * Main function is to handling change notifications, generate a set
 * of flow modifications that represent the changes and apply these
 * modifications.
 */
class FlowManager : public EndpointListener,
                    public PolicyListener,
                    public OnConnectListener,
                    public MessageHandler,
                    public PortStatusListener {
public:
    /**
     * Construct a new flow manager for the agent
     * @param agent the agent object
     */
    FlowManager(Agent& agent);
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
     * Set the flow executor to use
     * @param e the flow executor
     */
    void SetExecutor(FlowExecutor *e) {
        executor = e;
    }

    /**
     * Set the port mapper to use
     * @param m the port mapper
     */
    void SetPortMapper(PortMapper *m);

    /**
     * Set the object used for reading flows and groups from the switch.
     *
     * @param r The reader object
     */
    void SetFlowReader(FlowReader *r) {
        reader = r;
    }

    /**
     * Register the given connection with the learning switch. Installs
     * all the necessary listeners on the connection.
     *
     * @param connection the connection to use for learning
     */
    void registerConnection(SwitchConnection* connection);

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
        ENCAP_VXLAN,
        /**
         * Encapsulate using an IVXLAN tunnel, with a VNID for each
         * endpoint group and additional metadata in the tunnel header
         */
        ENCAP_IVXLAN
    };

    /**
     * Set the encap type to use for packets sent over the network
     * @param encapType the encap type
     */
    void SetEncapType(EncapType encapType);
    /**
     * Set the openflow interface name for encapsulated packets
     * @param encapIface the interface name
     */
    void SetEncapIface(const std::string& encapIface);

    /**
     * Flooding scopes supported by the flow manager.
     */
    enum FloodScope {
        /**
         * Flood to all endpoints within a flood domain
         */
        FLOOD_DOMAIN,

        /**
         * Flood to endpoints only within an endpoint group
         */
        ENDPOINT_GROUP
    };

    /**
     * Set the flood scope
     * @param floodScope the flood scope
     */
    void SetFloodScope(FloodScope floodScope);

    /**
     * Set the remote IP to use for unicast tunnel traffic
     * @param tunnelRemoteIp the remote tunnel IP
     */
    void SetTunnelRemoteIp(const std::string& tunnelRemoteIp);

    /**
     * Enable or disable the virtual routing
     *
     * @param virtualRouterEnabled true to enable the router
     */
    void SetVirtualRouter(bool virtualRouterEnabled);

    /**
     * Set the MAC address to use for the virtual router
     * @param mac the MAC address formatted as a colon-separated
     * string of 6 hex-encoded bytes.
     */
    void SetVirtualRouterMac(const std::string& mac);

    /**
     * Set the flow ID cache directory
     * @param flowIdCache the directory where flow ID entries will be
     * cached
     */
    void SetFlowIdCache(const std::string& flowIdCache);

    /**
     * Get the openflow port that maps to the configured tunnel
     * interface
     * @return the openflow port number
     */
    uint32_t GetTunnelPort();

    /**
     * Get the configured tunnel destination as a parsed IP address
     * @return the tunnel destination
     */
    boost::asio::ip::address& GetTunnelDst() { return tunnelDst; }
    /**
     * Get the router MAC address as an array of 6 bytes
     * @return the router MAC
     */
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

    /* Interface: EndpointListener */
    void endpointUpdated(const std::string& uuid);

    /* Interface: PolicyListener */
    void egDomainUpdated(const opflex::modb::URI& egURI);
    void contractUpdated(const opflex::modb::URI& contractURI);
    void configUpdated(const opflex::modb::URI& configURI);

    /* Interface: OnConnectListener */
    void Connected(SwitchConnection *swConn);

    /* Interface: PortStatusListener */
    void portStatusUpdate(const std::string& portName, uint32_t portNo);

    /**
     * Get the VNIDs for the specified endpoint groups.
     *
     * @param egURIs URIs of endpoint groups to search for
     * @param egVnids VNIDs of the endpoint groups for those which
     * have one
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
     * Get the cookie used for flow entries that are learned reactively.
     *
     * @return flow-cookie for learnt entries
     */
    static ovs_be64 GetLearnEntryCookie();

    /**
     * Get the cookie used for learn flow entries that are proactively
     * installed
     *
     * @return flow-cookie for learnt entries
     */
    static ovs_be64 GetProactiveLearnEntryCookie();

    /**
     * Get the cookie used for cookies that direct neighbor discovery
     * packets to the controller
     *
     * @return flow-cookie for ND packets
     */
    static ovs_be64 GetNDCookie();

    /**
     * Maximum flow priority of the entries in policy table.
     */
    static const uint16_t MAX_POLICY_RULE_PRIORITY;

    // see: MessageHandler
    void Handle(SwitchConnection *swConn,
                ofptype type, ofpbuf *msg);

    /**
     * Indicate that the agent is connected to its Opflex peer.
     * Note: This method should not be invoked directly except for purposes
     * of unit-testing.
     */
    void PeerConnected();

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
     * Compare and update flow/group tables due to changes in platform
     * config
     *
     * @param configURI URI of the changed contract
     */
    void HandleConfigUpdate(const opflex::modb::URI& configURI);

    /**
     * Handle establishment of connection to a switch by reconciling
     * cached memory state with switch state.
     *
     * @param sw Connection to the switch
     */
    void HandleConnection(SwitchConnection *sw);

    /**
     * Handle changes to port-status by recomputing flows for endpoints
     * and endpoint groups.
     *
     * @param portName Name of the port that changed
     * @param portNo Port number of the port that changed
     */
    void HandlePortStatusUpdate(const std::string& portName, uint32_t portNo);

    bool GetGroupForwardingInfo(const opflex::modb::URI& egUri, uint32_t& vnid,
            boost::optional<opflex::modb::URI>& rdURI, uint32_t& rdId, uint32_t& bdId,
            boost::optional<opflex::modb::URI>& fdURI, uint32_t& fdId);
    void UpdateGroupSubnets(const opflex::modb::URI& egUri,
            uint32_t routingDomainId);
    bool WriteFlow(const std::string& objId, int tableId,
            FlowEntryList& el);
    bool WriteFlow(const std::string& objId, int tableId, FlowEntry *e);

    /**
     * Update all current group table entries
     */
    void UpdateGroupTable();

    /**
     * Write a group-table change to the switch
     *
     * @param entry Change to the group-table entry
     * @return true is successful, false otherwise
     */
    bool WriteGroupMod(const GroupEdit::Entry& entry);

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
            FlowEntryList& entries);

    static bool ParseIpv4Addr(const std::string& str, uint32_t *ip);
    static bool ParseIpv6Addr(const std::string& str, in6_addr *ip);

    /**
     * Update flow-tables to associate an endpoint with a flood-group.
     *
     * @param fgrpURI URI of flood-group (flood-domain or endpoint-group)
     * @param endpoint The endpoint to update
     * @param epPort Port number of endpoint
     * @param isPromiscuous whether the endpoint port is promiscuous
     * @param fd Flood-domain to which the endpoint belongs
     */
    void UpdateEndpointFloodGroup(const opflex::modb::URI& fgrpURI,
                                  const Endpoint& endPoint,
                                  uint32_t epPort,
                                  bool isPromiscuous,
                                  boost::optional<boost::shared_ptr<
                                      modelgbp::gbp::FloodDomain> >& fd);

    /**
     * Update flow-tables to dis-associate an endpoint from any flood-group.
     *
     * @param epUUID UUID of endpoint
     */
    void RemoveEndpointFromFloodGroup(const std::string& epUUID);

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
    GroupEdit::Entry CreateGroupMod(uint16_t type, uint32_t groupId,
                                    const Ep2PortMap& ep2port,
                                    bool onlyPromiscuous = false);

    Agent& agent;
    SwitchConnection* connection;
    FlowExecutor* executor;
    PortMapper *portMapper;
    FlowReader *reader;

    FallbackMode fallbackMode;
    EncapType encapType;
    std::string encapIface;
    FloodScope floodScope;
    boost::asio::ip::address tunnelDst;
    boost::optional<boost::asio::ip::address> mcastTunDst;
    bool virtualRouterEnabled;
    uint8_t routerMac[6];
    TableState flowTables[NUM_FLOW_TABLES];
    std::string flowIdCache;

    /*
     * Map of flood-group URI to the endpoints associated with it.
     * The flood-group can either be a flood-domain or an endpoint-group
     */
    typedef boost::unordered_map<opflex::modb::URI, Ep2PortMap> FloodGroupMap;
    FloodGroupMap floodGroupMap;

    WorkQueue workQ;

    const char * GetIdNamespace(opflex::modb::class_id_t cid);
    IdGenerator idGen;

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
        void GotFlows(int tableNum, const FlowEntryList& flows,
            bool done);

        /**
         * Callback function provided to FlowReader to process received
         * group table entries.
         */
        void GotGroups(const GroupEdit::EntryList& groups,
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
                const Ep2PortMap& epMap, bool prom, GroupEdit& ge);

        FlowManager& flowManager;
        FlowEntryList recvFlows[FlowManager::NUM_FLOW_TABLES];
        bool tableDone[FlowManager::NUM_FLOW_TABLES];

        typedef boost::unordered_map<uint32_t, GroupEdit::Entry> GroupMap;
        GroupMap recvGroups;
        bool groupsDone;

        bool syncInProgress;
        bool syncPending;
    };
    friend class FlowSyncer;

    FlowSyncer flowSyncer;

    volatile bool stopping;

    /**
     * Timer callback that begins reconciliation.
     */
    void OnConnectTimer(const boost::system::error_code& ec);
    boost::scoped_ptr<boost::asio::deadline_timer> connectTimer;
    long connectDelayMs;

    /**
     * Timer callback for router advertisements
     */
    void OnAdvertTimer(const boost::system::error_code& ec);
    boost::scoped_ptr<boost::asio::deadline_timer> advertTimer;
    volatile int initialAdverts;

    bool opflexPeerConnected;

    void initPlatformConfig();
};

} // namespace ovsagent

#endif // OVSAGENT_FLOWMANAGER_H_
