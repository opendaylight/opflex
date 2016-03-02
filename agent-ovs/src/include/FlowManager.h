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

#include <opflex/ofcore/PeerStatusListener.h>

#include "Agent.h"
#include "SwitchConnection.h"
#include "PortMapper.h"
#include "FlowReader.h"
#include "FlowExecutor.h"
#include "IdGenerator.h"
#include "ActionBuilder.h"
#include "PacketInHandler.h"
#include "AdvertManager.h"
#include "RDConfig.h"

namespace ovsagent {

/**
 * @brief Makes changes to OpenFlow tables to be in sync with state
 * of MOs.
 * Main function is to handling change notifications, generate a set
 * of flow modifications that represent the changes and apply these
 * modifications.
 */
class FlowManager : public EndpointListener,
                    public ServiceListener,
                    public ExtraConfigListener,
                    public PolicyListener,
                    public OnConnectListener,
                    public PortStatusListener,
                    public opflex::ofcore::PeerStatusListener {
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
     * Get the port mapper
     * @return the port mapper
     */
    PortMapper* GetPortMapper() { return portMapper; }

    /**
     * Set the object used for reading flows and groups from the switch.
     *
     * @param r The reader object
     */
    void SetFlowReader(FlowReader *r);

    /**
     * Register the given connection with the flow manager. Installs
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
     * Get the encap type to use for packets sent over the network
     * @return the encap type
     */
    EncapType GetEncapType() { return encapType; }

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
     * Set the remote port to use for tunnel traffic
     * @param tunnelRemotePort the remote tunnel port
     */
    void setTunnelRemotePort(uint16_t tunnelRemotePort);

    /**
     * Enable or disable the virtual routing
     *
     * @param virtualRouterEnabled true to enable the router
     * @param routerAdv true to enable IPv6 router advertisements
     * @param mac the MAC address to use as the router MAC formatted
     * as a colon-separated string of 6 hex-encoded bytes.
     */
    void SetVirtualRouter(bool virtualRouterEnabled,
                          bool routerAdv,
                          const std::string& mac);

    /**
     * Enable or disable the virtual DHCP server
     *
     * @param dhcpEnabled true to enable the server
     * @param mac the MAC address to use as the dhcp MAC formatted as
     * a colon-separated string of 6 hex-encoded bytes.
     */
    void SetVirtualDHCP(bool dhcpEnabled,
                        const std::string& mac);

    /**
     * Enable or disable endpoint advertisements
     *
     * @param endpointAdv true to enable periodic endpoint
     * advertisements
     */
    void SetEndpointAdv(bool endpointAdv);

    /**
     * Set the flow ID cache directory
     * @param flowIdCache the directory where flow ID entries will be
     * cached
     */
    void SetFlowIdCache(const std::string& flowIdCache);

    /**
     * Set the multicast group file
     * @param mcastGroupFile The file where multicast group
     * subscriptions will be written
     */
    void SetMulticastGroupFile(const std::string& mcastGroupFile);

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
     * Get the multicast tunnel destination
     * @return the tunnel destination
     */
    boost::asio::ip::address& GetMcastTunDst()
    { return mcastTunDst ? mcastTunDst.get() : tunnelDst; }

    /**
     * Get the router MAC address as an array of 6 bytes
     * @return the router MAC
     */
    const uint8_t *GetRouterMacAddr() { return routerMac; }

    /**
     * Get the DHCP MAC address as an array of 6 bytes
     * @return the DHCP MAC
     */
    const uint8_t *GetDHCPMacAddr() { return dhcpMac; }

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
    virtual void endpointUpdated(const std::string& uuid);

    /* Interface: ServiceListener */
    virtual void anycastServiceUpdated(const std::string& uuid);

    /* Interface: ExtraConfigListener */
    virtual void rdConfigUpdated(const opflex::modb::URI& rdURI);

    /* Interface: PolicyListener */
    virtual void egDomainUpdated(const opflex::modb::URI& egURI);
    virtual void domainUpdated(opflex::modb::class_id_t cid,
                               const opflex::modb::URI& domURI);
    virtual void contractUpdated(const opflex::modb::URI& contractURI);
    virtual void configUpdated(const opflex::modb::URI& configURI);

    /* Interface: OnConnectListener */
    virtual void Connected(SwitchConnection *swConn);

    /* Interface: PortStatusListener */
    virtual void portStatusUpdate(const std::string& portName, uint32_t portNo,
                                  bool fromDesc);

    /**
     * Implementation for PeerStatusListener::peerStatusUpdated
     *
     * @param peerHostname the host name for the connection
     * @param peerPort the port number for the connection
     * @param peerStatus the new status for the connection
     */
    virtual void peerStatusUpdated(const std::string& peerHostname,
                                   int peerPort,
                                   PeerStatus peerStatus);

    /**
     * Get the VNID and routing-domain ID for the specified endpoint
     * groups or L3 external networks
     *
     * @param uris URIs of endpoint groups to search for
     * @param ids Map of VNIDs-to-RoutingDomainID of the groups which
     * have a VNID. Routing-domain ID is set to 0 if the group is not
     * part of a routing-domain
     */
    void getGroupVnidAndRdId(
        const boost::unordered_set<opflex::modb::URI>& uris,
        /* out */boost::unordered_map<uint32_t, uint32_t>& ids);

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
     * Get the cookie used for flows that direct neighbor discovery
     * packets to the controller
     *
     * @return flow-cookie for ND packets
     */
    static ovs_be64 GetNDCookie();

    /**
     * Get the cookie used for flows that direct DHCP packets to the
     * controller
     *
     * @param v4 true for dhcpv4, false for v6
     * @return flow-cookie for DHCP packets
     */
    static ovs_be64 GetDHCPCookie(bool v4 = true);

    /**
     * Get the cookie used for flows that direct virtual IP
     * announcement packets to the controller
     *
     * @param v4 true for ARP, false for neighbor discovery
     * @return flow-cookie for VIP announcements
     */
    static ovs_be64 GetVIPCookie(bool v4 = true);

    /**
     * Get the cookie used for flows that direct ICMP error messages
     * that require body translation to the controller
     *
     * @param v4 true for ICMPv4, false for ICMPv6
     * @return flow-cookie for ICMP Errors
     */
    static ovs_be64 GetICMPErrorCookie(bool v4 = true);

    /**
     * Set fill in tunnel metadata in an action builder
     * @param ab the action builder
     * @param type the encap type
     * @param tunDst the tunnel destination, or boost::none to send to the
     * tunnel dst in reg7.
     */
    static void
    SetActionTunnelMetadata(ActionBuilder& ab, FlowManager::EncapType type,
                            const boost::optional<boost::asio::ip::address>&
                            tunDst = boost::none);

    /**
     * Get the promiscuous-mode ID equivalent for a flood domain ID
     * @param fgrpId the flood domain Id
     */
    static uint32_t getPromId(uint32_t fgrpId);

    /**
     * Maximum flow priority of the entries in policy table.
     */
    static const uint16_t MAX_POLICY_RULE_PRIORITY;

    /**
     * Indicate that the agent is connected to its Opflex peer.
     * Note: This method should not be invoked directly except for purposes
     * of unit-testing.
     */
    void PeerConnected();

    /**
     * Enqueue a task to be executed asynchronously on the flow
     * manager's task queue
     *
     * @param w the work item to enqueue
     */
    void QueueFlowTask(const boost::function<void ()>& w);

    /**
     * Compare the state of a give table against the provided
     * snapshot.
     *
     * @param tableId the table to compare against
     * @param el the list of flows to compare against
     * @param diffs returns the differences between the entries
     */
    void DiffTableState(int tableId, const FlowEntryList& el,
                        /* out */ FlowEdit& diffs);

    /**
     * Get the tunnel destination to use for the given endpoint group.
     * @param epgURI the group URI
     * @return the tunnel destination IP
     */
    boost::asio::ip::address getEPGTunnelDst(const opflex::modb::URI& epgURI);

    /**
     * Indices of tables managed by the flow-manager.
     */
    enum {
        /**
         * Handles port security/ingress policy
         */
        SEC_TABLE_ID,
        /**
         * Maps source addresses to endpoint groups and sets this
         * mapping into registers for use by later tables
         */
        SRC_TABLE_ID,
        /**
         * For flows that can be forwarded by bridging, maps the
         * destination L2 address to an endpoint group and next hop
         * interface and sets this mapping into registers for use by
         * later tables.  Also handles replies to protocols handled by
         * the agent or switch, such as ARP and NDP.
         */
        BRIDGE_TABLE_ID,
        /**
         * For flows that require routing, maps the destination L3
         * address to an endpoint group or external network and next
         * hop action and sets this information into registers for use
         * by later tables.
         */
        ROUTE_TABLE_ID,
        /**
         * For flows destined for a NAT IP address, determine the
         * source external network for the mapped IP address and set
         * this in the source registers to allow applying policy to
         * NATed flows.
         */
        NAT_IN_TABLE_ID,
        /**
         * If the bridge domain is configured to flood packets (such
         * as is required to enable transparent layer 2 services),
         * this table handles the MAC learning using a simple reactive
         * model.
         */
        LEARN_TABLE_ID,
        /**
         * Map traffic returning from a service interface to the
         * appropriate endpoint interface.
         */
        SERVICE_MAP_DST_TABLE_ID,
        /**
         * Allow policy for the flow based on the source and
         * destination groups and the contracts that are configured.
         */
        POL_TABLE_ID,
        /**
         * Apply a destination action based on the action set in the
         * metadata field.
         */
        OUT_TABLE_ID,
        /**
         * The total number of flow tables
         */
        NUM_FLOW_TABLES
    };

    /**
     * "Policy applied" bit.  Bypass policy table because policy has
     * already been applied.
     */
    static const uint64_t METADATA_POLICY_APPLIED_MASK;

    /**
     * the OUT_MASK specifies 8 bits that indicate the action to take
     * in the output table.  If nothing is set, then the action is to
     * output to the interface in REG7
     */
    static const uint64_t METADATA_OUT_MASK;

    /**
     * Resubmit to the first "dest" table with the source registers
     * set to the corresponding values for the EPG in REG7
     */
    static const uint64_t METADATA_RESUBMIT_DST;

    /**
     * Perform "outbound" NAT action and then resubmit with the source
     * EPG set to the mapped NAT EPG
     */
    static const uint64_t METADATA_NAT_OUT;

    /**
     * Output to the interface in REG7 but intercept ICMP error
     * replies and overwrite the encapsulated error packet source
     * address with the (rewritten) destination address of the outer
     * packet.
     */
    static const uint64_t METADATA_REV_NAT_OUT;

    /**
     * Output to the tunnel destination appropriate for the EPG
     */
    static const uint64_t METADATA_TUNNEL_OUT;

    /**
     * Output to the flood group appropriate for the EPG
     */
    static const uint64_t METADATA_FLOOD_OUT;

private:
    /**
     * Compare and update flow/group tables due to changes in an endpoint.
     *
     * @param uuid UUID of the changed endpoint
     */
    void HandleEndpointUpdate(const std::string& uuid);

    /**
     * Compare and update flow/group tables due to changes in an
     * anycast service.
     *
     * @param uuid UUID of the changed anycast service
     */
    void HandleAnycastServiceUpdate(const std::string& uuid);

    /**
     * Compare and update flow/group tables due to changes in an
     * endpoint group.
     *
     * @param egURI URI of the changed endpoint group
     */
    void HandleEndpointGroupDomainUpdate(const opflex::modb::URI& egURI);

    /**
     * Update flows related to the given routing domain
     *
     * @param rdURI URI of the changed routing domain
     */
    void HandleRoutingDomainUpdate(const opflex::modb::URI& rdURI);

    /**
     * Update flows related to the given flood domain
     *
     * @param fdURI URI of the changed flood domain
     */
    void HandleFloodDomainUpdate(const opflex::modb::URI& fdURI);

    /**
     * Handle changes to a forwarding domain; only deals with
     * cleaning up flows etc when these objects are removed.
     *
     * @param cid Class of the forwarding domain
     * @param domURI URI of the changed forwarding domain
     */
    void HandleDomainUpdate(opflex::modb::class_id_t cid,
                            const opflex::modb::URI& domURI);

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
            boost::optional<opflex::modb::URI>& rdURI, uint32_t& rdId,
            boost::optional<opflex::modb::URI>& bdURI, uint32_t& bdId,
            boost::optional<opflex::modb::URI>& fdURI, uint32_t& fdId);
    void UpdateGroupSubnets(const opflex::modb::URI& egUri,
                            uint32_t bdId, uint32_t rdId);
    void UpdateEPGFlood(const opflex::modb::URI& epgURI,
                        uint32_t epgVnid, uint32_t fgrpId,
                        boost::asio::ip::address epgTunDst);
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
     * @param allow true if the traffic should be allowed, false otherwise
     * @param priority Priority of the entry created
     * @param cookie Cookie of the entry created
     * @param svnid VNID of the source endpoint group for the entry
     * @param dvnid VNID of the destination endpoint group for the entry
     * @param srdid RoutingDomain ID of the source endpoint group
     * @param entries List to append entry to
     */
    void AddEntryForClassifier(modelgbp::gbpe::L24Classifier *classifier,
                               bool allow, uint16_t priority, uint64_t cookie,
                               uint32_t svnid, uint32_t dvnid, uint32_t srdid,
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
    std::string tunnelPortStr;
    boost::optional<boost::asio::ip::address> mcastTunDst;
    bool virtualRouterEnabled;
    uint8_t routerMac[6];
    bool routerAdv;
    bool virtualDHCPEnabled;
    uint8_t dhcpMac[6];
    TableState flowTables[NUM_FLOW_TABLES];
    std::string flowIdCache;
    std::string mcastGroupFile;

    boost::mutex queueMutex;
    boost::unordered_set<std::string> queuedItems;

    /*
     * Map of flood-group URI to the endpoints associated with it.
     * The flood-group can either be a flood-domain or an endpoint-group
     */
    typedef boost::unordered_map<opflex::modb::URI, Ep2PortMap> FloodGroupMap;
    FloodGroupMap floodGroupMap;

    const char * GetIdNamespace(opflex::modb::class_id_t cid);
    IdGenerator idGen;

    bool isSyncing;

    uint32_t getExtNetVnid(const opflex::modb::URI& uri);

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

    PacketInHandler pktInHandler;
    AdvertManager advertManager;

    volatile bool stopping;

    /**
     * Timer callback that begins reconciliation.
     */
    void OnConnectTimer(const boost::system::error_code& ec);
    boost::scoped_ptr<boost::asio::deadline_timer> connectTimer;
    long connectDelayMs;

    /**
     * Timer callback to clean up IDs that have been erased
     */
    void OnIdCleanupTimer(const boost::system::error_code& ec);
    boost::scoped_ptr<boost::asio::deadline_timer> idCleanupTimer;

    bool opflexPeerConnected;

    void initPlatformConfig();

    /* Set of URIs of managed objects */
    typedef boost::unordered_set<opflex::modb::URI> UriSet;
    /* Map of multi-cast IP addresses to associated managed objects */
    typedef boost::unordered_map<std::string, UriSet> MulticastMap;
    MulticastMap mcastMap;

    /**
     * Associate or disassociate a managed object with a multicast IP, and
     * update the multicast group subscription if necessary.
     * @param mcastIp The multicast IP to associate with; if unset disassociates
     * any previous association
     * @param uri URI of the managed object to associate to
     */
    void updateMulticastList(const boost::optional<std::string>& mcastIp,
                             const opflex::modb::URI& uri);

    /**
     * Remove all multicast IP associations for a managed object.
     * @param uri URI of the managed object to disassociate
     * @return true if an update should be queued
     */
    bool removeFromMulticastList(const opflex::modb::URI& uri);

    /**
     * Queue a write of the current multicast subscriptions
     */
    void multicastGroupsUpdated();

    /**
     * Write out the current multicast subscriptions
     */
    void writeMulticastGroups();
};

} // namespace ovsagent

#endif // OVSAGENT_FLOWMANAGER_H_
