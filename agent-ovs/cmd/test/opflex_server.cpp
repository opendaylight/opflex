/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Opflex server
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */
#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include <unistd.h>
#include <csignal>
#include <sys/inotify.h>

#include <string>
#include <vector>
#include <iostream>

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

#include <modelgbp/dmtree/Root.hpp>
#include <modelgbp/metadata/metadata.hpp>
#include <opflex/test/GbpOpflexServer.h>
#include <opflex/ofcore/OFFramework.h>
#include <opflex/ofcore/OFConstants.h>

#include <opflexagent/logging.h>
#include <opflexagent/cmd.h>
#include <rapidjson/filereadstream.h>
#include "Policies.h"
#ifdef HAVE_GRPC_SUPPORT
#include "GbpClient.h"
#endif

using std::string;
using std::make_pair;
namespace po = boost::program_options;
using opflex::test::GbpOpflexServer;
using opflex::ofcore::OFConstants;
using namespace opflexagent;

void sighandler(int sig) {
    LOG(INFO) << "Got " << strsignal(sig) << " signal";
}

#define SERVER_ROLES \
        (OFConstants::POLICY_REPOSITORY |     \
         OFConstants::ENDPOINT_REGISTRY |     \
         OFConstants::OBSERVER)
#define LOCALHOST "127.0.0.1"
#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define EVENT_BUF_LEN     ( 1024 * ( EVENT_SIZE + 16 ) )

