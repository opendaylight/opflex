/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Copyright (c) 2017 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef __VPP_UPLINK_H__
#define __VPP_UPLINK_H__

#include <vom/vxlan_tunnel.hpp>
#include <vom/dhcp_config.hpp>
#include <vom/dhcp_config_cmds.hpp>
#include <vom/tap_interface.hpp>

using namespace VOM;

namespace VPP
{
    /*
     * forward declarations to reduce compile time coupling
     */
    class Interface;

    /**
     * A description of the uplink interface. 
     *  Can be one of VLAN< VXLAN or iVXLAN
     */
    class Uplink
    {
    public:
        /**
         * The uplink interface's encapsulation type for data traffic.
         */
        enum uplink_type_t
        {
            /**
             * VXLAN encap
             */
            VXLAN,
            /**
             * VLAN encap
             */
            VLAN
        };

        /**
         * Default Constructor
         */
        Uplink();

        /**
         * Given the VNID, create aninterface of the appropriate type
         */
        VOM::interface *mk_interface(const std::string &uuid, uint32_t vnid);

        /**
         * Set the uplink paramenters for vxlan
         */
        void set(const std::string &uplink,
                 uint16_t vlan,
                 const std::string &name,
                 const boost::asio::ip::address &ip,
                 uint16_t port);

        /**
         * Set the uplink paramenters for vlan
         */
        void set(const std::string &uplink,
                 uint16_t vlan,
                 const std::string &name);

        /**
         * make the control channel/interfaces
         *
         * @param fqdn Fully Qualifed Domain name
         */
        void configure(const std::string &fqdn);

        /**
         * Handle notifications about DHCP complete
         */
        void handle_dhcp_event(dhcp_config_cmds::events_cmd *cmd);

    private:
        /**
         * Configure the tap interface
         */
        void configure_tap(const route::prefix_t &pfx);

        /**
         * the encap type on the uplinnk
         */
        uplink_type_t m_type;

        /**
         * VXLAN uplink encap, if used
         */
        VOM::vxlan_tunnel::endpoint_t m_vxlan;

        /**
         * A reference to the uplink physical insterface in the OM
         */
        std::shared_ptr<interface> m_uplink;

        /**
         * The VLAN used for control traffic
         */
        uint16_t m_vlan;

        /**
         * The name of the uplink interface
         */
        std::string m_iface;
    };
};

#endif
