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


  std::mutex map_mutex;
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

  typedef u16 replyId;
  typedef map< replyId, message_response_t> response_t; // u16 is _vl_msg_id of reply
  typedef u32 contextId;
  typedef map< contextId, response_t> message_map_t;
  message_map_t messageMap;
  message_map_t streamMap;

  /*
   * State maintenance
   */
  // map< string, u32> VppApi::bridgeIdByName;
  // VppApi::portMap_t VppApi::intfIndexByName;
  // map< string, string> VppApi::bridgeNameByIntf;
  // map< u32, VppAcl> VppApi::aclsByIndex;
  // u32 VppApi::contextId = 42;
  VppConnection::messageIdsBiMap VppConnection::messageIds;
  map< string, string> VppConnection::messageCRCByName; //Name, CRC


  int VppConnection::lockedWrite(char* msg, int size)
  {
    std::lock_guard<std::mutex> lock(pneum_mutex);
    return pneum_write(msg, size);
  }

  int VppConnection::lockedRead(char** reply, int* reply_len, int timeout )
  {
    std::lock_guard<std::mutex> lock(pneum_mutex);
    return pneum_read (reply, reply_len, timeout);
  }

  int VppConnection::lockedGetMsgTableSize()
  {
    std::lock_guard<std::mutex> lock(pneum_mutex);
    return pneum_msg_table_size();
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
    return VppConnection::lockedGetMsgIndex(name);
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

      LOG(INFO) << "Reading VPP API definitions from " << fstr;
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

  int VppConnection::getMsgTableSize()
  {
    return VppConnection::lockedGetMsgTableSize();
  }

  u16 VppConnection::getMessageIdByName(string name)
  {
    VppConnection::messageIdsNameIter nameIter = messageIds.right.find(name);
    if ( nameIter != messageIds.right.end() ) {
      return nameIter->second;
    } else {
      return ~0;
    };
  }

  string VppConnection::getMessageNameById(u16 id)
  {
    VppConnection::messageIdsIdIter idIter = messageIds.left.find(id);
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

    pneum_callback_t cb = messageReceiveHandler;

    if ( 0 != name.length() ) {
      rv = VppConnection::lockedConnect(name, cb);
      if ( 0 == rv ) {
        clientIndex = VppConnection::lockedGetClientIndex();
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
    std::lock_guard<std::mutex> lock(map_mutex);
    LOG(DEBUG) << "mutex";

    if (!connected) {
      LOG(ERROR) << "not connected";
      return;
    }

    message_response_t message_response;
    response_t response;
    LOG(DEBUG) << " enabling context: " << context << " streamMessage ID: " << streamId;

    response.insert( move(make_pair( streamId, move (message_response))));
    streamMap.insert( make_pair (context, move(response)));
  }

  int VppConnection::send(char* msg, int size, u16 replyId)
  {
    std::lock_guard<std::mutex> lock(map_mutex);
    LOG(DEBUG) << "mutex";

    if (!connected) {
      LOG(ERROR) << "not connected";
      return -1;
    }

    message_response_t message_response;
    response_t response;
    vl_api_control_ping_t _mp, *mp;
    mp = (vl_api_control_ping_t *)msg;
    u32 context = ntohl(mp->context);
    u16 msgId = ntohs(mp->_vl_msg_id);
    LOG(DEBUG) << "Context: " << context << " Message ID: " << msgId \
              << " ReplyId: " << replyId;

    response.insert( move(make_pair( replyId, move (message_response))));
    messageMap.insert( make_pair (context, move(response)));
    int rv = VppConnection::lockedWrite(msg, size);
    if ( rv < 0 ) {
      LOG(ERROR) << "Unable to write to VPP. Error: " << rv;
      return rv;
    }
    rv = barrierSend(context);
    if ( rv < 0 ) {
      LOG(ERROR) << "Unable to write to VPP. Error: " << rv;
      return rv;
    }
    //Send barrier with contextID.
    /*
      TODO alagalah: need to split contextID into u16 replyID u16 transactionID. This means
     map (messageMap) will have u16 transaction -> (1..N) u16 replyIDs with response pair.
     This means that replyID initially will not be used, as the context should only point to one
     thing as control_ping_reply msgID won't match that in response_t key:replyID
    */

    /*TODO alagalah - bit of a mouthful. Need to move the global to a class with utilities,
      potentially SwitchConnection makes sense with send()/receiveHandler() in there
      although that would weld some useful generic VppApi functionality to agent-vpp
      SwitchConnection could always implement that class though ?
    */
    return rv;
  }

  int VppConnection::barrierSend()
  {
    return barrierSend(getContextId());
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
    LOG(DEBUG) << "Sending Barrier with context: " << context;
    return VppConnection::lockedWrite ( (char *)mp, sizeof(*mp));
  }

  static void VppConnection::messageReceiveHandler(char* data, int len)
  {
    std::lock_guard<std::mutex> lock(map_mutex);
    LOG(DEBUG) << "mutex";

    vl_api_control_ping_reply_t *rmp;
    // Can't call this function without an object (see static)
    //int controlPingReplyID = getMessageIdByName({"control_ping_reply_aa016e7b"});
    int controlPingReplyID = 493; //TODO alagalah <- only until work out static. won't work on
    // other installs of VPP.

    rmp = (vl_api_control_ping_reply_t *)data;
    u16 msgId = ntohs (rmp->_vl_msg_id);
    u32 context = ntohl (rmp->context);

    vector< char*> msg;

    // Check we have map entry for this context - if not discard
    auto strIter = streamMap.find(0); //TODO alagalah Setting Context == 0 for streaming until VPP API sorted
    if ((strIter != streamMap.end()) &&
        (strIter->second.find(msgId) != strIter->second.end())) {
      /* Check strIter->second.begin()->second.prom.get_future().valid()
       * IF 1
      *    this is unread future shared state
      *    create new promise()
      *    push back *data
      *    promise.set_value()
      * ELSE
      *    this is an invalid future
      *    create new promise()
      *    create new msg
      *    push back *data
      *    promise.set_value()
      */
      auto fut = std::move(strIter->second.find(msgId)->second.prom.get_future());
      if (fut.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        LOG(DEBUG) << "... valid future for msgId: " << msgId << ". Getting value to msg";
        msg = fut.get();
      } else {
        LOG(DEBUG) << " no data to reset for msgId:" << msgId;
      }
      // else msg is empty...

      strIter->second.find(msgId)->second.prom = std::move(std::promise<vector< char*>>());
      msg.push_back(data);
      strIter->second.find(msgId)->second.prom.set_value(std::move(msg));
      // Done with this stream
      return;
    }



    // Check we have map entry for this context - if not discard
    auto iter = messageMap.find(context);
    if (iter == messageMap.end()) {
      //      LOG(DEBUG) << "Could not find messageMap entry for context: " << context;
      return;
    }

    /*
     * If this is a control_ping_reply...
     * .. set promise value because we are done
     * else
     * .. it is a reply, append to map.
     */
    if (htons(rmp->_vl_msg_id) == controlPingReplyID) {
      // TODO alagalah - once we split u32 context into u16 replyID and u16 transaction
      // we can do a find.. .for now, temporarily using begin()
      //      auto msg = std::move(iter->second.find(msgId)->second.msg);
      // iter->second.find(msgId)->second.prom.set_value(std::move(msg));
      msg = std::move(iter->second.begin()->second.msg);
      iter->second.begin()->second.prom.set_value(std::move(msg));
      //TODO alagalah do we need to flush message? ie
      // does set_value make a copy anyway?
    } else {
      iter->second.begin()->second.msg.push_back(data);
    }

  }

  std::pair<bool, char*> VppConnection::barrierReceived()
  {
    vl_api_control_ping_reply_t *rmp;
    int apiMsgId = getMessageIdByName({"control_ping_reply"});
    char * _reply;
    int reply_len;
    int rv;
    bool isControlPingReply = false;

    rv = VppConnection::lockedRead (&_reply, &reply_len, 0);

    if (rv) {
      LOG(ERROR) << "Bad things from barrierReceived";
      return std::make_pair(false, (char *)NULL);
    }

    rmp = (vl_api_control_ping_reply_t *)_reply;
    LOG(DEBUG) << "Received messageId: " << ntohs(rmp->_vl_msg_id) << " context: " << ntohl(rmp->context);
    rv = ntohl(rmp->retval);
    isControlPingReply = htons(rmp->_vl_msg_id) == apiMsgId;

    return std::make_pair (isControlPingReply, _reply);

  }

  std::vector<char*> VppConnection::readStream(u32 context, u16 streamId)
  {
    std::lock_guard<std::mutex> lock(map_mutex);
    LOG(DEBUG) << "mutex";

    std::vector<char*> msgs;
    auto iter = streamMap.find(context);
    auto fut = iter->second.begin()->second.prom.get_future();
    // fut.wait();
    // return fut.get();
    // alagalah - some quick testing
    LOG(INFO) << " future valid = " << fut.valid() << " trying wait/get";
    if (fut.valid() == 1) {
      fut.wait();
      msgs = fut.get();
      iter->second.find(streamId)->second.prom = std::move(std::promise<vector< char*>>());

    }
    return msgs;
  }

  std::vector<char*> VppConnection::messagesReceived(u32 context)
  {

    auto iter = messageMap.find(context);
    auto fut = iter->second.begin()->second.prom.get_future();
    // fut.wait();
    // return fut.get();
    // alagalah - some quick testing
    LOG(INFO) << " future valid = " << fut.valid() << " trying wait/get";

    fut.wait();
    auto msgs = fut.get();
    if (fut.valid() == 0)
      LOG(INFO) << " future valid = " << fut.valid() << " after wait/get";
    return msgs;
  }

  std::vector<char*> VppConnection::messagesReceived()
  {
    bool barrier = false;
    int rv;
    std::vector<char*> reply;
    rv = barrierSend();

    if ( rv )
      LOG(ERROR) << "something bad in messageReceived() but ... shrug";

    while (!barrier) {
      auto rvp = barrierReceived();
      barrier = rvp.first;
      if (!barrier)
        reply.push_back(rvp.second);
    }

    return reply;
  }

  void VppConnection::disconnect()
  {
    VppConnection::lockedDisconnect();
    connected = false;
  }

  bool VppConnection::isConnected()
  {
    return connected;
  }



} // namespace ovsagent