int main(int argc, char** argv) {
    signal(SIGPIPE, SIG_IGN);
    // Parse command line options
    po::options_description desc("Allowed options");
    try {
        desc.add_options()
            ("help,h", "Print this help message")
            ("log", po::value<string>()->default_value(""),
             "Log to the specified file (default standard out)")
            ("level", po::value<string>()->default_value("info"),
             "Use the specified log level (default info)")
            ("sample", po::value<string>()->default_value(""),
             "Output a sample policy to the given file then exit")
            ("daemon", "Run the opflex server as a daemon")
            ("policy,p", po::value<string>()->default_value(""),
             "Read the specified policy file to seed the MODB")
            ("ssl_castore", po::value<string>()->default_value("/etc/ssl/certs/"),
             "Use the specified path or certificate file as the SSL CA store")
            ("ssl_key", po::value<string>()->default_value(""),
             "Enable SSL and use the private key specified")
            ("ssl_pass", po::value<string>()->default_value(""),
             "Use the specified password for the private key")
            ("peer", po::value<std::vector<string> >(),
             "A peer specified as hostname:port to return in identity response")
            ("transport_mode_proxies", po::value<std::vector<string> >(),
             "3 transport_mode_proxy IPv4 addresses specified to return "
             "in identity response")
            ("grpc_address", po::value<string>()->default_value("localhost:19999"),
             "GRPC server address for policy updates")
            ("grpc_conf", po::value<string>()->default_value(""),
             "GRPC config file, should be in same directory as policy file")
            ("prr_interval_secs", po::value<int>()->default_value(60),
             "How often to wakeup prr thread to check for prr timeouts");
    } catch (const boost::bad_lexical_cast& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    bool daemon = false;
    std::string log_file;
    std::string level_str;
    std::string policy_file;
    boost::filesystem::path pf_path;
    boost::filesystem::path pf_dir;
    std::string sample_file;
    std::string ssl_castore;
    std::string ssl_key;
    std::string ssl_pass;
    std::vector<std::string> peers;
    std::vector<std::string> transport_mode_proxies;
    int prr_interval_secs;
#ifdef HAVE_GRPC_SUPPORT
    std::string grpc_address;
    std::string grpc_conf_file;
#endif
    char buf[EVENT_BUF_LEN];
    int fd, wd;

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
        log_file = vm["log"].as<string>();
        level_str = vm["level"].as<string>();
        policy_file = vm["policy"].as<string>();
        sample_file = vm["sample"].as<string>();
        ssl_castore = vm["ssl_castore"].as<string>();
        ssl_key = vm["ssl_key"].as<string>();
        ssl_pass = vm["ssl_pass"].as<string>();
        if (vm.count("peer"))
            peers = vm["peer"].as<std::vector<string> >();
        if(vm.count("transport_mode_proxies")) {
            transport_mode_proxies =
                vm["transport_mode_proxies"].as<std::vector<string>>();
        }
#ifdef HAVE_GRPC_SUPPORT
        grpc_conf_file = vm["grpc_conf"].as<string>();
        if (grpc_conf_file != "") {
            FILE* fp = fopen(grpc_conf_file.c_str(), "r");
            if (fp == NULL) {
                LOG(ERROR) << "Could not open grpc_conf file "
                           << grpc_conf_file << " for reading";
            } else {
                char buffer[1024];
                rapidjson::FileReadStream f(fp, buffer, sizeof(buffer));
                rapidjson::Document d;
                d.ParseStream<0, rapidjson::UTF8<>,
                    rapidjson::FileReadStream>(f);
                fclose(fp);
                if (d.IsObject()) {
                    if (d.HasMember("grpc-address")) {
                        const rapidjson::Value& grpc_addressv = d["grpc-address"];
                        if (grpc_addressv.IsString())
                            grpc_address = grpc_addressv.GetString();
                   }
                }
            }
        }
        if (grpc_address == "")
            grpc_address = vm["grpc_address"].as<string>();
#endif
        prr_interval_secs = vm["prr_interval_secs"].as<int>();
    } catch (const po::unknown_option& e) {
        std::cerr << e.what() << std::endl;
        return 2;
    } catch (const std::bad_cast& e) {
        std::cerr << e.what() << std::endl;
        return 3;
    }

    if (daemon)
        daemonize();

    initLogging(level_str, false /*syslog*/, log_file, "opflex-server");

    try {
        if (sample_file != "") {
            opflex::ofcore::MockOFFramework mframework;
            mframework.setModel(modelgbp::getMetadata());
            mframework.start();
            Policies::writeBasicInit(mframework);
            Policies::writeTestPolicy(mframework);

            mframework.dumpMODB(sample_file);

            mframework.stop();
            return 0;
        }

        GbpOpflexServer::peer_vec_t peer_vec;
        for (const std::string& pstr : peers)
            peer_vec.push_back(make_pair(SERVER_ROLES, pstr));
        if (peer_vec.size() == 0)
            peer_vec.push_back(make_pair(SERVER_ROLES, LOCALHOST":8009"));

        GbpOpflexServer server(8009, SERVER_ROLES, peer_vec,
                               transport_mode_proxies,
                               modelgbp::getMetadata(),
                               prr_interval_secs);
#ifdef HAVE_GRPC_SUPPORT
	LOG(INFO) << "Connecting to gbp-server at address: "
                  << grpc_address;
        GbpClient client(grpc_address, server);
#endif

        if (policy_file != "") {
            server.readPolicy(policy_file);
        }

        if (ssl_key != "") {
            server.enableSSL(ssl_castore, ssl_key, ssl_pass);
        }

        server.start();
        signal(SIGINT | SIGTERM, sighandler);
        fd = inotify_init();
        if (fd < 0) {
            LOG(ERROR) << "Could not initialize inotify: "
                       << strerror(errno);
            goto cleanup;
        }
        pf_path = policy_file;
        pf_dir = pf_path.parent_path();
        wd = inotify_add_watch(fd, pf_dir.c_str(), IN_CLOSE_WRITE);
        if (wd < 0) {
            LOG(ERROR) << "Could not add inotify watch for "
                       << policy_file << ": "
                       << strerror(errno);
            goto cleanup;
        } else {
            LOG(INFO) << "Watching policy/conf directory: "
                      << pf_dir;
        }
        while (true) {
            ssize_t len = read(fd, buf, sizeof buf);
            if (len < 0 && errno != EAGAIN) {
                LOG(ERROR) << "Error while reading inotify events: "
                           << strerror(errno);
                goto cleanup;
            }

            if (len < 0) continue;

            const struct inotify_event *event;
            for (char* ptr = buf; ptr < buf + len;
                 ptr += sizeof(struct inotify_event) + event->len) {
                 event = (const struct inotify_event *) ptr;

                if ((event->mask & IN_CLOSE_WRITE) && event->len > 0) {
                    LOG(INFO) << "Policy/Config dir modified : " << pf_dir;
#ifdef HAVE_GRPC_SUPPORT
                    client.Stop();
#endif
                    /* should be stopped after client */
                    server.stop();
                    if (execv(argv[0], argv)) {
                        LOG(ERROR) << "opflex_server failed to restart self"
                                   << strerror(errno);
                        goto cleanup;
                    }
                } else {
                    break;
                }
            }
        }
cleanup:
        server.stop();
    } catch (const std::exception& e) {
        LOG(ERROR) << "Fatal error: " << e.what();
        return 4;
    } catch (...) {
        LOG(ERROR) << "Unknown fatal error";
        return 5;
    }
}
