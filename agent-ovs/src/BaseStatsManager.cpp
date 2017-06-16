#include "logging.h"
#include "IdGenerator.h"
#include "Agent.h"
#include "IntFlowManager.h"
#include "TableState.h"
#include "ovs-shim.h"
#include "ovs-ofputil.h"
#include <lib/util.h>
#include "BaseStatsManager.h"

extern "C" {
#include <openvswitch/ofp-msgs.h>
}

#include <opflex/modb/URI.h>
#include <opflex/modb/Mutator.h>
#include <modelgbp/gbpe/L24Classifier.hpp>
#include <modelgbp/gbpe/HPPClassifierCounter.hpp>
#include <modelgbp/observer/PolicyStatUniverse.hpp>

namespace ovsagent {

using std::string;
using std::unordered_map;
using boost::optional;
using boost::make_optional;
using std::shared_ptr;
using opflex::modb::URI;
using opflex::modb::Mutator;
using opflex::ofcore::OFFramework;
using namespace modelgbp::gbp;
using namespace modelgbp::observer;
using namespace modelgbp::gbpe;
using boost::asio::deadline_timer;
using boost::asio::placeholders::error;
using boost::posix_time::milliseconds;
using boost::uuids::to_string;
using boost::uuids::basic_random_generator;
using std::bind;
using boost::system::error_code;

static const int MAX_AGE = 3;

BaseStatsManager::BaseStatsManager(Agent* agent_, IdGenerator& idGen_,
                                       SwitchManager& switchManager_,
                                       long timer_interval_)
    : agent(agent_), connection(NULL), idGen(idGen_),
      switchManager(switchManager_),
      agent_io(agent_->getAgentIOService()),
      timer_interval(timer_interval_), stopping(false) {
    
    
}

BaseStatsManager::~BaseStatsManager() {

}

void BaseStatsManager::registerConnection(SwitchConnection* connection) {
    this->connection = connection;
}

void BaseStatsManager::start() {
    stopping = false;

    connection->RegisterMessageHandler(OFPTYPE_FLOW_STATS_REPLY, this);
    connection->RegisterMessageHandler(OFPTYPE_FLOW_REMOVED, this);

    timer.reset(new deadline_timer(agent_io, milliseconds(timer_interval)));
}

void BaseStatsManager::stop() {
    stopping = true;

    if (connection) {
        connection->UnregisterMessageHandler(OFPTYPE_FLOW_STATS_REPLY, this);
        connection->UnregisterMessageHandler(OFPTYPE_FLOW_REMOVED, this);
    }

    if (timer) {
        timer->cancel();
    }
}

void BaseStatsManager::updateFlowEntryMap(uint64_t cookie, uint16_t priority,
                                            uint8_t index,const struct match& match) {
    FlowEntryMatchKey_t flowEntryKey((uint32_t)ovs_ntohll(cookie),
                                     priority, match);

    /* check if ACCStats Manager has it in its oldFlowCounterMap */
    FlowEntryCounterMap_t::iterator it =
        oldFlowCounterMap[index].find(flowEntryKey);
    if (it != oldFlowCounterMap[index].end()) {
        return;
    }

    /* check if Stats Manager has it in its newFlowCounterMap */
    it = newFlowCounterMap[index].find(flowEntryKey);
    if (it != newFlowCounterMap[index].end()) {
        FlowCounters_t&  flowCounters = it->second;
        flowCounters.age += 1;
        return;
    }

    /* Add the flow entry to newmap */
    FlowCounters_t&  flowCounters = newFlowCounterMap[index][flowEntryKey];
    flowCounters.visited = false;
    flowCounters.age = 0;
    flowCounters.last_packet_count = make_optional(false, 0);
    flowCounters.last_byte_count = make_optional(false, 0);
    flowCounters.diff_packet_count = make_optional(false, 0);
    flowCounters.diff_byte_count = make_optional(false, 0);
    LOG(DEBUG) <<"newFlowCounterMap entry added!"
               << "Cookie=" << (uint32_t)ovs_ntohll(cookie)
               << " reg0=" << match.flow.regs[0]<<" index "<<index;
}

void BaseStatsManager::on_timer_base(const error_code& ec,int index,PolicyCounterMap_t& newClassCountersMap) {

    // Walk through all the old map entries that have
    // been visited.
    for (FlowEntryCounterMap_t:: iterator itr = oldFlowCounterMap[index].begin();
         itr != oldFlowCounterMap[index].end();
         itr++) {
        const FlowEntryMatchKey_t& flowEntryKey = itr->first;
        FlowCounters_t&  newFlowCounters = itr->second;
        // Have we visited this flow entry yet
        if (!newFlowCounters.visited) {
            // increase age by polling interval
            newFlowCounters.age += 1;
            if (newFlowCounters.age >= MAX_AGE) {
                LOG(DEBUG) << "Unvisited entry for last " << MAX_AGE
                           << " polling intervals";
            }
            continue;
        }

        // Have we collected non-zero diffs for this flow entry
	      if (newFlowCounters.diff_packet_count && newFlowCounters.diff_packet_count.get() != 0) {

            FlowMatchKey_t flowMatchKey(flowEntryKey.cookie,
                                        flowEntryKey.match.flow.regs[0],
                                        flowEntryKey.match.flow.regs[2]);

            PolicyCounters_t&  newClassCounters =
                newClassCountersMap[flowMatchKey];

            // We get multiple flow stats entries for same
            // cookie, reg0 and reg2 ie for each classifier
            // multiple flow entries may get installed in OVS
            // The newCountersMap is used for the purpose of
            // additing counters of such flows that have same
            // classifier, reg0 and reg2.

            uint64_t packet_count = 0;
            uint64_t byte_count = 0;
            if (newClassCounters.packet_count) {
                // get existing packet_count and byte_count
                packet_count = newClassCounters.packet_count.get();
                byte_count = newClassCounters.byte_count.get();
            }

            // Add counters for new flow entry to existing
            // packet_count and byte_count
            if (newFlowCounters.diff_packet_count) {
                newClassCounters.packet_count =
                  make_optional(true,
                              newFlowCounters.diff_packet_count.get() +
                              packet_count);
                newClassCounters.byte_count =
                  make_optional(true,
                              newFlowCounters.diff_byte_count.get() +
                              byte_count);
            }

            // reset the per flow entry diff counters to zero.
            newFlowCounters.diff_packet_count = make_optional(true, 0);
            newFlowCounters.diff_byte_count = make_optional(true, 0);
            // set the age of this entry as zero as we have seen
            // its counter increment last polling cycle.
            newFlowCounters.age = 0;
        }
        // Set entry visited as false as we have consumed its diff
        // counters.  When we visit this entry when handling a
        // FLOW_STATS_REPLY corresponding to this entry, we mark this
        // entry as visited again.
        newFlowCounters.visited = false;
    }


    // Walk through all the removed flow map entries

    for (FlowEntryCounterMap_t::iterator itt = removedFlowCounterMap[index].begin();
         itt != removedFlowCounterMap[index].end();
         itt++) {
        const FlowEntryMatchKey_t& remFlowEntryKey = itt->first;
        FlowCounters_t&  remFlowCounters = itt->second;

        // Have we collected non-zero diffs for this removed flow entry
        if (remFlowCounters.diff_packet_count) {

            FlowMatchKey_t flowMatchKey(remFlowEntryKey.cookie,
                                        remFlowEntryKey.match.flow.regs[0],remFlowEntryKey.match.flow.regs[2]);

            PolicyCounters_t& newClassCounters =
                newClassCountersMap[flowMatchKey];

            uint64_t packet_count = 0;
            uint64_t byte_count = 0;
            if (newClassCounters.packet_count) {
                // get existing packet_count and byte_count
                packet_count = newClassCounters.packet_count.get();
                byte_count = newClassCounters.byte_count.get();
            }

            // Add counters for flow entry to be removed
            newClassCounters.packet_count =
                make_optional(true,
                              (remFlowCounters.diff_packet_count) ? remFlowCounters.diff_packet_count.get() : 0 +
                              packet_count);
            newClassCounters.byte_count =
                make_optional(true,
                    (remFlowCounters.diff_byte_count) ? remFlowCounters.diff_byte_count.get() : 0 +
                              byte_count);
        }
    }

    // Delete all the entries from removedFlowCountersMap
    // Since we have taken them into account in our calculations for
    // per classifier/epg pair counters.

    removedFlowCounterMap[index].clear();

    // Walk through all the old map entries and remove those entries
    // that have not been visited but age is equal to MAX_AGE times
    // polling interval.

    for (FlowEntryCounterMap_t::iterator itr = oldFlowCounterMap[index].begin();
         itr != oldFlowCounterMap[index].end();) {
        const FlowEntryMatchKey_t& flowEntryKey = itr->first;
        FlowCounters_t&  flowCounters = itr->second;
        // Have we visited this flow entry yet
        if (!flowCounters.visited && (flowCounters.age >= MAX_AGE)) {
            LOG(DEBUG) << "Erasing unused entry from oldFlowCounterMap";
            itr = oldFlowCounterMap[index].erase(itr);
        } else
            itr++;
    }

    // Walk through all the new map entries and remove those entries
    // that have age equal to MAX_AGE times polling interval.
    for (FlowEntryCounterMap_t::iterator itr = newFlowCounterMap[index].begin();
         itr != newFlowCounterMap[index].end();) {
        const FlowEntryMatchKey_t& flowEntryKey = itr->first;
        FlowCounters_t&  flowCounters = itr->second;
        // Have we visited this flow entry yet
        if (flowCounters.age >= MAX_AGE) {
            LOG(DEBUG) << "Erasing unused entry from newFlowCounterMap";
            itr = newFlowCounterMap[index].erase(itr);
        } else
            itr++;
    }

}
 
void BaseStatsManager::updateNewFlowCounters(uint32_t cookie,
                                               uint16_t priority,
                                               struct match& match,
                                               uint64_t flow_packet_count,
                                               uint64_t flow_byte_count,
                                               uint8_t index,
                                               bool flowRemoved) {

    FlowEntryMatchKey_t flowEntryKey(cookie, priority, match);

    // look in existing oldFlowCounterMap
    FlowEntryCounterMap_t::iterator it =
        oldFlowCounterMap[index].find(flowEntryKey);
    uint64_t packet_count = 0;
    uint64_t byte_count = 0;
    if (it != oldFlowCounterMap[index].end()) {
        FlowCounters_t& oldFlowCounters = it->second;
        /* compute diffs and mark the entry as visited */
        packet_count = oldFlowCounters.last_packet_count.get();
        byte_count = oldFlowCounters.last_byte_count.get();
      if ((flow_packet_count - packet_count) > 0) {
            LOG(DEBUG) << "FlowStat: entry found in oldFlowCounterMap"<<index;
            LOG(DEBUG) << "Non zero diff packet count flow entry found";
      }
      if ((flow_packet_count - packet_count) > 0) {
            	oldFlowCounters.diff_packet_count =
                	flow_packet_count - packet_count;
            	oldFlowCounters.diff_byte_count = flow_byte_count - byte_count;
            	oldFlowCounters.last_packet_count = flow_packet_count;
            	oldFlowCounters.last_byte_count = flow_byte_count;
            	oldFlowCounters.visited = true;
      }
      if (flowRemoved) {
            // Move the entry to removedFlowCounterMap
            FlowCounters_t & newFlowCounters =
            removedFlowCounterMap[index][flowEntryKey];
            newFlowCounters.diff_byte_count = oldFlowCounters.diff_byte_count;
            newFlowCounters.diff_packet_count =
                	oldFlowCounters.diff_packet_count;
            LOG(DEBUG) << "flowRemoved: packet count :"
                       << newFlowCounters.diff_packet_count
                       << " byte count: "
                       << newFlowCounters.diff_byte_count;
            // Delete the entry from oldFlowCounterMap
            oldFlowCounterMap[index].erase(flowEntryKey);
        }
    } else {
        // Check if we this entry exists in newFlowCounterMap;
        FlowEntryCounterMap_t::iterator it =
            newFlowCounterMap[index].find(flowEntryKey);
        if (it != newFlowCounterMap[index].end()) {

            // the flow entry probably got removed even before Policy Stats
            // manager could process its FLOW_STATS_REPLY

            if (flowRemoved) {
                FlowCounters_t & remFlowCounters =
                removedFlowCounterMap[index][flowEntryKey];
               	remFlowCounters.diff_byte_count =
                    	make_optional(true, flow_byte_count);
                remFlowCounters.diff_packet_count =
                    	make_optional(true, flow_packet_count);
                // remove the entry from newFlowCounterMap.
                newFlowCounterMap[index].erase(flowEntryKey);
            } else {
                // Store the current counters as last counter value by
                // adding the entry to oldFlowCounterMap and expect
                // next FLOW_STATS_REPLY for it to compute delta and
                // mark the entry as visited. If PolicyStats
                // Manager(OVS agent) got restarted then we can't use
                // the counters reported as is until delta is computed
                // as entry may have existed long before it.

                if (flow_packet_count != 0) {
                    /* store the counters in oldFlowCounterMap for it and
                     * remove from newFlowCounterMap as it is no more new
                     */
                    FlowCounters_t & newFlowCounters =
                    oldFlowCounterMap[index][flowEntryKey];
                    newFlowCounters.last_packet_count =
                            make_optional(true, flow_packet_count);
                    newFlowCounters.last_byte_count =
                            make_optional(true, flow_byte_count);
                    newFlowCounters.visited = false;
                    newFlowCounters.age = 0;
                    // remove the entry from newFlowCounterMap.
                    LOG(DEBUG) << "Non-zero flow_packet_count"
                               << " moving entry from newFlowCounterMap to"
                               << " oldFlowCounterMap";
                    newFlowCounterMap[index].erase(flowEntryKey);
                }
            }
        } else {
            // This is the case when FLOW_REMOVE is received even
            // before Policy Stats Manager knows about it.  This can
            // potentially happen as we learn about flow entries
            // periodically. The side affect is we lose a maximum of
            // polling interval worth of counters for this classifier
            // entry. We choose to ignore this case for now.
            LOG(DEBUG) << "Received flow stats for an unknown flow: "
                       << "Cookie : " << cookie << " Priority :" << priority
                       << " reg 0 " << match.flow.regs[0] << " reg 2 "
                       << match.flow.regs[2];
        }
    }
}

void BaseStatsManager::Handle(SwitchConnection* connection,
                                int msgType, ofpbuf *msg) {

    if (msg == (ofpbuf *)NULL) {
        LOG(ERROR) << "Unexpected null message";
        return;
    }
    if (msgType == OFPTYPE_FLOW_STATS_REPLY) {
        handleFlowStats(msgType, msg);
    } else if (msgType == OFPTYPE_FLOW_REMOVED) {
        handleFlowRemoved(msg);
    } else {
        LOG(ERROR) << "Unexpected message type: " << msgType;
        return;
    }

}

void BaseStatsManager::handleFlowStats(int msgType, ofpbuf *msg) {

    struct ofputil_flow_stats* fentry, fstat;
    PolicyCounterMap_t newCountersMap;
    PolicyManager& polMgr = agent->getPolicyManager();

    fentry = &fstat;
    std::lock_guard<std::mutex> lock(pstatMtx);

    do {
        ofpbuf actsBuf;
        ofpbuf_init(&actsBuf, 64);
        bzero(fentry, sizeof(struct ofputil_flow_stats));

        int ret = ofputil_decode_flow_stats_reply(fentry, msg, false, &actsBuf);

        ofpbuf_uninit(&actsBuf);

        if (ret != 0) {
            if (ret != EOF)
                LOG(ERROR) << "Failed to decode flow stats reply: "
                           << ovs_strerror(ret);
            else
                LOG(ERROR) << "No more flow stats entries to decode "
                           << ovs_strerror(ret);
            break;
        } else {
           bool found = false;
	         uint8_t index = 0;
           for(int i = 0;i < 2;i++) {
              if (fentry->table_id == table_ids[i]) {
                  found = true;
                  index = i;
              }
           }
           if (!found) {
              LOG(ERROR) << "Unexpected table_id = " << fentry->table_id <<
                " in flow remove msg: ";
              return;
            }
            if ((fentry->flags & OFPUTIL_FF_SEND_FLOW_REM) == 0) {
                // skip those flow entries that don't have flag set
                continue;
            }

            // Handle forward entries and drop entries separately
            // Packet forward entries are per classifier
            // Packet drop entries are per routingDomain

            struct match *pflow_metadata;
            pflow_metadata = &(fentry->match);
            uint32_t rdId = 0;
            boost::optional<std::string> idRdStr;
            if (table_ids[0] == IntFlowManager::POL_TABLE_ID) {
                rdId = (uint32_t)pflow_metadata->flow.regs[6];
                if (rdId) {
                    idRdStr = idGen
                      .getStringForId(IntFlowManager::
                                    getIdNamespace(RoutingDomain::CLASS_ID),
                                    rdId);
                    if (idRdStr == boost::none) {
                        LOG(DEBUG) << "rdId: " << rdId <<
                        " to URI translation does not exist";
                        continue;
                     }
                 }
            }
            // Does flow stats entry qualify to be a drop entry?
            // if yes, then process it and continue with next flow
            // stats entry.
            if (rdId && idRdStr && (fentry->priority == 1)) {
                handleDropStats(rdId, idRdStr, fentry);
            } else {

                // Handle flow stats entries for packets that are matched
                // and are forwarded

                updateNewFlowCounters((uint32_t)ovs_ntohll(fentry->cookie),
                                      fentry->priority,
                                      (fentry->match),
                                      fentry->packet_count,
                                      fentry->byte_count,
                                      index,false);
            }
        }
    } while (true);

}

void BaseStatsManager::handleFlowRemoved(ofpbuf *msg) {

    const struct ofp_header *oh = (ofp_header *)msg->data;
    struct ofputil_flow_removed* fentry, flow_removed;
    PolicyManager& polMgr = agent->getPolicyManager();

    fentry = &flow_removed;
    std::lock_guard<std::mutex> lock(pstatMtx);
    int ret;
    bzero(fentry, sizeof(struct ofputil_flow_removed));

    ret = ofputil_decode_flow_removed(fentry, oh);
    if (ret != 0) {
        LOG(ERROR) << "Failed to decode flow removed message: "
                   << ovs_strerror(ret);
        return;
    } else {
      bool found = false;
	    uint8_t index = 0;
      for(int i = 0;i < 2;i++) {
          if (fentry->table_id == table_ids[i]) {
              found = true;
              index = i;
          }
      }
      if (!found) {
            LOG(ERROR) << "Unexpected table_id = " << fentry->table_id <<
                " in flow remove msg: ";
            return;
      }
      updateNewFlowCounters((uint32_t)ovs_ntohll(fentry->cookie),
                              fentry->priority,
                              (fentry->match),
                              fentry->packet_count,
                              fentry->byte_count,
                              index,true);
    }

}

void BaseStatsManager::sendRequest(uint32_t table_id) {
  // send port stats request again
    ofp_version ofVer = (ofp_version)connection->GetProtocolVersion();
    ofputil_protocol proto = ofputil_protocol_from_ofp_version(ofVer);

    ofputil_flow_stats_request fsr;
    bzero(&fsr, sizeof(ofputil_flow_stats_request));
    fsr.aggregate = false;
    match_init_catchall(&fsr.match);
    fsr.table_id = table_id;
    fsr.out_port = OFPP_ANY;
    fsr.out_group = OFPG_ANY;
    fsr.cookie = fsr.cookie_mask = (uint64_t)0;

    ofpbuf *req = ofputil_encode_flow_stats_request(&fsr, proto);
    ofpmsg_update_length(req);

    int err = connection->SendMessage(req);
    if (err != 0) {
        LOG(ERROR) << "Failed to send policy statistics request: "
                   << ovs_strerror(err);
    }
}

void BaseStatsManager::
generatePolicyStatsObjects(PolicyCounterMap_t *newCountersMap1, PolicyCounterMap_t *newCountersMap2) {
    // walk through newCountersMap to create new set of MOs

    PolicyManager& polMgr = agent->getPolicyManager();

    for (PolicyCounterMap_t:: iterator itr = newCountersMap1->begin();
         itr != newCountersMap1->end();
         itr++) {
        const FlowMatchKey_t& flowKey = itr->first;
        PolicyCounters_t&  newCounters1 = itr->second;
        PolicyCounters_t newCounters2;
	      boost::optional<std::string> secGrpsIdStr;
        optional<URI> srcEpgUri;
        optional<URI> dstEpgUri;
        if (newCountersMap2 != NULL) {
        
	        boost::optional<std::string> secGrpsIdStr = idGen.getStringForId(
                "secGroupSet",flowKey.reg0);
	        if (secGrpsIdStr == boost::none) {
            LOG(ERROR) << "Reg0: " << flowKey.reg0 <<
                          " to SecGrpSetId translation does not exist";
            continue;
          }
          if (newCountersMap2->count(flowKey)) {
            newCounters2 = newCountersMap2->find(flowKey)->second ;
          }
        } else {
            srcEpgUri = polMgr.getGroupForVnid(flowKey.reg0);
            dstEpgUri = polMgr.getGroupForVnid(flowKey.reg2);
            if (srcEpgUri == boost::none) {
                LOG(ERROR) << "Reg0: " << flowKey.reg0
                       << " to EPG URI translation does not exist";
                continue;
            }
            if (dstEpgUri == boost::none) {
                LOG(ERROR) << "Reg2: " << flowKey.reg2
                       << " to EPG URI translation does not exist";
                continue;
            }
        }
        boost::optional<std::string> idStr =
            idGen.getStringForId(IntFlowManager::
                                 getIdNamespace(L24Classifier::CLASS_ID),
                                 flowKey.cookie);
        if (idStr == boost::none) {
            LOG(ERROR) << "Cookie: " << flowKey.cookie
                       << " to Classifier URI translation does not exist";
            continue;
        }
        if (newCountersMap2 != NULL) {
          updatePolicyStatsCounters(idStr.get(),
                                      newCounters1,newCounters2);
        } else {
          if (newCounters1.packet_count.get() != 0) {
            updatePolicyStatsCounters(srcEpgUri.get().toString(),
                                      dstEpgUri.get().toString(),
                                      idStr.get(),
                                      newCounters1);
          }

        }
    }
}

size_t BaseStatsManager::KeyHasher::
operator()(const BaseStatsManager::FlowMatchKey_t& k) const noexcept {
    using boost::hash_value;
    using boost::hash_combine;

    std::size_t seed = 0;
    hash_combine(seed, hash_value(k.cookie));
    hash_combine(seed, hash_value(k.reg0));
    hash_combine(seed, hash_value(k.reg2));

    return (seed);
}

size_t BaseStatsManager::FlowKeyHasher::
operator()(const BaseStatsManager::FlowEntryMatchKey_t& k) const noexcept {
    using boost::hash_value;
    using boost::hash_combine;

    std::size_t hashv = match_hash(&k.match, 0);
    hash_combine(hashv, hash_value(k.cookie));
    hash_combine(hashv, hash_value(k.priority));

    return (hashv);
}
} /* namespace ovsagent */

