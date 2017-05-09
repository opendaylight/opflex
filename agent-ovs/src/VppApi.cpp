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


#include <utility>
#include <functional>
#include "logging.h"

//Debugging
#include <unistd.h>
// End debugging includes

#include "VppApi.h"
#include "VppConnection.h"

using namespace std;

using boost::asio::ip::address;
using boost::asio::ip::address_v4;
using boost::asio::ip::address_v6;

namespace fs = boost::filesystem;
namespace pt = boost::property_tree;

namespace ovsagent {


  /*
   * VPP uses port tags to implement policy (EPGs, SecGroups, direction)
   * Currently re-using convention from networking-vpp ML2 driver
   */
  const int VppApi::VPP_TO_VM = 1;
  const int VppApi::VM_TO_VPP = 0;
  const std::string VppApi::VPP_TO_VM_MARK("from-vpp");
  const std::string VppApi::VM_TO_VPP_MARK("to-vpp");
  const std::string VppApi::SECGROUP_TAG("secgroup");
  const std::string VppApi::EPG_TAG("epg");
  const std::string VppApi::TAG_UPLINK_PREFIX("uplink:");
  const std::string VppApi::TAG_L2IFACE_PREFIX("port:");

  void VppApi::setName(string n)
  {
    this->name = n;
  }

  string VppApi::getName()
  {
    return name;
  }

  // Establish connection if not already done so
  int VppApi::connect(string n)
  {
    name = n;
    auto rv = vppConn.connect(name);
    if (rv) {
      LOG(ERROR) << "Error connecting to VPP: " << rv;
      return rv;
    }
    //TODO supported versions should be defined in conf file
    std::vector<string> supported{"17.04"};
    if (!checkVersion(supported)) {
      LOG(ERROR) << "Unsupported version of VPP. Disconnecting.";
      disconnect();
      return rv;
    }

  }


  auto VppApi::disconnect() -> void
  {
    vppConn.lockedDisconnect();
  }

  auto VppApi::isConnected() -> bool
  {
    return vppConn.isConnected();
  }

  VppConnection VppApi::getVppConnection()
  {
    return vppConn;
  }

  /*
   *  TODO: Decide on sub-class structure. I don't see need for Abstract bollocks...
   *  ... its like peeing in a brown suit... feels good but nobody really notices...
   * ... maybe thats how 10x programmers pump out 8x of their code?
   */
  auto VppApi::getVersion() -> VppVersion
  {
    /*
     * TODO: This whole guitar lick of typedef'ing, sizeof, cast etc
     * can be better handled via Templates template<class S_MSG, class R_MSG> etc
     * ... but early optimisation, root of all evil etc.
     */
    vl_api_show_version_t _mp, *mp;
    vl_api_show_version_reply_t *rmp;
    int apiMsgId = vppConn.getMessageIdByName({"show_version"});
    int apiReplyMsgId = vppConn.getMessageIdByName({"show_version_reply"});
    int msgSize = sizeof(*mp);
    u32 context = vppConn.getNextContextId();
    char *reply;
    int reply_len;
    int rv;

    mp = &_mp;

    memset (mp, 0, sizeof (*mp));
    mp->_vl_msg_id = ntohs (apiMsgId);
    mp->client_index = clientIndex;
    mp->context = ntohl(context);


    rv = vppConn.send ((char*)mp, msgSize, apiReplyMsgId);

    if ( rv < 0 ) {
      LOG(ERROR) << "VPP had an error";
      return version; //TODO alagalah - just horrible.
    }

    std::vector<char*> msgs = vppConn.messagesReceived(context);

    for (const auto& msg: msgs) {
      rmp = (vl_api_show_version_reply_t *) msg;
      if ( htons(rmp->_vl_msg_id) == apiReplyMsgId) {
        rv = ntohl (rmp->retval);
      }
    }

    if ( rv == 0 ) {
      version.program.assign((char *)rmp->program);
      version.version.assign((char *)rmp->version);
      version.buildDate.assign((char *)rmp->build_date);
      version.buildDirectory.assign((char *)rmp->build_directory);
    } else {
      LOG(ERROR) << "VPP had an error";
    }

    return version;
  }


  auto VppApi::printVersion(VppVersion version) -> void
  {
    LOG(INFO) << "Version: " << version.version.c_str();
    LOG(INFO) << "Program: " << version.program;
    LOG(INFO) << "Build Date: " << version.buildDate;
    LOG(INFO) << "Build Dir: " << version.buildDirectory;
  }

  auto VppApi :: checkVersion(std::vector<string> versions) -> bool
  {
    VppVersion versionInfo = getVersion();

    for (auto const v: versions) {
      if (boost::starts_with(versionInfo.version, v))
          return true;
    }
    return false;
  }

  auto VppApi::printIpAddr (ip46_address ip_addr) -> void
  {
    if (ip_addr.is_ipv6) {
      for (int i = 0; i < 8; i++) {
          if (i > 0)
            cout << ':';

          if (ip_addr.ip6.as_u16[i] == 0 && i < 8 && ip_addr.ip6.as_u16[i + 1] == 0) {
            while (ip_addr.ip6.as_u16[i + 1] == 0)
              ++i;
            continue;
          }
          cout << setfill('0') << setw(4) << hex << ip_addr.ip6.as_u16[i];
      }
      cout << dec;
    } else {
      cout << unsigned(ip_addr.ip4.as_u8[0]) << "." ;
      cout << unsigned(ip_addr.ip4.as_u8[1]) << "." ;
      cout << unsigned(ip_addr.ip4.as_u8[2]) << "." ;
      cout << unsigned(ip_addr.ip4.as_u8[3]) ;
    }
  }

