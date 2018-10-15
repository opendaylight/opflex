/*
 * StatsSimulator.h
 *
 *  Created on: Sep 25, 2018
 *      Author: msandhu
 */

#ifndef AGENT_OVS_LIB_INCLUDE_OPFLEXAGENT_STATSSIMULATOR_H_
#define AGENT_OVS_LIB_INCLUDE_OPFLEXAGENT_STATSSIMULATOR_H_

#include <opflexagent/Agent.h>
#include <opflexagent/logging.h>
#include <opflexagent/cmd.h>
#include <boost/asio.hpp>

using boost::asio::deadline_timer;
using boost::posix_time::milliseconds;

namespace opflexagent {



class SimStats : public opflexagent::PolicyListener {
    private:
        opflexagent::Agent& agent;
        uint32_t timer_interval;
        std::mutex mutex;
        std::unordered_set<opflex::modb::URI> contracts;
        std::unordered_set<opflex::modb::URI> secGroups;
        std::atomic<uint64_t> intCounter;
        std::atomic<uint64_t> contractCounter;
        std::atomic<uint64_t> contractGenIdCounter;
        std::atomic<uint64_t> secGrpCounter;
        std::atomic<uint64_t> secGrpGenIdCounter;
        boost::asio::io_service io;
        std::atomic_bool stopping;
        std::shared_ptr<std::thread> io_service_thread;
        std::shared_ptr<boost::asio::deadline_timer> timer;
    public:
        SimStats(opflexagent::Agent& agent_, uint32_t timer_interval_);
        ~SimStats();
        void updateInterfaceCounters(opflexagent::Agent& a);
        void updateContractCounters(opflexagent::Agent& a);
        void updateSecGrpCounters(opflexagent::Agent& a);
        void on_timer(const boost::system::error_code& ec);
        void start();
        void stop();
        virtual void contractUpdated(const opflex::modb::URI& uri);
        virtual void secGroupUpdated(const opflex::modb::URI& uri);
};

} /* namespace opflexagent */

#endif /* AGENT_OVS_LIB_INCLUDE_OPFLEXAGENT_STATSSIMULATOR_H_ */
