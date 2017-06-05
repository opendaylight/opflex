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

  template <class T, class U, class V>
  T * setResponse (T &_mp, U u, V context) {
    T *mp;
    mp = &_mp;
    memset (mp, 0, sizeof (*mp));
    mp->_vl_msg_id = ntohs (u);
    mp->context = ntohl(context);
    return mp;
  }


  MockVppConnection::MockVppConnection() {
  }

  MockVppConnection::~MockVppConnection() {
  }

  int MockVppConnection::lockedWrite(char* msg, int size)
  {
    vl_api_control_ping_t *rmp;
    // Can't call this function without an object (see static)
    //int controlPingReplyID = getMessageIdByName({"control_ping_reply_aa016e7b"});
    int controlPingReplyID = 493; //TODO alagalah <- only until work out static. won't work on
    // other installs of VPP.

    /*
     * Will extend it using switch case
     */

    rmp = (vl_api_control_ping_t *)msg;
    u16 msgId = ntohs (rmp->_vl_msg_id);
    u32 context = htonl (rmp->context);

    switch (msgId) {
    case 1:
      vl_api_show_version_reply_t _mp, *mp;
      mp = setResponse(_mp, getMessageIdByName("show_version_reply"), context);
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
    case 3:
        vl_api_bridge_domain_add_del_t *rmp;
        vl_api_bridge_domain_add_del_reply_t _mp3, *mp3;
        mp3 = setResponse(_mp3, getMessageIdByName("bridge_domain_add_del_reply"), context);
        rmp = (vl_api_bridge_domain_add_del_t *)msg;
        if (rmp->bd_id > 0) {
            mp3->retval = 0;
        } else {
            mp3->retval = INVALID_VALUE;
        }
        VppConnection::vppMessageReceiveHandler(std::move((char *)mp3), sizeof(*mp3));
        break;
    case 5:
    case 7:
    case 9:
    case 11:
        // API unit test responses
        cout << " Need to construct UT response " << endl;
        break;
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
      ((u16)7, "sw_interface_set_l2_bridge")
      ((u16)8, "sw_interface_set_l2_bridge_reply")
      ((u16)9, "sw_interface_add_del_address")
      ((u16)10, "sw_interface_add_del_address_reply")
      ((u16)11, "sw_interface_set_flags")
      ((u16)12, "sw_interface_set_flags_reply");

    VppConnection::messageCRCByName = {          \
      {"show_version", "12345678"},                                       \
      {"show_version_reply", "12345678"},                                 \
      {"bridge_domain_add_del", "12345678"},				  \
      {"bridge_domain_add_del_reply", "12345678"},			  \
      {"af_packet_create", "12345678"},					  \
      {"af_packet_create_reply", "12345678"},				  \
      {"sw_interface_set_l2_bridge", "12345678"},			  \
      {"sw_interface_set_l2_bridge_reply", "12345678"},			  \
      {"sw_interface_add_del_address", "12345678"},			  \
      {"sw_interface_add_del_address_reply", "12345678"},		  \
      {"sw_interface_set_flags", "12345678"},				  \
      {"sw_interface_set_flags_reply", "12345678"}
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