  /*
   * VppApi state management functions for maps
   */


  auto VppApi::getIntfIndexByName (string name) -> std::pair<bool, u32>
  {
    map<string, u32>::iterator intfIterator;
    std::pair<bool, u32> rv = make_pair(false, ~0);

    if (name.empty()) {
      LOG(ERROR) << "Interface name empty.";
      return rv;
    }

    intfIterator = intfIndexByName.find(name);

    if (intfIterator == intfIndexByName.end()) {
      LOG(INFO) << "Interface name not found.";
      return rv;
    }

    rv.first = true;
    rv.second = intfIterator->second;
    return rv;
  }

  auto VppApi::setIntfIndexByName (string name, int sw_if_index) -> bool
  {
    if (getIntfIndexByName(name).first == false) {
      intfIndexByName.insert(make_pair(name,sw_if_index));
      return true;
    }
    return false;
  }

  auto VppApi::deleteIntfIndexByName (string name) -> bool
  {
    if (getIntfIndexByName(name).first == true) {
      intfIndexByName.erase(name);
      return true;
    }
    return false;
  }


  auto VppApi::getBridgeIdByName (string name) -> std::pair<bool, u32>
  {
    map<string, u32>::iterator bridgeIterator;
    std::pair<bool, u32> rv = make_pair(false, ~0);

    if (name.empty()) {
      LOG(ERROR) << "Bridge name empty";
      return rv;
    }

    bridgeIterator = bridgeIdByName.find(name);

    if (bridgeIterator == bridgeIdByName.end()) {
      LOG(INFO) << "Bridge name not found";
      return rv;
    }

    rv.first = true;
    rv.second = bridgeIterator->second;
    return rv;
  }

  auto VppApi::setBridgeIdByName (string name, int id) -> bool
  {
    if (getBridgeIdByName(name).first == false) {
      bridgeIdByName.insert( make_pair( name, id));
      return true;
    }
    return false;
  }

  auto VppApi::deleteBridgeIdByName (string name) -> bool
  {
    if (getBridgeIdByName(name).first == true) {
      bridgeIdByName.erase(name);
      return true;
    }
    return false;
  }

  auto VppApi::getBridgeNameByIntf (string intf) -> std::pair<bool, string>
  {
    map<string, string>::iterator bridgeIterator;
    std::pair<bool, string> rv = make_pair(false, std::string());

    if (intf.empty()) {
        LOG(ERROR) << "No interface name passed";
        return rv;
      }

    bridgeIterator = bridgeNameByIntf.find(intf);

    if (bridgeIterator == bridgeNameByIntf.end()) {
      LOG(INFO) << "Interface name not associated with bridge.";
      return rv;
    }

    rv.first = true;
    rv.second = bridgeIterator->second;
    return rv;
  }

  auto VppApi::setBridgeNameByIntf (string name, string intf) -> bool
  {
    if (getBridgeNameByIntf(name).first == false) {
      bridgeNameByIntf.insert( make_pair( name, intf));
      return true;
    }
    return false;
  }

  auto VppApi::deleteBridgeNameByIntf (string name) -> bool
  {
    if (getBridgeNameByIntf(name).first == true) {
      bridgeNameByIntf.erase(name);
      return true;
    }
    return false;
  }

  auto VppApi::getAclByIndex(u32 index) -> std::pair<bool, VppAcl>
  {
    std::pair<bool, VppAcl> rv = make_pair(false, VppAcl{});

    if (index == ~0) {
      LOG(ERROR) << "index = ~0, not valid index, used to indicate create";
      rv.first = false;
      return rv;
    }

    auto iter = aclsByIndex.find(index);

    if (iter == aclsByIndex.end()) {
      rv.first = false;
      return rv;
    }

    rv.first = true;
    rv.second = iter->second;
    return rv;
  }

  auto VppApi::setAclByIndex(VppAcl acl) -> bool
  {
    return true;
  }

  ip46_address
  VppApi :: boostToVppIp46 (address addr) {
    ip46_address vppAddr;

    if (addr.is_v4() ) {
      vppAddr.is_ipv6 = 0;
      vppAddr.ip4 = boostToVppIp4(addr.to_v4());
    } else {
      vppAddr.is_ipv6 = 1;
      vppAddr.ip6 = boostToVppIp6(addr.to_v6());
    }

    return vppAddr;
  }

  ip4_address
  VppApi :: boostToVppIp4 (address_v4 addr) {
    ip4_address ip4;

    array<uint8_t, 4> as_u8 = addr.to_bytes();

    ip4.as_u8[0] = as_u8[0];
    ip4.as_u8[1] = as_u8[1];
    ip4.as_u8[2] = as_u8[2];
    ip4.as_u8[3] = as_u8[3];

    return ip4;
  }

  //TODO
  ip6_address
  VppApi :: boostToVppIp6 (address_v6 addr) {
    ip6_address ip6;

    return ip6;
  }

} // namespace ovsagent
