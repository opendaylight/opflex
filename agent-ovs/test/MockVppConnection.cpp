/*
 * Copyright (c) 2017 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <string.h>
#include <iostream>
#include <iomanip>
#include <stdlib.h>
#include <signal.h>
#include <netinet/in.h>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/assign.hpp>


#include <utility>
#include <functional>
#include "logging.h"

//Debugging
#include <unistd.h>
// End debugging includes

#include "MockVppConnection.h"

using namespace std;

using boost::asio::ip::address;
using boost::asio::ip::address_v4;
using boost::asio::ip::address_v6;

namespace fs = boost::filesystem;
namespace pt = boost::property_tree;



namespace ovsagent {

  template <class T>
  T * setResponse (T &_mp, u16 u, u32 context) {
    T *mp;
    mp = &_mp;
    memset (mp, 0, sizeof (*mp));
    mp->_vl_msg_id = ntohs (u);
    mp->context = context;
    return mp;
  }


  MockVppConnection::MockVppConnection() {
  }

  MockVppConnection::~MockVppConnection() {
  }

  int MockVppConnection::lockedWrite(char* msg, int size)
  {
    vl_api_control_ping_t *cmp;
    // Can't call this function without an object (see static)
    //int controlPingReplyID = getMessageIdByName({"control_ping_reply_aa016e7b"});
    int controlPingReplyID = 493; //TODO alagalah <- only until work out static. won't work on
    // other installs of VPP.

    /*
     * Will extend it using switch case
     */

    cmp = (vl_api_control_ping_t *)msg;
    u16 msgId = ntohs (cmp->_vl_msg_id);
    u32 context = cmp->context;

    switch (msgId) {
    case 1: {
      vl_api_show_version_t *rmp;
      vl_api_show_version_reply_t _mp, *mp;
      mp = setResponse(_mp, getMessageIdByName("show_version_reply"), context);
      rmp = (vl_api_show_version_t *)msg;
      mp->retval = 0;
      const char *version="17.04";
      const char *program="vpp";
      const char *build_directory="somewhere";
      const char *build_date = "soon";
      strcpy((char*)mp->build_directory, build_directory);
      strcpy((char*)mp->build_date, build_date);
      strcpy((char*)mp->program, program);
      strcpy((char*)mp->version, version);
      VppConnection::vppMessageReceiveHandler(std::move((char *)mp), sizeof(*mp));
      break;
    }
    case 3: {
        vl_api_bridge_domain_add_del_t *rmp;
        vl_api_bridge_domain_add_del_reply_t _mp, *mp;
        mp = setResponse(_mp, getMessageIdByName("bridge_domain_add_del_reply"), context);
        rmp = (vl_api_bridge_domain_add_del_t *)msg;
        if (rmp->bd_id > 0) {
            mp->retval = 0;
        } else {
            mp->retval = htonl(INVALID_VALUE);
        }
        VppConnection::vppMessageReceiveHandler(std::move((char *)mp), sizeof(*mp));
        break;
    }
    case 5: {
        vl_api_af_packet_create_t *rmp;
        vl_api_af_packet_create_reply_t _mp, *mp;
        string intfName;
        string const name{"veth1-test"};
        u32 sw_if_index = 1;
        mp = setResponse(_mp, getMessageIdByName("af_packet_create_reply"), context);
        rmp = (vl_api_af_packet_create_t *)msg;
        if (strcmp((char *)rmp->host_if_name, name.c_str()) == 0) {
            mp->sw_if_index = htonl(sw_if_index);
            mp->retval = 0;
        } else {
            mp->retval = htonl(INVALID_VALUE);
        }
        VppConnection::vppMessageReceiveHandler(std::move((char *)mp), sizeof(*mp));
        break;
    }
    case 7: {
        vl_api_af_packet_delete_t *rmp;
        vl_api_af_packet_delete_reply_t _mp, *mp;
        string intfName;
        string const name{"veth1-test"};
        u32 sw_if_index = 1;
        mp = setResponse(_mp, getMessageIdByName("af_packet_delete_reply"), context);
        rmp = (vl_api_af_packet_delete_t *)msg;
        if (strcmp((char *)rmp->host_if_name, name.c_str()) == 0) {
            mp->retval = 0;
        } else {
            mp->retval = htonl(INVALID_VALUE);
        }
        VppConnection::vppMessageReceiveHandler(std::move((char *)mp), sizeof(*mp));
        break;
    }
    case 9: {
        vl_api_sw_interface_set_l2_bridge_t *rmp;
        vl_api_sw_interface_set_l2_bridge_reply_t _mp, *mp;
        mp = setResponse(_mp, getMessageIdByName("sw_interface_set_l2_bridge_reply"), context);
        rmp = (vl_api_sw_interface_set_l2_bridge_t *)msg;
        if (ntohl(rmp->rx_sw_if_index) == 1 && ntohl(rmp->bd_id) == 1) {
            mp->retval = 0;
        } else {
            mp->retval = htonl(INVALID_VALUE);
        }
        VppConnection::vppMessageReceiveHandler(std::move((char *)mp), sizeof(*mp));
        break;
    }
    case 11: {
        vl_api_sw_interface_add_del_address_t *rmp;
        vl_api_sw_interface_add_del_address_reply_t _mp, *mp;
        mp = setResponse(_mp, getMessageIdByName("sw_interface_add_del_address_reply"), context);
        rmp = (vl_api_sw_interface_add_del_address_t *)msg;
        ip4_address ip4;
        u8 mask = 24;
        if (ntohl(rmp->sw_if_index) == 1) {
            array<uint8_t, 4> as_u8 = boost::asio::ip::address::from_string("192.168.10.10").to_v4().to_bytes();
            ip4.as_u8[0] = as_u8[0];
            ip4.as_u8[1] = as_u8[1];
            ip4.as_u8[2] = as_u8[2];
            ip4.as_u8[3] = as_u8[3];

            if (rmp->is_add == 1 && memcmp(rmp->address, &ip4.as_u32, sizeof(u32)) == 0
                && rmp->address_length == mask) {
                mp->retval = 0;
            } else {
                mp->retval = htonl(ADDRESS_NOT_FOUND_FOR_INTERFACE);
            }
        } else {
            mp->retval = htonl(INVALID_SW_IF_INDEX);
        }
        VppConnection::vppMessageReceiveHandler(std::move((char *)mp), sizeof(*mp));
        break;
    }
    case 13: {
        vl_api_sw_interface_set_flags_t *rmp;
        vl_api_sw_interface_set_flags_reply_t _mp, *mp;
        mp = setResponse(_mp, getMessageIdByName("sw_interface_set_flags_reply"), context);
        rmp = (vl_api_sw_interface_set_flags_t *)msg;
        if (ntohl(rmp->sw_if_index) == 1 && rmp->admin_up_down == 1
            && rmp->link_up_down == 1) {
            mp->retval = 0;
        } else {
            mp->retval = htonl(INVALID_VALUE);
        }
        VppConnection::vppMessageReceiveHandler(std::move((char *)mp), sizeof(*mp));
        break;
    }
    case 15: {
        vl_api_create_loopback_t *rmp;
        vl_api_create_loopback_reply_t _mp, *mp;
        u32 sw_if_index = 1;
        mac_addr_t macAddr;
        macAddr[0] = 0xde; macAddr[1] = 0xad; macAddr[2] = 0xbe;
        macAddr[3] = 0xef; macAddr[4] = 0xff; macAddr[5] = 0x01;
        mp = setResponse(_mp, getMessageIdByName("create_loopback_reply"), context);
        rmp = (vl_api_create_loopback_t *)msg;
        if (memcmp(rmp->mac_address, macAddr, sizeof(macAddr))) {
            mp->sw_if_index = htonl(sw_if_index);
            mp->retval = 0;
        } else {
            mp->retval = htonl(INVALID_VALUE);
        }
        VppConnection::vppMessageReceiveHandler(std::move((char *)mp), sizeof(*mp));
        break;
    }
    case 17: {
        // FIXME Not implemented yet
        break;
    }
    case 19: {
        vl_api_vxlan_add_del_tunnel_t *rmp;
        vl_api_vxlan_add_del_tunnel_reply_t _mp,*mp;
        u32 sw_if_index = 1;
        mp = setResponse(_mp, getMessageIdByName("vxlan_add_del_tunnel_reply"), context);
        rmp = (vl_api_vxlan_add_del_tunnel_t *)msg;
        if (rmp->is_add) {
            mp->sw_if_index = htonl(sw_if_index);
            mp->retval = 0;
        } else {
            mp->retval = htonl(INVALID_VALUE);
        }
        VppConnection::vppMessageReceiveHandler(std::move((char *)mp), sizeof(*mp));
        break;
    }
    case 21: {
        vl_api_sw_interface_set_table_reply_t _mp, *mp;
        vl_api_sw_interface_set_table_t *rmp;
        mp = setResponse(_mp, getMessageIdByName("sw_interface_set_table_reply"), context);
        rmp = (vl_api_sw_interface_set_table_t *)msg;
        u32 sw_if_index = 1;
        u8 vrf_id = 1;
        if (ntohl(rmp->sw_if_index) == sw_if_index && rmp->vrf_id == vrf_id) {
            mp->retval = 0;
        } else {
            mp->retval = htonl(INVALID_VALUE);
        }
        VppConnection::vppMessageReceiveHandler(std::move((char *)mp), sizeof(*mp));
        break;
    }
    case 23: {
        // FIXME Not implemented yet
        break;
    }
    default:
        cout << " Bad API msg " << endl;
    }
    return 0;
  }

  int MockVppConnection::lockedConnect(string name, pneum_callback_t cb)
  {
    return 0;
  }

  void MockVppConnection::lockedDisconnect()
  {
  }

  int MockVppConnection::populateMessageIds()
  {
    VppConnection::messageIds =
      boost::assign::list_of <VppConnection::messageIdsBiMap::relation>
      ((u16)1, "show_version")
      ((u16)2, "show_version_reply")
      ((u16)3, "bridge_domain_add_del")
      ((u16)4, "bridge_domain_add_del_reply")
      ((u16)5, "af_packet_create")
      ((u16)6, "af_packet_create_reply")
      ((u16)7, "af_packet_delete")
      ((u16)8, "af_packet_delete_reply")
      ((u16)9, "sw_interface_set_l2_bridge")
      ((u16)10, "sw_interface_set_l2_bridge_reply")
      ((u16)11, "sw_interface_add_del_address")
      ((u16)12, "sw_interface_add_del_address_reply")
      ((u16)13, "sw_interface_set_flags")
      ((u16)14, "sw_interface_set_flags_reply")
      ((u16)15, "create_loopback")
      ((u16)16, "create_loopback_reply")
      ((u16)17, "delete_loopback")
      ((u16)18, "delete_loopback_reply")
      ((u16)19, "vxlan_add_del_tunnel")
      ((u16)20, "vxlan_add_del_tunnel_reply")
      ((u16)21, "sw_interface_set_table")
      ((u16)22, "sw_interface_set_table_reply")
      ((u16)23, "ip_add_del_route")
      ((u16)24, "ip_add_del_route_reply_reply");

    VppConnection::messageCRCByName = {          \
      {"show_version", "12345678"},                                       \
      {"show_version_reply", "12345678"},                                 \
      {"bridge_domain_add_del", "12345678"},				  \
      {"bridge_domain_add_del_reply", "12345678"},			  \
      {"af_packet_create", "12345678"},					  \
      {"af_packet_create_reply", "12345678"},				  \
      {"af_packet_delete", "12345678"},                                   \
      {"af_packet_delete_reply", "12345678"},                             \
      {"sw_interface_set_l2_bridge", "12345678"},			  \
      {"sw_interface_set_l2_bridge_reply", "12345678"},			  \
      {"sw_interface_add_del_address", "12345678"},			  \
      {"sw_interface_add_del_address_reply", "12345678"},		  \
      {"sw_interface_set_flags", "12345678"},				  \
      {"sw_interface_set_flags_reply", "12345678"},                       \
      {"create_loopback", "12345678"},                                    \
      {"create_loopback_reply", "12345678"},                              \
      {"delete_loopback", "12345678"},                                    \
      {"delete_loopback_reply", "12345678"},                              \
      {"vxlan_add_del_tunnel", "12345678"},                               \
      {"vxlan_add_del_tunnel_reply", "12345678"},                         \
      {"sw_interface_set_table", "12345678"},                             \
      {"sw_interface_set_table_reply", "12345678"},                       \
      {"ip_add_del_route", "12345678"},                                   \
      {"ip_add_del_route_reply", "12345678"},
    };

    return 0;
  }

  u16 MockVppConnection::getMsgIndex(string name)
  {
    return getMessageIdByName(name);
  }

  int MockVppConnection::barrierSend(u32 context)
  {
    vl_api_control_ping_reply_t _rmp, *rmp;
    int apiMsgId = 493; //TODO alagalah <- only until work out static. won't work on
    // other installs of VPP.

    rmp = &_rmp;
    memset (rmp, 0, sizeof (*rmp));
    rmp->_vl_msg_id = ntohs (apiMsgId);
    rmp->context = ntohl(context);
    //    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    VppConnection::vppMessageReceiveHandler((char *)rmp, sizeof(*rmp));
    return 0;
  }


} // namespace ovsagent
