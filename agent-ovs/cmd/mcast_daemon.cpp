/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Main implementation for multicast listener daemon
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <opflexagent/MulticastListener.h>
#include <opflexagent/FSWatcher.h>
#include <opflexagent/logging.h>
#include <opflexagent/cmd.h>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include <string>
#include <iostream>
#include <unordered_map>
#include <memory>
#include <thread>
#include <functional>

#include <csignal>
#include <cstring>

using std::string;
using boost::optional;
using std::shared_ptr;
using std::unique_ptr;
using std::unordered_set;
using std::unordered_map;
namespace po = boost::program_options;
namespace pt = boost::property_tree;
using namespace opflexagent;

void sighandler(int sig) {
    LOG(INFO) << "Got " << strsignal(sig) << " signal";
}

class McastWatcher : public FSWatcher::Watcher {
public:
    McastWatcher(boost::asio::io_service& io_,
                 MulticastListener& listener_)
        : io(io_), listener(listener_) { }
    virtual ~McastWatcher() {}

    // FSWatcher::Watcher::updated
    virtual void updated(const boost::filesystem::path& filePath) {
        string fstr = filePath.filename().string();
        if (!boost::algorithm::ends_with(fstr, ".json") ||
            boost::algorithm::starts_with(fstr, "."))
            return;
        readConfig(filePath.string());
    }

    // FSWatcher::Watcher::deleted
    virtual void deleted(const boost::filesystem::path& filePath) {
        addresses.erase(filePath.string());
        sync();
    }

private:
    boost::asio::io_service& io;
    MulticastListener& listener;

    typedef unordered_map<string, unordered_set<string> > file_addr_map_t;
    file_addr_map_t addresses;

    void sync() {
        shared_ptr<unordered_set<string > > addrs(new unordered_set<string >());
        for (const file_addr_map_t::value_type& faddr : addresses) {
            addrs->insert(faddr.second.begin(), faddr.second.end());
        }

        io.dispatch([this, addrs]() { listener.sync(addrs); });
    }

    void readConfig(const std::string& filePath) {
        static const std::string MULTICAST_GROUPS("multicast-groups");

        unordered_set<string>& fileAddrs = addresses[filePath];
        fileAddrs.clear();

        try {
            pt::ptree properties;
            pt::read_json(filePath, properties);
            optional<pt::ptree&> groups =
                properties.get_child_optional(MULTICAST_GROUPS);
            if (groups) {
                for (const pt::ptree::value_type &v : groups.get())
                    fileAddrs.insert(v.second.data());
            }
        } catch (pt::json_parser_error& e) {
            LOG(ERROR) << "Could not parse multicast group file: " << e.what();
        }

        sync();
    }

};

#define DEFAULT_WATCH LOCALSTATEDIR"/lib/opflex-agent-ovs/mcast/"

int main(int argc, char** argv) {
    signal(SIGPIPE, SIG_IGN);

    // Parse command line options
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help,h", "Print this help message")
        ("watch,d", po::value<string>()->default_value(""),
         "Watch the specified directory for multicast address files")
        ("log", po::value<string>()->default_value(""),
         "Log to the specified file (default standard out)")
        ("level", po::value<string>()->default_value("info"),
         "Use the specified log level (default info).")
        ("syslog", "Log to syslog instead of file or standard out")
        ("daemon", "Run the multicast daemon as a daemon")
        ;

    bool daemon = false;
    bool logToSyslog = false;
    string log_file;
    string level_str;
    string watch_dir;

    po::variables_map vm;
    try {
        po::store(po::command_line_parser(argc, argv).
                  options(desc).run(), vm);
        po::notify(vm);

        if (vm.count("help")) {
            std::cout << "Usage: " << argv[0] << " [options]\n";
            std::cout << desc;
            return 0;
        }
        if (vm.count("daemon")) {
            daemon = true;
        }
        watch_dir = vm["watch"].as<string>();
        if (watch_dir == "")
            watch_dir = DEFAULT_WATCH;
        log_file = vm["log"].as<string>();
        level_str = vm["level"].as<string>();
        if (vm.count("syslog")) {
            logToSyslog = true;
        }
    } catch (po::unknown_option& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    if (daemon)
        daemonize();

    initLogging(level_str, logToSyslog, log_file, "mcast-daemon");

    try {
        boost::asio::io_service io;
        unique_ptr<boost::asio::io_service::work>
            work(new boost::asio::io_service::work(io));
        MulticastListener listener(io);

        std::thread io_thread([&io]() { io.run(); });

        FSWatcher watcher;
        McastWatcher mwatcher(io, listener);

        LOG(INFO) << "Watching " << watch_dir << " for multicast addresses";
        watcher.addWatch(watch_dir, mwatcher);
        watcher.start();

        // Pause the main thread until interrupted
        signal(SIGINT, sighandler);
        signal(SIGTERM, sighandler);
        pause();
        work.reset();
        io_thread.join();
        watcher.stop();
        listener.stop();
        return 0;
    } catch (const std::exception& e) {
        LOG(ERROR) << "Fatal error: " << e.what();
        return 2;
    } catch (...) {
        LOG(ERROR) << "Unknown fatal error";
        return 3;
    }
}
