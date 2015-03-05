/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Mock server standalone
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */
#include <unistd.h>
#include <signal.h>

#include <string>
#include <vector>
#include <iostream>

#include <boost/program_options.hpp>
#include <boost/foreach.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/iostreams/device/file_descriptor.hpp>
#include <boost/iostreams/stream.hpp>

#include <modelgbp/dmtree/Root.hpp>
#include <modelgbp/metadata/metadata.hpp>
#include <opflex/ofcore/OFFramework.h>
#include <opflex/modb/URI.h>
#include <opflex/ofcore/OFConstants.h>
#include <opflex/ofcore/InspectorClient.h>

#include "logging.h"
#include "cmd.h"
#include <signal.h>

using std::string;
using boost::scoped_ptr;
namespace po = boost::program_options;
using opflex::ofcore::InspectorClient;
using opflex::modb::URI;
using boost::iostreams::stream;
using boost::iostreams::close_handle;
using boost::iostreams::file_descriptor_sink;

using namespace ovsagent;

void sighandler(int sig) {
    LOG(INFO) << "Got " << strsignal(sig) << " signal";
}
#define DEF_SOCKET LOCALSTATEDIR"/run/opflex-agent-ovs-inspect.sock"

int main(int argc, char** argv) {
    signal(SIGPIPE, SIG_IGN);
    // Parse command line options
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help,h", "Print this help message")
        ("log", po::value<string>()->default_value(""), 
         "Log to the specified file (default standard out)")
        ("level", po::value<string>()->default_value("error"), 
         "Use the specified log level (default info)")
        ("socket", po::value<string>()->default_value(DEF_SOCKET),
         "Connect to the specified UNIX domain socket (default "DEF_SOCKET")")
        ("uri,u", po::value<std::vector<string> >(),
         "Query for a specific object with subjectname,uri")
        ("subject,s", po::value<std::vector<string> >(),
         "Query for all objects of the specified type")
        ("recursive,r", "Retrieve the whole subtree for each returned object")
        ("follow-refs,f", "Follow references in returned objects")
        ("output,o", po::value<std::string>()->default_value(""),
         "Output the results to the specified file (default standard out)")
        ("type,t", po::value<std::string>()->default_value("tree"),
         "Specify the output format: tree, list, or dump (default tree)")
        ("props,p", "Include object properties in output")
        ;

    string log_file;
    string level_str;

    string socket;
    std::vector<string> objects;
    std::vector<string> subjects;
    string out_file;
    string type;

    bool props = false;
    bool recursive = false;
    bool followRefs = false;

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
        socket = vm["socket"].as<string>();
        out_file = vm["output"].as<string>();
        type = vm["type"].as<string>();
        if (vm.count("uri"))
            objects = vm["uri"].as<std::vector<string> >();
        if (vm.count("subject"))
            subjects = vm["subject"].as<std::vector<string> >();
    } catch (po::unknown_option e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    initLogging(level_str, log_file);

    if (objects.size() == 0 && subjects.size() == 0) {
        LOG(ERROR) << "No queries specified; must include at least"
            " one uri or subject query";
        return 1;
    }
    if (type != "tree" && type != "dump" && type != "list") {
        LOG(ERROR) << "Invalid output type: " << type;
        return 1;
    }

    try {
        scoped_ptr<InspectorClient>
            client(InspectorClient::newInstance(socket,
                                                modelgbp::getMetadata()));
        client->setRecursive(recursive);
        client->setFollowRefs(followRefs);

        BOOST_FOREACH(string subject, subjects)
            client->addClassQuery(subject);

        BOOST_FOREACH(string object, objects) {
            size_t ci = object.find_first_of(",");
            if (ci == string::npos || ci == 0 || ci >= (object.size()-1)) {
                LOG(ERROR) << "Invalid object: " << object << 
                    ": must be in the form subject,uri";
                continue;
            }
            client->addQuery(object.substr(0, ci),
                             URI(object.substr(ci+1, string::npos)));
        }

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
            client->prettyPrint(outs, false, props);
        else
            client->prettyPrint(outs, true, props);

    } catch (const std::exception& e) {
        LOG(ERROR) << "Fatal error: " << e.what();
        return 2;
    } catch (...) {
        LOG(ERROR) << "Unknown fatal error";
        return 3;
    }
}
