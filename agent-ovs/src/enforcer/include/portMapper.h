/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */


#ifndef _PORTMAPPER_H_
#define _PORTMAPPER_H_

#include <string>
#include <boost/unordered_map.hpp>
#include <boost/thread/mutex.hpp>

#include <switchConnection.h>
#include <ovs.h>

namespace opflex {
namespace enforcer {

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
    void Handle(SwitchConnection *swConn, ofptype type, ofpbuf *msg);

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

    typedef boost::unordered_map<std::string, ofputil_phy_port> PortMap;
    PortMap portMap;
    PortMap tmpPortMap;

    ovs_be32 lastDescReqXid;

    boost::mutex mapMtx;
};

}   // namespace enforcer
}   // namespace opflex

#endif // _PORTMAPPER_H_
