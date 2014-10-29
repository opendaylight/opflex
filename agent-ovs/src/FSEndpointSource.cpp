/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for FSEndpointSource class.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <config.h>
#if defined(HAVE_SYS_INOTIFY_H) && defined(HAVE_SYS_EVENTFD_H)
#define USE_INOTIFY
#endif

#include <stdexcept>
#include <sstream>

#ifdef USE_INOTIFY
#include <sys/inotify.h>
#include <sys/eventfd.h>
#endif
#include <poll.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <boost/foreach.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <glog/logging.h>

#include "FSEndpointSource.h"

namespace ovsagent {

using boost::chrono::milliseconds;
using boost::this_thread::sleep_for;
using boost::thread;
using boost::thread_interrupted;
using boost::scoped_array;
using boost::optional;
namespace fs = boost::filesystem;
using std::string;
using std::runtime_error;
using opflex::modb::URI;

FSEndpointSource::FSEndpointSource(EndpointManager* manager_,
                                   const std::string& endpointDir_) 
    : EndpointSource(manager_), endpointDir(endpointDir_), eventFd(-1) {
    start();
}

FSEndpointSource::~FSEndpointSource() {
    stop();
}

void FSEndpointSource::start() {
#ifdef USE_INOTIFY
    if (!fs::exists(endpointDir)) {
        throw runtime_error(string("Endpoint directory " ) + 
                            endpointDir.string() + 
                            string(" does not exist"));
    }
    if (!fs::is_directory(endpointDir)) {
        throw runtime_error(string("Endpoint directory ") + 
                            endpointDir.string() +
                            string(" is not a directory"));
    }

    eventFd = eventfd(0,0);
    if (eventFd < 0) {
        throw runtime_error(string("Could not allocate eventfd descriptor: ") +
                            strerror(errno));
    }

    pollThread = new thread(boost::ref(*this));
#endif /* USE_INOTIFY */
}

void FSEndpointSource::stop() {
#ifdef USE_INOTIFY
    if (pollThread != NULL) {
        uint64_t u = 1;
        ssize_t s = write(eventFd, &u, sizeof(uint64_t));
        if (s != sizeof(uint64_t)) 
            throw runtime_error(string("Could not signal polling thread: ") +
                                strerror(errno));

        pollThread->join();
        delete pollThread;

        close(eventFd);
    }

#endif /* USE_INOTIFY */
}

void FSEndpointSource::scanPath() {
    if (fs::is_directory(endpointDir)) {
        fs::directory_iterator end;
        for (fs::directory_iterator it(endpointDir); it != end; ++it) {
            if (fs::is_regular_file(it->status())) {
                readEndpoint(it->path());
            }
        }
    }
}

static uint64_t parseMac(string macstr) {
    uint64_t result = 0;
    uint8_t* r8 = (uint8_t*)&result;
    int r[6];
    if (std::sscanf(macstr.c_str(),
                   "%02x:%02x:%02x:%02x:%02x:%02x",
                    r, r+1, r+2, r+3, r+4, r+5) != 6) {
        throw std::runtime_error(macstr + " is not a valid MAC address");
    }

    for (int i = 0; i < 6; ++i) {
        if (ntohl(1) == 1)
            r8[i+2] = (uint8_t)r[i];
        else
            r8[8-(i+2)] = (uint8_t)r[i];
    }
    return result;
}

static bool isep(fs::path filePath) {
    string fstr = filePath.filename().string();
    return (boost::algorithm::ends_with(fstr, ".ep") &&
            !boost::algorithm::starts_with(fstr, "."));
}

void FSEndpointSource::readEndpoint(fs::path filePath) {
    if (!isep(filePath)) return;

    static const std::string UUID("uuid");
    static const std::string MAC("mac");
    static const std::string IP("ip");
    static const std::string ENDPOINT_GROUP("endpoint-group");
    static const std::string IFACE_NAME("interface-name");

    try {
        using boost::property_tree::ptree;
        Endpoint newep;
        ptree properties;

        string pathstr = filePath.string();

        read_json(pathstr, properties);
        newep.setUUID(properties.get<string>(UUID));
        newep.setMAC(parseMac(properties.get<string>(MAC)));
        optional<ptree&> ips = properties.get_child_optional(IP);
        if (ips) {
            BOOST_FOREACH(const ptree::value_type &v, ips.get())
                newep.addIP(v.second.data());
        }
        optional<string> eg = 
            properties.get_optional<string>(ENDPOINT_GROUP);
        if (eg)
            newep.setEgURI(URI(eg.get()));
        optional<string> iface = 
            properties.get_optional<string>(IFACE_NAME);
        if (iface)
            newep.setInterfaceName(iface.get());

        knownEps[pathstr] = newep.getUUID();
        updateEndpoint(newep);

        LOG(INFO) << "Updated endpoint " << newep 
                  << " from " << filePath;

    } catch (const std::exception& ex) {
        LOG(ERROR) << "Could not load endpoint from: "
                   << filePath << ": "
                   << ex.what();
    } catch (...) {
        LOG(ERROR) << "Unknown error while loading endpoint information from "
                   << filePath;
    }
}

void FSEndpointSource::deleteEndpoint(fs::path filePath) {

    try {
        string pathstr = filePath.string();
        ep_map_t::iterator it = knownEps.find(pathstr);
        if (it != knownEps.end()) {
            LOG(INFO) << "Removed endpoint " 
                      << it->second 
                      << " at " << filePath;
            removeEndpoint(it->second);
        }
    } catch (const std::exception& ex) {
        LOG(ERROR) << "Could not delete endpoint for "
                   << filePath << ": "
                   << ex.what();
    } catch (...) {
        LOG(ERROR) << "Unknown error while deleting endpoint information for "
                   << filePath;
    }
}

void FSEndpointSource::operator()() {
#ifdef USE_INOTIFY
#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define EVENT_BUF_LEN     ( 1024 * ( EVENT_SIZE + 16 ) )
    struct pollfd fds[2];
    nfds_t nfds;
    char buf[EVENT_BUF_LEN];

    LOG(INFO) << "Watching " << endpointDir << " for endpoint data";

    int fd = inotify_init1(IN_NONBLOCK);
    if (fd < 0) {
        LOG(ERROR) << "Could not initialize inotify: "
                   << strerror(errno);
        return;
    }
    int wd = inotify_add_watch( fd, endpointDir.c_str(), 
                                IN_CLOSE_WRITE | IN_DELETE | 
                                IN_MOVED_TO| IN_MOVED_FROM);
    if (wd < 0) {
        LOG(ERROR) << "Could not add inotify watch for "
                   << endpointDir << ": "
                   << strerror(errno);
        return;
    }

    nfds = 2;
    // eventfd input
    fds[0].fd = eventFd;
    fds[0].events = POLLIN;
    
    // inotify input
    fds[1].fd = fd;
    fds[1].events = POLLIN;

    scanPath();

    while (true) {
        int poll_num = poll(fds, nfds, -1);
        if (poll_num < 0) {
            if (errno == EINTR)
                continue;
            LOG(ERROR) << "Error while polling events: "
                       << strerror(errno);
            break;
        }
        if (poll_num > 0) {
            if (fds[0].revents & POLLIN) {
                // notification on eventfd descriptor; exit the
                // thread
                break;
            }
            if (fds[1].revents & POLLIN) {
                // inotify events are available
                while (true) {
                    ssize_t len = read(fd, buf, sizeof buf);
                    if (len < 0 && errno != EAGAIN) {
                        LOG(ERROR) << "Error while reading inotify events: "
                                   << strerror(errno);
                        goto cleanup;
                    }

                    if (len < 0) break;
                    
                    const struct inotify_event *event;
                    for (char* ptr = buf; ptr < buf + len;
                         ptr += sizeof(struct inotify_event) + event->len) {
                        event = (const struct inotify_event *) ptr;

                        if (event->len) {
                            if ((event->mask & IN_CLOSE_WRITE) ||
                                (event->mask & IN_MOVED_TO)) {
                                readEndpoint(endpointDir / event->name);
                            } else if ((event->mask & IN_DELETE) ||
                                (event->mask & IN_MOVED_FROM)) {
                                deleteEndpoint(endpointDir / event->name);
                            }
                        }
                    }
                }
            }
        }

    }
 cleanup:

    close(fd);

#endif /* USE_INOTIFY */
}

} /* namespace ovsagent */
