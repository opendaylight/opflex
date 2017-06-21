/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Copyright (c) 2014-2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef VPPAGENT_VPPMANAGER_H_
#define VPPAGENT_VPPMANAGER_H_

#include "Agent.h"
#include "SwitchManager.h"
#include "IdGenerator.h"
#include "RDConfig.h"
#include "TaskQueue.h"
#include "VppApi.h"
#include <opflex/ofcore/PeerStatusListener.h>

#include <boost/asio/ip/address.hpp>
#include <boost/optional.hpp>
#include <boost/noncopyable.hpp>

#include <utility>
#include <unordered_map>

namespace ovsagent {

/**
 * @brief Makes changes to VPP to be in sync with state of MOs.
 * Main function is to handling change notifications, generate a set
 * of config modifications that represent the changes and apply these
 * modifications.
 */
class VppManager :     public EndpointListener,
                       public ServiceListener,
                       public ExtraConfigListener,
                       public PolicyListener,
                       public PortStatusListener,
                       public opflex::ofcore::PeerStatusListener,
                       private boost::noncopyable {
public:
    /**
     * Construct a new Vpp manager for the agent
     * @param agent the agent object
     * @param idGen the flow ID generator
     */
    VppManager(Agent& agent,
               IdGenerator& idGen);

    ~VppManager() {}

    /**
     * Module start
     */
    void start(const std::string& name);

    /**
     * Installs listeners for receiving updates to MODB state.
     */
    void registerModbListeners();

    /**
     * Module stop
     */
    void stop();

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
    void setEncapType(EncapType encapType);

    /**
     * Get the encap type to use for packets sent over the network
     * @return the encap type
     */
    EncapType getEncapType() { return encapType; }

    /**
     * Set the openflow interface name for encapsulated packets
     * @param encapIface the interface name
     */
    void setEncapIface(const std::string& encapIface);

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
    void setFloodScope(FloodScope floodScope);

    /**
     * Set the tunnel remote IP and port to use for tunnel traffic
     * @param tunnelRemoteIp the remote tunnel IP
     * @param tunnelRemotePort the remote tunnel port
     */
    void setTunnel(const std::string& tunnelRemoteIp,
                   uint16_t tunnelRemotePort);

    /**
     * Enable connection tracking support
     */
    void enableConnTrack();

    /**
     * Enable or disable the virtual routing
     *
     * @param virtualRouterEnabled true to enable the router
     * @param routerAdv true to enable IPv6 router advertisements
     * @param mac the MAC address to use as the router MAC formatted
     * as a colon-separated string of 6 hex-encoded bytes.
     */
    void setVirtualRouter(bool virtualRouterEnabled,
                          bool routerAdv,
                          const std::string& mac);

    /**
     * Enable or disable the virtual DHCP server
     *
     * @param dhcpEnabled true to enable the server
     * @param mac the MAC address to use as the dhcp MAC formatted as
     * a colon-separated string of 6 hex-encoded bytes.
     */
    void setVirtualDHCP(bool dhcpEnabled,
                        const std::string& mac);

    /**
     * Set the multicast group file
     * @param mcastGroupFile The file where multicast group
     * subscriptions will be written
     */
    void setMulticastGroupFile(const std::string& mcastGroupFile);

    /**
     * Get the interface index that maps to the configured tunnel
     * interface
     * @return the interface index
     */
    uint32_t getTunnelIntfIndex();

    /**
     * Get the configured tunnel destination as a parsed IP address
     * @return the tunnel destination
     */
    boost::asio::ip::address& getTunnelDst() { return tunnelDst; }

    /**
     * Get the router MAC address as an array of 6 bytes
     * @return the router MAC
     */
    const uint8_t *getRouterMacAddr() { return routerMac; }

    /**
     * Get the DHCP MAC address as an array of 6 bytes
     * @return the DHCP MAC
     */
    const uint8_t *getDHCPMacAddr() { return dhcpMac; }

    /* Interface: EndpointListener */
    virtual void endpointUpdated(const std::string& uuid);

    /* Interface: ServiceListener */
    virtual void serviceUpdated(const std::string& uuid);

    /* Interface: ExtraConfigListener */
    virtual void rdConfigUpdated(const opflex::modb::URI& rdURI);

    /* Interface: PolicyListener */
    virtual void egDomainUpdated(const opflex::modb::URI& egURI);
    virtual void domainUpdated(opflex::modb::class_id_t cid,
                               const opflex::modb::URI& domURI);
    virtual void contractUpdated(const opflex::modb::URI& contractURI);
    virtual void configUpdated(const opflex::modb::URI& configURI);

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
        const std::unordered_set<opflex::modb::URI>& uris,
        /* out */std::unordered_map<uint32_t, uint32_t>& ids);

    /**
     * Get or generate a unique ID for a given object for use with flows.
     *
     * @param cid Class ID of the object
     * @param uri URI of the object
     * @return A unique ID for the object
     */
    uint32_t getId(opflex::modb::class_id_t cid, const opflex::modb::URI& uri);

