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

  VppApi::VppApi(unique_ptr<VppConnection> conn) :
    vppConn(move(conn)) {
  }

  VppApi::~VppApi() {
  }

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
    auto rv = getVppConnection()->connect(name);
    if (rv) {
      LOG(ERROR) << "Error connecting to VPP: " << rv;
      return rv;
    }
    //TODO supported versions should be defined in conf file
    std::vector<string> supported{"17.04"};
    if (!checkVersion(supported)) {
      disconnect();
      return rv;
    }

  }

  void VppApi::disconnect()
  {
    getVppConnection()->lockedDisconnect();
  }

  bool VppApi::isConnected()
  {
    return getVppConnection()->isConnected();
  }

  VppConnection* VppApi::getVppConnection()
  {
    return vppConn.get();
  }

  /*
   *  TODO: Decide on sub-class structure. I don't see need for Abstract bollocks...
   *  ... its like peeing in a brown suit... feels good but nobody really notices...
   * ... maybe thats how 10x programmers pump out 8x of their code?
   */
  VppVersion VppApi::getVersion()
  {
    /*
     * TODO: This whole guitar lick of typedef'ing, sizeof, cast etc
     * can be better handled via Templates template<class S_MSG, class R_MSG> etc
     * ... but early optimisation, root of all evil etc.
     */
    vl_api_show_version_t _mp, *mp;
    vl_api_show_version_reply_t *rmp;
    int apiMsgId = getVppConnection()->getMessageIdByName({"show_version"});
    int apiReplyMsgId = getVppConnection()->getMessageIdByName({"show_version_reply"});
    int msgSize = sizeof(*mp);
    u32 context = getVppConnection()->getNextContextId();
    char *reply;
    int reply_len;
    int rv;

    mp = &_mp;

    memset (mp, 0, sizeof (*mp));
    mp->_vl_msg_id = ntohs (apiMsgId);
    mp->client_index = clientIndex;
    mp->context = ntohl(context);


    rv = getVppConnection()->send ((char*)mp, msgSize, apiReplyMsgId);

    if ( rv < 0 ) {
      LOG(ERROR) << "VPP had an error";
      return version; //TODO alagalah - just horrible.
    }

    rv = -1;

    std::vector<char*> msgs = getVppConnection()->messagesReceived(context);

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

  bool VppApi::checkVersion(std::vector<string> versions)
  {
    VppVersion versionInfo = getVersion();

    for (auto const v: versions) {
      if (boost::starts_with(versionInfo.version, v))
          return true;
    }
    return false;
  }


} // namespace ovsagent
