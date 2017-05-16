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

using namespace std;

using boost::asio::ip::address;
using boost::asio::ip::address_v4;
using boost::asio::ip::address_v6;

namespace fs = boost::filesystem;
namespace pt = boost::property_tree;


// VPP client C code
extern "C" {
  #include "pneum.h"
};

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
  map< string, u32> VppApi::bridgeIdByName;
  VppApi::portMap_t VppApi::intfIndexByName;
  map< string, string> VppApi::bridgeNameByIntf;
  map< u32, VppAcl> VppApi::aclsByIndex;
  u32 VppApi::contextId = 42;
  VppApi::messageIdsBiMap VppApi::messageIds;
  map< string, string> VppApi::messageCRCByName; //Name, CRC

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


  auto VppApi::setName(string _name) -> void
  {
    name = _name;
  }

  auto VppApi::getName() -> string
  {
    return name;
  }

  VppApi::VppApi () :
    connected(false),
    name({""})
  {
  }
  VppApi::VppApi (string n) :
    connected(false),
    name(n)
  {}

  auto VppApi::getMsgIndex (string name) -> u16
  {
    return lockedGetMsgIndex(name);
  }

  auto VppApi::populateMessageIds() -> int
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

  auto VppApi::getMsgTableSize () -> int
  {
    return lockedGetMsgTableSize();
  }

  auto VppApi::getMessageIdByName(string name) -> u16
  {
    VppApi::messageIdsNameIter nameIter = messageIds.right.find(name);
    if ( nameIter != messageIds.right.end() ) {
      return nameIter->second;
    } else {
      return ~0;
    };
  }

  auto VppApi::getMessageNameById(u16 id) -> string
  {
    VppApi::messageIdsIdIter idIter = messageIds.left.find(id);
    if ( idIter != messageIds.left.end() ) {
      return idIter->second;
    } else {
      return {""};
    };
  }

  auto VppApi::getMessageWithCRCByName(string) -> string
  {
    return {""};
  }

  auto  VppApi::_connect (bool useCB) -> int
  {
    int rv = -1 ;
    if ( connected )
      return 0;

    pneum_callback_t cb = NULL;

    if (useCB) {
      cb = messageReceiveHandler;
    }

    if ( 0 != name.length() ) {
      rv = lockedConnect(name, cb);
      if ( 0 == rv ) {
        clientIndex = lockedGetClientIndex();
        connected = true;
        if ( populateMessageIds() < 0) {
          LOG(ERROR) << "Couldn't populate message IDs from VPP";
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
    };
    return rv;
  }

  // Establish connection if not already done so
  auto VppApi::connect() -> int
  {
    return _connect(true); //Using Callback handler
  }

  auto VppApi::getContextId() -> u32
  {
    return contextId;
  }

  auto VppApi::getContextIdParm() -> u32
  {
    return ntohl(getContextId());
  }

  auto VppApi::getNextContextId() -> u32
  {
    return ++contextId;
  }

  auto VppApi::getNextContextIdParm() -> u32
  {
    return ntohl(getNextContextId());
  }

  auto VppApi::enableStream(u32 context, u16 streamId) -> void
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

  auto VppApi::send(char* msg, int size, u16 replyId) -> int
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

  auto VppApi::barrierSend () -> int
  {
    return barrierSend(getContextId());
  }

  auto VppApi::barrierSend (u32 context) -> int
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
    return lockedWrite ( (char *)mp, sizeof(*mp));
  }

  static auto VppApi::messageReceiveHandler(char* data, int len) -> void
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

  auto VppApi::barrierReceived() -> std::pair<bool, char*>
  {
    vl_api_control_ping_reply_t *rmp;
    int apiMsgId = getMessageIdByName({"control_ping_reply"});
    char * _reply;
    int reply_len;
    int rv;
    bool isControlPingReply = false;

    rv = lockedRead (&_reply, &reply_len, 0);

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

  auto VppApi::readStream(u32 context, u16 streamId) -> std::vector<char*>
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

  auto VppApi::messagesReceived(u32 context) -> std::vector<char*>
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

  auto VppApi::messagesReceived() -> std::vector<char*>
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

  auto VppApi::disconnect() -> void
  {
    lockedDisconnect();
    connected = false;
  }

  auto VppApi::isConnected() -> bool
  {
    return connected;
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
    int apiMsgId = getMessageIdByName({"show_version"});
    int apiReplyMsgId = getMessageIdByName({"show_version_reply"});
    int msgSize = sizeof(*mp);
    u32 context = getNextContextId();
    char *reply;
    int reply_len;
    int rv;

    mp = &_mp;

    memset (mp, 0, sizeof (*mp));
    mp->_vl_msg_id = ntohs (apiMsgId);
    mp->client_index = clientIndex;
    mp->context = ntohl(context);


    rv = send ((char*)mp, msgSize, apiReplyMsgId);

    if ( rv < 0 ) {
      LOG(ERROR) << "VPP had an error";
      return version; //TODO alagalah - just horrible.
    }

    std::vector<char*> msgs = messagesReceived(context);

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
