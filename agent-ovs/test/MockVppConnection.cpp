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


  std::mutex mock_map_mutex;
  std::mutex mock_pneum_mutex;

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
  message_map_t mock_messageMap;
  message_map_t mock_streamMap;

  MockVppConnection::MockVppConnection() : contextId(13) {
  }

  MockVppConnection::~MockVppConnection() {
  }

  MockVppConnection::messageIdsBiMap MockVppConnection::messageIds =
    boost::assign::list_of <MockVppConnection::messageIdsBiMap::relation>
    ((u16)1, "show_version")
    ((u16)2, "show_version_reply");

  map< string, string> MockVppConnection::messageCRCByName = {          \
    {"show_version", "12345678"},                                       \
    {"show_version_reply", "12345678"}                                  \
  };


  int MockVppConnection::lockedWrite(char* msg, int size)
  {
    vl_api_control_ping_t *rmp;
    // Can't call this function without an object (see static)
    //int controlPingReplyID = getMessageIdByName({"control_ping_reply_aa016e7b"});
    int controlPingReplyID = 493; //TODO alagalah <- only until work out static. won't work on
    // other installs of VPP.

    rmp = (vl_api_control_ping_t *)msg;
    u16 msgId = ntohs (rmp->_vl_msg_id);
    u32 context = htonl (rmp->context);

    if (msgId == getMessageIdByName("show_version")) {
      vl_api_show_version_reply_t _mp, *mp;
      mp = &_mp;
      memset (mp, 0, sizeof (*mp));
      mp->_vl_msg_id = ntohs (msgId);
      mp->context = ntohl(context);
      mp->retval = 0;
      strcpy(mp->version, "17.04");
      vppMessageReceiveHandler((char *)mp, sizeof(*mp));
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

  u32 MockVppConnection::lockedGetClientIndex()
  {
    std::lock_guard<std::mutex> lock(mock_pneum_mutex);
    return getpid();
  }

  u32 MockVppConnection::getContextId()
  {
    return contextId;
  }

  u32 MockVppConnection::getContextIdParm()
  {
    return ntohl(getContextId());
  }

  u32 MockVppConnection::getNextContextId()
  {
    return ++contextId;
  }

  u32 MockVppConnection::getNextContextIdParm()
  {
    return ntohl(getNextContextId());
  }

  void MockVppConnection::setName(string n)
  {
    name = n;
  }

  string MockVppConnection::getName()
  {
    return name;
  }


  int MockVppConnection::populateMessageIds()
  {
    return 0;
  }

  u16 MockVppConnection::getMessageIdByName(string name)
  {
    MockVppConnection::messageIdsNameIter nameIter = messageIds.right.find(name);
    if ( nameIter != messageIds.right.end() ) {
      return nameIter->second;
    } else {
      return ~0;
    };
  }

  u16 MockVppConnection::getMsgIndex(string name)
  {
    return getMessageIdByName(name);
  }

  string MockVppConnection::getMessageNameById(u16 id)
  {
    MockVppConnection::messageIdsIdIter idIter = messageIds.left.find(id);
    if ( idIter != messageIds.left.end() ) {
      return idIter->second;
    } else {
      return {""};
    };
  }

  string MockVppConnection::getMessageWithCRCByName(string)
  {
    return {""};
  }

  int MockVppConnection::connect (string n)
  {
    name = n;
    int rv = -1 ;
    if ( connected )
      return 0;

    pneum_callback_t cb = vppMessageReceiveHandler;

    if ( 0 != name.length() ) {
      rv = MockVppConnection::lockedConnect(name, cb);
      if ( 0 == rv ) {
        clientIndex = MockVppConnection::lockedGetClientIndex();
        connected = true;
        if ( MockVppConnection::populateMessageIds() < 0) {
          LOG(ERROR) << "Couldn't populate message IDs from VPP";
          return rv;
        }
      }
    };
    return rv;
  }

  void MockVppConnection::enableStream(u32 context, u16 streamId)
  {
  }

  int MockVppConnection::send(char* msg, int size, u16 replyId)
  {
    std::lock_guard<std::mutex> lock(mock_map_mutex);
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

    response.insert( move(make_pair( replyId, move (message_response))));
    mock_messageMap.insert( make_pair (context, move(response)));
    int rv = MockVppConnection::lockedWrite(msg, size);
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

  int MockVppConnection::barrierSend(u32 context)
  {
    vl_api_control_ping_reply_t _rmp, *rmp;
    int apiMsgId = 493; //TODO alagalah <- only until work out static. won't work on
    // other installs of VPP.

    rmp = &_rmp;
    memset (rmp, 0, sizeof (*rmp));
    rmp->_vl_msg_id = ntohs (apiMsgId);
    rmp->context = ntohl(context);
    vppMessageReceiveHandler((char *)rmp, sizeof(*rmp));
    return 0;
  }

  static void MockVppConnection::vppMessageReceiveHandler(char* data, int len)
  {
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
    auto strIter = mock_streamMap.find(0); //TODO alagalah Setting Context == 0 for streaming until VPP API sorted
    if ((strIter != mock_streamMap.end()) &&
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
    auto iter = mock_messageMap.find(context);
    if (iter == mock_messageMap.end()) {
      //      LOG(DEBUG) << "Could not find mock_messageMap entry for context: " << context;
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

  void MockVppConnection::messageReceiveHandler(char* data, int len)
  {
    vppMessageReceiveHandler(data, len);
  }

  std::vector<char*> MockVppConnection::readStream(u32 context, u16 streamId)
  {
    std::lock_guard<std::mutex> lock(mock_map_mutex);
    LOG(DEBUG) << "mutex";

    std::vector<char*> msgs;
    auto iter = mock_streamMap.find(context);
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

  std::vector<char*> MockVppConnection::messagesReceived(u32 context)
  {

    auto iter = mock_messageMap.find(context);
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

  void MockVppConnection::disconnect()
  {
    MockVppConnection::lockedDisconnect();
    connected = false;
  }

  bool MockVppConnection::isConnected()
  {
    return connected;
  }



} // namespace ovsagent
