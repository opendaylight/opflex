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
#include <opflex/modb/URIBuilder.h>

#include "FSEndpointSource.h"
#include "logging.h"

namespace ovsagent {

using boost::thread;
using boost::scoped_array;
using boost::optional;
namespace fs = boost::filesystem;
using std::string;
using std::runtime_error;
using std::pair;
using opflex::modb::URI;
using opflex::modb::MAC;

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

static bool isep(fs::path filePath) {
    string fstr = filePath.filename().string();
    return (boost::algorithm::ends_with(fstr, ".ep") &&
            !boost::algorithm::starts_with(fstr, "."));
}

void FSEndpointSource::readEndpoint(fs::path filePath) {
    if (!isep(filePath)) return;

    static const std::string EP_UUID("uuid");
    static const std::string EP_MAC("mac");
    static const std::string EP_IP("ip");
    static const std::string EP_GROUP("endpoint-group");
    static const std::string POLICY_SPACE_NAME("policy-space-name");
    static const std::string EG_MAPPING_ALIAS("eg-mapping-alias");
    static const std::string EP_GROUP_NAME("endpoint-group-name");
    static const std::string EP_IFACE_NAME("interface-name");
    static const std::string EP_PROMISCUOUS("promiscuous-mode");
    static const std::string EP_ATTRIBUTES("attributes");

    static const std::string DHCP4("dhcp4");
    static const std::string DHCP6("dhcp6");
    static const std::string DHCP_IP("ip");
    static const std::string DHCP_PREFIX_LEN("prefix-len");
    static const std::string DHCP_ROUTERS("routers");
    static const std::string DHCP_DNS_SERVERS("dns-servers");
    static const std::string DHCP_DOMAIN("domain");
    static const std::string DHCP_SEARCH_LIST("search-list");
    static const std::string DHCP_STATIC_ROUTES("static-routes");
    static const std::string DHCP_STATIC_ROUTE_DEST("dest");
    static const std::string DHCP_STATIC_ROUTE_DEST_PREFIX("dest-prefix");
    static const std::string DHCP_STATIC_ROUTE_NEXTHOP("next-hop");

    static const std::string FLOATING_IP("floating-ip");
    static const std::string MAPPED_IP("mapped-ip");

    try {
        using boost::property_tree::ptree;
        Endpoint newep;
        ptree properties;

        string pathstr = filePath.string();

        read_json(pathstr, properties);
        newep.setUUID(properties.get<string>(EP_UUID));
        optional<string> mac = properties.get_optional<string>(EP_MAC);
        if (mac) {
            newep.setMAC(MAC(mac.get()));
        }
        optional<ptree&> ips = properties.get_child_optional(EP_IP);
        if (ips) {
            BOOST_FOREACH(const ptree::value_type &v, ips.get())
                newep.addIP(v.second.data());
        }

        optional<string> eg = 
            properties.get_optional<string>(EP_GROUP);
        if (eg) {
            newep.setEgURI(URI(eg.get()));
        } else {
            optional<string> eg_name = 
                properties.get_optional<string>(EP_GROUP_NAME);
            optional<string> ps_name = 
                properties.get_optional<string>(POLICY_SPACE_NAME);
            if (eg_name && ps_name) {
                newep.setEgURI(opflex::modb::URIBuilder()
                               .addElement("PolicyUniverse")
                               .addElement("PolicySpace")
                               .addElement(ps_name.get())
                               .addElement("GbpEpGroup")
                               .addElement(eg_name.get()).build());
            } else {
                optional<string> eg_mapping_alias =
                    properties.get_optional<string>(EG_MAPPING_ALIAS);
                if (eg_mapping_alias) {
                    newep.setEgMappingAlias(eg_mapping_alias.get());
                }
            }
        }

        optional<string> iface = 
            properties.get_optional<string>(EP_IFACE_NAME);
        if (iface)
            newep.setInterfaceName(iface.get());
        optional<bool> promisc = 
            properties.get_optional<bool>(EP_PROMISCUOUS);
        if (promisc)
            newep.setPromiscuousMode(promisc.get());

        optional<ptree&> attrs =
            properties.get_child_optional(EP_ATTRIBUTES);
        if (attrs) {
            BOOST_FOREACH(const ptree::value_type &v, attrs.get()) {
                newep.addAttribute(v.first, v.second.data());
            }
        }

        optional<ptree&> dhcp4 = properties.get_child_optional(DHCP4);
        if (dhcp4) {
            Endpoint::DHCPv4Config c;

            optional<string> ip =
                dhcp4.get().get_optional<string>(DHCP_IP);
            if (ip)
                c.setIpAddress(ip.get());

            optional<uint8_t> prefix =
                dhcp4.get().get_optional<uint8_t>(DHCP_PREFIX_LEN);
            if (prefix)
                c.setPrefixLen(prefix.get());

            optional<ptree&> routers =
                dhcp4.get().get_child_optional(DHCP_ROUTERS);
            if (routers) {
                BOOST_FOREACH(const ptree::value_type &u, routers.get())
                    c.addRouter(u.second.data());
            }

            optional<ptree&> dns =
                dhcp4.get().get_child_optional(DHCP_DNS_SERVERS);
            if (dns) {
                BOOST_FOREACH(const ptree::value_type &u, dns.get())
                    c.addDnsServer(u.second.data());
            }

            optional<string> domain =
                dhcp4.get().get_optional<string>(DHCP_DOMAIN);
            if (domain)
                c.setDomain(domain.get());

            optional<ptree&> staticRoutes =
                dhcp4.get().get_child_optional(DHCP_STATIC_ROUTES);
            if (staticRoutes) {
                BOOST_FOREACH(const ptree::value_type &u,
                              staticRoutes.get()) {
                    optional<string> dst = u.second.get_optional<string>
                        (DHCP_STATIC_ROUTE_DEST);
                    uint8_t dstPrefix =
                        u.second.get<uint8_t>
                        (DHCP_STATIC_ROUTE_DEST_PREFIX, 32);
                    optional<string> nextHop = u.second.get_optional<string>
                        (DHCP_STATIC_ROUTE_NEXTHOP);
                    if (dst && nextHop)
                            c.addStaticRoute(dst.get(),
                                             dstPrefix,
                                             nextHop.get());
                }
            }

            newep.setDHCPv4Config(c);
        }

        optional<ptree&> dhcp6 = properties.get_child_optional(DHCP6);
        if (dhcp6) {
            Endpoint::DHCPv6Config c;

            optional<ptree&> searchPath =
                dhcp6.get().get_child_optional(DHCP_SEARCH_LIST);
            if (searchPath) {
                BOOST_FOREACH(const ptree::value_type &u, searchPath.get())
                    c.addSearchListEntry(u.second.data());
            }

            optional<ptree&> dns =
                dhcp6.get().get_child_optional(DHCP_DNS_SERVERS);
            if (dns) {
                BOOST_FOREACH(const ptree::value_type &u, dns.get())
                    c.addDnsServer(u.second.data());
            }

            newep.setDHCPv6Config(c);
        }

        optional<ptree&> fips =
            properties.get_child_optional(FLOATING_IP);
        if (fips) {
            BOOST_FOREACH(const ptree::value_type &v, fips.get()) {
                optional<string> fuuid =
                    v.second.get_optional<string>(EP_UUID);
                if (!fuuid) continue;

                Endpoint::FloatingIP fip(fuuid.get());

                optional<string> ip =
                    v.second.get_optional<string>(EP_IP);
                if (ip)
                    fip.setIP(ip.get());

                optional<string> mappedIp =
                    v.second.get_optional<string>(MAPPED_IP);
                if (mappedIp)
                    fip.setMappedIP(mappedIp.get());

                optional<string> feg =
                    v.second.get_optional<string>(EP_GROUP);
                if (feg) {
                    fip.setEgURI(URI(feg.get()));
                } else {
                    optional<string> feg_name =
                        v.second.get_optional<string>(EP_GROUP_NAME);
                    optional<string> fps_name =
                        v.second.get_optional<string>(POLICY_SPACE_NAME);
                    if (feg_name && fps_name) {
                        fip.setEgURI(opflex::modb::URIBuilder()
                                     .addElement("PolicyUniverse")
                                     .addElement("PolicySpace")
                                     .addElement(fps_name.get())
                                     .addElement("GbpEpGroup")
                                     .addElement(feg_name.get()).build());
                    }
                }

                if (fip.getIP() && fip.getMappedIP() && fip.getEgURI())
                    newep.addFloatingIP(fip);
            }
        }

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
            knownEps.erase(it);
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
