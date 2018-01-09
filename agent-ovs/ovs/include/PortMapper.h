/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef OPFLEXAGENT_PORTMAPPER_H_
#define OPFLEXAGENT_PORTMAPPER_H_

#include "SwitchConnection.h"
#include "ovs-shim.h"

#include <string>
#include <unordered_map>
#include <mutex>

struct ofpbuf;

namespace opflexagent {

/**
 * @brief Abstract base-class for handling port-status change events.
 */
class PortStatusListener {
public:
    /**
     * Called when there is a change to a port on the switch.
     *
     * @param portName Name of the port that changed
     * @param portNo Port number of the port that changed
     * @param fromDesc true if the update is from a description reply
     */
    virtual void portStatusUpdate(const std::string& portName,
                                  uint32_t portNo,
                                  bool fromDesc) = 0;
};

/**
 * @brief Class that maps OpenFlow port-names on a switch to the
 * corresponding port-numbers.
 */

class PortMapper : public MessageHandler,
                   public OnConnectListener {
public:
    PortMapper();
    virtual ~PortMapper();

    /**
     * Return the OpenFlow port-number for the provided port-name.
     * @return Valid port-number if found, OFPP_NONE otherwise.
     */
    virtual uint32_t FindPort(const std::string& name);

    /**
     * Return the OpenFlow port name for the provided port number
     * @return the string port name
     * @throws std::out_of_range if there is no such port known
     */
    virtual const std::string& FindPort(uint32_t of_port_no);

    /**
     * Register handler for port-status events notifications.
     *
     * @param l Listener to register
     */
    void registerPortStatusListener(PortStatusListener *l);

    /**
     * Unregister a previously registered port-status listener.
     *
     * @param l Listener to unregister
     */
    void unregisterPortStatusListener(PortStatusListener *l);

    /**
     * Register all the necessary event listeners on connection.
     * @param conn Connection to register
     */
    void InstallListenersForConnection(SwitchConnection *conn);

    /**
     * Unregister all event listeners from connection.
     * @param conn Connection to unregister from
     */
    void UninstallListenersForConnection(SwitchConnection *conn);

    /** Interface: MessageHandler */
    void Handle(SwitchConnection *swConn, int type, ofpbuf *msg);

    /** Interface: OnConnectListener */
    void Connected(SwitchConnection *swConn);

private:
    /**
     * Update port-map with information received in the port description
     * message.
     * Port info is first put in a temporary map which gets moved to the
     * main map once the outstanding port description request is complete.
     */
    void HandlePortDescReply(ofpbuf *msg);

    /**
     * Update port-map based on port-status notifications.
     */
    void HandlePortStatus(ofpbuf *msg);

    /**
     * Notify event listeners about change in status of a port.
     *
     * @param portName Name of the port that changed
     * @param portNo Port number of the port that changed
     * @param fromDesc true if the update is from a description reply
     */
    void notifyListeners(const std::string& portName, uint32_t portNo,
                         bool fromDesc);

    typedef std::unordered_map<std::string, PhyPortP> PortMap;
    typedef std::unordered_map<uint32_t, std::string> RPortMap;
    PortMap portMap;
    RPortMap rportMap;
    PortMap tmpPortMap;
    RPortMap tmprPortMap;

    uint32_t lastDescReqXid;

    typedef std::list<PortStatusListener *>  PortStatusList;
    PortStatusList portStatusListeners;

    std::mutex mapMtx;
};

} // namespace opflexagent

#endif // OPFLEXAGENT_PORTMAPPER_H_
