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

#include "VppConnection.h"

using namespace std;

using boost::asio::ip::address;
using boost::asio::ip::address_v4;
using boost::asio::ip::address_v6;

namespace fs = boost::filesystem;
namespace pt = boost::property_tree;



namespace ovsagent {


  std::recursive_mutex map_mutex;
  std::mutex pneum_mutex;

  struct message_response_t {
    std::promise<vector< char*>> prom;
    vector< char*> msg;

    message_response_t () = default;

    message_response_t (const message_response_t&) = delete;

    message_response_t (message_response_t&& source)
      : prom(std::move(source.prom))
      , msg(std::move(source.msg))
    {  }

  };

  typedef map< u16, message_response_t> response_t; // u16 is _vl_msg_id of reply
  typedef map< u32, response_t> message_map_t; // u32 is context
  message_map_t messageMap;
  message_map_t streamMap;

  VppConnection::messageIdsBiMap VppConnection::messageIds;
  map< string, string> VppConnection::messageCRCByName; //Name, CRC


  VppConnection::VppConnection() : contextId(7) {
  }

  VppConnection::~VppConnection() {
  }

  int VppConnection::lockedWrite(char* msg, int size)
  {
    std::lock_guard<std::mutex> lock(pneum_mutex);
    return pneum_write(msg, size);
  }

  u16 VppConnection::lockedGetMsgIndex(string name)
  {
    std::lock_guard<std::mutex> lock(pneum_mutex);
    return pneum_get_msg_index((unsigned char *) name.c_str());
  }

  int VppConnection::lockedConnect(string name, pneum_callback_t cb)
  {
    return pneum_connect( (char*) name.c_str(), NULL, cb, 32 /* rx queue length */);
  }

  void VppConnection::lockedDisconnect()
  {
    std::lock_guard<std::mutex> lock(pneum_mutex);
    pneum_disconnect();
  }

  u32 VppConnection::lockedGetClientIndex()
  {
    std::lock_guard<std::mutex> lock(pneum_mutex);
    return getpid();
  }

  u32 VppConnection::getContextId()
  {
    return contextId;
  }

  u32 VppConnection::getContextIdParm()
  {
    return ntohl(getContextId());
  }

  u32 VppConnection::getNextContextId()
  {
    return ++contextId;
  }

  u32 VppConnection::getNextContextIdParm()
  {
    return ntohl(getNextContextId());
  }

  void VppConnection::setName(string n)
  {
    name = n;
  }

  string VppConnection::getName()
  {
    return name;
  }

  u16 VppConnection::getMsgIndex (string name)
  {
    return lockedGetMsgIndex(name);
  }

  int VppConnection::populateMessageIds()
  {
    string vppApis{"/usr/share/vpp/api/"};
    fs::recursive_directory_iterator end;
    set<string> files;
    for (fs::recursive_directory_iterator it(vppApis);
         it != end; ++it) {
      if (fs::is_regular_file(it->status())) {
        string fstr = it->path().string();
        if (boost::algorithm::ends_with(fstr, ".json") &&
            !boost::algorithm::starts_with(fstr, ".")) {
          files.insert(fstr);
        }
      }
    }
    for (const std::string& fstr : files) {
      pt::ptree properties;
      string msg{};
      u16 msgId;

      LOG(DEBUG) << "Reading VPP API definitions from " << fstr;
      pt::read_json(fstr, properties);
      for (pt::ptree::value_type &messages : properties.get_child("messages")) {
        for (pt::ptree::value_type &message : messages.second) {
          if (!message.second.data().empty()) {
            msg = message.second.data();
          }
          for (pt::ptree::value_type &field : message.second) {
            if (field.first == "crc") {
              string msg_crc = msg + "_" + field.second.data().erase(0,2);
              messageCRCByName.insert(make_pair(msg, msg_crc));
              msgId = getMsgIndex(msg_crc);
              messageIds.insert( messageIdsPair( msgId, msg));
            }
          }
        }
      }
    }
    return 0;
  }

  u16 VppConnection::getMessageIdByName(string name)
  {
    messageIdsNameIter nameIter = messageIds.right.find(name);
    if ( nameIter != messageIds.right.end() ) {
      return nameIter->second;
    } else {
      return ~0;
    };
  }

  string VppConnection::getMessageNameById(u16 id)
  {
    messageIdsIdIter idIter = messageIds.left.find(id);
    if ( idIter != messageIds.left.end() ) {
      return idIter->second;
    } else {
      return {""};
    };
  }

  string VppConnection::getMessageWithCRCByName(string)
  {
    return {""};
  }

