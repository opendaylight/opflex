/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * MODB inspection command-line tool
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <opflexagent/logging.h>
#include <opflexagent/cmd.h>
#include <signal.h>

#include <modelgbp/dmtree/Root.hpp>
#include <modelgbp/metadata/metadata.hpp>
#include <opflex/ofcore/OFFramework.h>
#include <opflex/modb/URI.h>
#include <opflex/ofcore/OFConstants.h>
#include <opflex/ofcore/InspectorClient.h>

#include <boost/program_options.hpp>
#include <boost/iostreams/device/file_descriptor.hpp>
#include <boost/iostreams/stream.hpp>

#include <string>
#include <vector>
#include <iostream>
#include <memory>

#include <unistd.h>
#include <signal.h>
#include <sys/ioctl.h>

using std::string;
using std::unique_ptr;
namespace po = boost::program_options;
using opflex::ofcore::InspectorClient;
using opflex::modb::URI;
using boost::iostreams::stream;
using boost::iostreams::close_handle;
using boost::iostreams::file_descriptor_sink;

using namespace opflexagent;

void sighandler(int sig) {
    LOG(INFO) << "Got " << strsignal(sig) << " signal";
}
#define DEF_SOCKET LOCALSTATEDIR"/run/opflex-agent-ovs-inspect.sock"

int main(int argc, char** argv) {
    signal(SIGPIPE, SIG_IGN);

    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

    // Parse command line options
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help,h", "Print this help message")
        ("log", po::value<string>()->default_value(""),
         "Log to the specified file (default standard out)")
        ("level", po::value<string>()->default_value("warning"),
         "Use the specified log level (default warning)")
        ("syslog", "Log to syslog instead of file or standard out")
        ("socket", po::value<string>()->default_value(DEF_SOCKET),
         "Connect to the specified UNIX domain socket (default " DEF_SOCKET ")")
        ("query,q", po::value<std::vector<string> >(),
         "Query for a specific object with subjectname,uri or all objects "
         "of a specific type with subjectname")
        ("recursive,r", "Retrieve the whole subtree for each returned object")
        ("follow-refs,f", "Follow references in returned objects")
        ("load", po::value<std::string>()->default_value(""),
         "Load managed objects from the specified file into the MODB view")
        ("output,o", po::value<std::string>()->default_value(""),
         "Output the results to the specified file (default standard out)")
        ("type,t", po::value<std::string>()->default_value("tree"),
         "Specify the output format: tree, asciitree, list, or dump "
         "(default tree)")
        ("props,p", "Include object properties in output")
        ("width,w", po::value<int>()->default_value(w.ws_col - 1),
         "Truncate output to the specified number of characters")
        ;

    bool log_to_syslog = false;
    string log_file;
    string level_str;

    string socket;
    std::vector<string> queries;
    string out_file;
    string load_file;
    string type;

    bool props = false;
    bool recursive = false;
    bool followRefs = false;
    int truncate = 0;

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
        if (vm.count("props"))
            props = true;
        if (vm.count("recursive"))
            recursive = true;
        if (vm.count("follow-refs"))
            followRefs = true;

        log_file = vm["log"].as<string>();
        level_str = vm["level"].as<string>();
        if (vm.count("syslog")) {
            log_to_syslog = true;
        }
        socket = vm["socket"].as<string>();
        out_file = vm["output"].as<string>();
        load_file = vm["load"].as<string>();
        type = vm["type"].as<string>();
        if (vm.count("query"))
            queries = vm["query"].as<std::vector<string> >();
        truncate = vm["width"].as<int>();
    } catch (po::unknown_option e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    if (truncate < 0) truncate = 0;

    initLogging(level_str, log_to_syslog, log_file, "gbp-inspect");

    if (queries.size() == 0 && load_file == "") {
        LOG(ERROR) << "No queries specified";
        return 1;
    }
    if (type != "tree" && type != "asciitree" &&
        type != "dump" && type != "list") {
        LOG(ERROR) << "Invalid output type: " << type;
        return 1;
    }

    try {
        unique_ptr<InspectorClient>
            client(InspectorClient::newInstance(socket,
                                                modelgbp::getMetadata()));
        client->setRecursive(recursive);
        client->setFollowRefs(followRefs);

        for (string query : queries) {
            size_t ci = query.find_first_of(",");
            if (ci == string::npos) {
                client->addClassQuery(query);
            } else {
                if (ci == 0 || ci >= (query.size()-1)) {
                    LOG(ERROR) << "Invalid query: " << query <<
                        ": must be in the form subject,uri or subject";
                    continue;
                }
                client->addQuery(query.substr(0, ci),
                                 URI(query.substr(ci+1, string::npos)));
            }
        }

        if (load_file != "") {
            FILE* inf = fopen(load_file.c_str(), "r");
            if (inf == NULL) {
                LOG(ERROR) << "Could not input file "
                           << load_file << " for reading";
                return 1;
            }
            client->loadFromFile(inf);
        }

        if (queries.size() > 0)
            client->execute();

        FILE* outf = stdout;
        if (out_file != "") {
            outf = fopen(out_file.c_str(), "w");
            if (outf == NULL) {
                LOG(ERROR) << "Could not open file "
                           << out_file << " for writing";
                return 1;
            }
        }
        stream<file_descriptor_sink> outs(fileno(outf), close_handle);

        if (type == "dump")
            client->dumpToFile(outf);
        else if (type == "list")
            client->prettyPrint(outs, false, props, true, truncate);
        else if (type == "asciitree")
            client->prettyPrint(outs, true, props, false, truncate);
        else
            client->prettyPrint(outs, true, props, true, truncate);

        fclose(outf);

    } catch (const std::exception& e) {
        LOG(ERROR) << "Fatal error: " << e.what();
        return 2;
    } catch (...) {
        LOG(ERROR) << "Unknown fatal error";
        return 3;
    }
}