    /**
     * Get the promiscuous-mode ID equivalent for a flood domain ID
     * @param fgrpId the flood domain Id
     */
    static uint32_t getPromId(uint32_t fgrpId);

    /**
     * Get the tunnel destination to use for the given endpoint group.
     * @param epgURI the group URI
     * @return the tunnel destination IP
     */
    boost::asio::ip::address getEPGTunnelDst(const opflex::modb::URI& epgURI);

private:
    /**
     * Compare and update changes in an endpoint.
     *
     * @param uuid UUID of the changed endpoint
     */
    void handleEndpointUpdate(const std::string& uuid);

    /**
     * Compare and update changes in an anycast service.
     *
     * @param uuid UUID of the changed anycast service
     */
    void handleAnycastServiceUpdate(const std::string& uuid);

    /**
     * Compare and update changes in an endpoint group.
     *
     * @param egURI URI of the changed endpoint group
     */
    void handleEndpointGroupDomainUpdate(const opflex::modb::URI& egURI);

    /**
     * Update given routing domain
     *
     * @param rdURI URI of the changed routing domain
     */
    void handleRoutingDomainUpdate(const opflex::modb::URI& rdURI);

    /**
     * Handle changes to a forwarding domain; only deals with
     * cleaning up when these objects are removed.
     *
     * @param cid Class of the forwarding domain
     * @param domURI URI of the changed forwarding domain
     */
    void handleDomainUpdate(opflex::modb::class_id_t cid,
                            const opflex::modb::URI& domURI);

    /**
     * Compare and update changes in a contract
     *
     * @param contractURI URI of the changed contract
     */
    void handleContractUpdate(const opflex::modb::URI& contractURI);

    /**
     * Compare and update changes in platform config
     *
     * @param configURI URI of the changed contract
     */
    void handleConfigUpdate(const opflex::modb::URI& configURI);

    /**
     * Handle changes to port-status for endpoints and endpoint groups.
     *
     * @param portName Name of the port that changed
     * @param portNo Port number of the port that changed
     */
    void handlePortStatusUpdate(const std::string& portName, uint32_t portNo);

    bool getGroupForwardingInfo(const opflex::modb::URI& egUri, uint32_t& vnid,
            boost::optional<opflex::modb::URI>& rdURI, uint32_t& rdId,
            boost::optional<opflex::modb::URI>& bdURI, uint32_t& bdId,
            boost::optional<opflex::modb::URI>& fdURI, uint32_t& fdId);
    void updateGroupSubnets(const opflex::modb::URI& egUri,
                            uint32_t bdId, uint32_t rdId);
    void updateEPGFlood(const opflex::modb::URI& epgURI,
                        uint32_t epgVnid, uint32_t fgrpId,
                        boost::asio::ip::address epgTunDst);


    /**
     * Associate an endpoint with a flood-group.
     *
     * @param fgrpURI URI of flood-group (flood-domain or endpoint-group)
     * @param endpoint The endpoint to update
     * @param epPort Port number of endpoint
     * @param isPromiscuous whether the endpoint port is promiscuous
     * @param fd Flood-domain to which the endpoint belongs
     */
    void updateEndpointFloodGroup(const opflex::modb::URI& fgrpURI,
                                  const Endpoint& endPoint,
                                  uint32_t epPort,
                                  bool isPromiscuous,
                                  boost::optional<std::shared_ptr<
                                      modelgbp::gbp::FloodDomain> >& fd);

    /**
     * Dis-associate an endpoint from any flood-group.
     *
     * @param epUUID UUID of endpoint
     */
    void removeEndpointFromFloodGroup(const std::string& epUUID);

    /*
     * Map of endpoint to the port it is using.
     */
    typedef std::unordered_map<std::string,
                               std::pair<uint32_t, bool> > Ep2PortMap;

    Agent& agent;
    VppApi vppApi;
    IdGenerator& idGen;
    TaskQueue taskQueue;

    EncapType encapType;
    std::string encapIface;
    FloodScope floodScope;
    boost::asio::ip::address tunnelDst;
    std::string tunnelPortStr;
    bool virtualRouterEnabled;
    uint8_t routerMac[6];
    bool routerAdv;
    bool virtualDHCPEnabled;
    bool conntrackEnabled;
    uint8_t dhcpMac[6];
    std::string mcastGroupFile;

    /*
     * Map of flood-group URI to the endpoints associated with it.
     * The flood-group can either be a flood-domain or an endpoint-group
     */
    typedef std::unordered_map<opflex::modb::URI, Ep2PortMap> FloodGroupMap;
    FloodGroupMap floodGroupMap;

    const char * getIdNamespace(opflex::modb::class_id_t cid);

    bool isSyncing;

    uint32_t getExtNetVnid(const opflex::modb::URI& uri);

    volatile bool stopping;

    void initPlatformConfig();

    /* Set of URIs of managed objects */
    typedef std::unordered_set<opflex::modb::URI> UriSet;
    /* Map of multi-cast IP addresses to associated managed objects */
    typedef std::unordered_map<std::string, UriSet> MulticastMap;
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

#endif // VPPAGENT_VPPMANAGER_H_