  int VppConnection::connect (string n)
  {
    name = n;
    int rv = -1 ;
    if ( connected )
      return 0;

    pneum_callback_t cb = vppMessageReceiveHandler;

    if ( 0 != name.length() ) {
      rv = lockedConnect(name, cb);
      if ( 0 == rv ) {
        clientIndex = lockedGetClientIndex();
        connected = true;
        if ( populateMessageIds() < 0) {
          LOG(ERROR) << "Couldn't populate message IDs from VPP";
          return rv;
        }
      }
    };
    return rv;
  }

  void VppConnection::enableStream(u32 context, u16 streamId)
  {
    if (!connected) {
      LOG(ERROR) << "not connected";
      return;
    }

    map_mutex.lock();
    message_response_t message_response;
    response_t response;

    response.insert( move(make_pair( streamId, move (message_response))));
    streamMap.insert( make_pair (context, move(response)));
    map_mutex.unlock();
  }

  int VppConnection::send(char* msg, int size, u16 replyId)
  {
    if (!connected) {
      LOG(ERROR) << "not connected";
      return -1;
    }

    map_mutex.lock();

    message_response_t message_response;
    response_t response;
    vl_api_control_ping_t _mp, *mp;
    mp = (vl_api_control_ping_t *)msg;
    u32 context = ntohl(mp->context);
    u16 msgId = ntohs(mp->_vl_msg_id);

    response.insert( move(make_pair( replyId, move (message_response))));
    messageMap.insert( make_pair (context, move(response)));

    map_mutex.unlock();

    int rv = lockedWrite(msg, size);
    if ( rv < 0 ) {
      LOG(ERROR) << "Unable to write to VPP. Error: " << rv;
      return rv;
    }
    rv = barrierSend(context);
    if ( rv < 0 ) {
      LOG(ERROR) << "Unable to write to VPP. Error: " << rv;
      return rv;
    }
    return rv;
  }

  int VppConnection::barrierSend(u32 context)
  {
    vl_api_control_ping_t _mp, *mp;
    int apiMsgId = getMessageIdByName({"control_ping"});
    int rv;

    mp = &_mp;
    memset (mp, 0, sizeof (*mp));
    mp->_vl_msg_id = ntohs (apiMsgId);
    mp->client_index = clientIndex;
    mp->context = ntohl(context);
    return lockedWrite ( (char *)mp, sizeof(*mp));
  }

  static void VppConnection::vppMessageReceiveHandler(char* data, int len)
  {

    vl_api_control_ping_reply_t *rmp;
    int controlPingReplyID = 493; //TODO alagalah <- only until work out static. won't work on
    // other installs of VPP.

    rmp = (vl_api_control_ping_reply_t *)data;
    u16 msgId = ntohs (rmp->_vl_msg_id);
    u32 context = ntohl (rmp->context);

    vector< char*> msg;

    // Check we have map entry for this context - if not discard
    map_mutex.lock();

    // Check we have map entry for this context - if not discard
    auto iter = messageMap.find(context);
    if (iter == messageMap.end()) {
      map_mutex.unlock();
      return;
    }

    /*
     * If this is a control_ping_reply...
     * .. set promise value because we are done
     * else
     * .. it is a reply, append to map.
     */
    if (htons(rmp->_vl_msg_id) == controlPingReplyID) {
      msg = std::move(iter->second.begin()->second.msg);
      iter->second.begin()->second.prom.set_value(std::move(msg));
    } else {
      char *buffer = new char[len];
      memcpy(buffer, data, len);
      iter->second.begin()->second.msg.push_back(std::move(buffer));
    }
    map_mutex.unlock();
  }

  void VppConnection::messageReceiveHandler(char* data, int len)
  {
    vppMessageReceiveHandler(data, len);
  }



  std::vector<char*> VppConnection::readStream(u32 context, u16 streamId)
  {

    std::vector<char*> msgs;

    map_mutex.lock();

    auto iter = streamMap.find(context);
    auto fut = iter->second.begin()->second.prom.get_future();
    if (fut.valid() == 1) {
      fut.wait();
      msgs = fut.get();
      iter->second.find(streamId)->second.prom = std::move(std::promise<vector< char*>>());
    }
    map_mutex.unlock();
    return msgs;
  }

  std::vector<char*> VppConnection::messagesReceived(u32 context)
  {
    auto iter = messageMap.find(context);
    auto fut = iter->second.begin()->second.prom.get_future();

    fut.wait();
    auto msgs = fut.get();
    return msgs;
  }

  void VppConnection::disconnect()
  {
    lockedDisconnect();
    connected = false;
  }

  bool VppConnection::isConnected()
  {
    return connected;
  }



} // namespace ovsagent
