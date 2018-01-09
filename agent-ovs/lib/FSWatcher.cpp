/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for FSWatcher class.
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

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include <opflex/modb/URIBuilder.h>

#include <opflexagent/FSWatcher.h>
#include <opflexagent/logging.h>

namespace opflexagent {

using std::thread;
using std::string;
using std::runtime_error;
using std::pair;
using boost::optional;
namespace fs = boost::filesystem;
using opflex::modb::URI;
using opflex::modb::MAC;

FSWatcher::FSWatcher() : eventFd(-1), initialScan(true) {

}

size_t FSWatcher::PathHash::
operator()(const boost::filesystem::path& p) const noexcept {
    return boost::filesystem::hash_value(p);
}

void FSWatcher::addWatch(const std::string& watchDir, Watcher& watcher) {
    fs::path wp(watchDir);
    WatchState& ws = regWatches[wp];
    ws.watchPath = wp;
    ws.watchers.push_back(&watcher);
}

void FSWatcher::setInitialScan(bool scan) {
    this->initialScan = scan;
}

void FSWatcher::start() {
#ifdef USE_INOTIFY
    for (const path_map_t::value_type& w : regWatches) {
        if (!fs::exists(w.first)) {
            throw runtime_error(string("Filesystem watch directory " ) +
                                w.first.string() +
                                string(" does not exist"));
        }
        if (!fs::is_directory(w.first)) {
            throw runtime_error(string("Filesystem watch directory ") +
                                w.first.string() +
                                string(" is not a directory"));
        }
    }

    eventFd = eventfd(0,0);
    if (eventFd < 0) {
        throw runtime_error(string("Could not allocate eventfd descriptor: ") +
                            strerror(errno));
    }

    pollThread.reset(new thread(std::ref(*this)));
#endif /* USE_INOTIFY */
}

void FSWatcher::stop() {
#ifdef USE_INOTIFY
    if (pollThread != NULL) {
        uint64_t u = 1;
        ssize_t s = write(eventFd, &u, sizeof(uint64_t));
        if (s != sizeof(uint64_t))
            throw runtime_error(string("Could not signal polling thread: ") +
                                strerror(errno));

        pollThread->join();
        pollThread.reset();

        close(eventFd);
        eventFd = -1;
    }

#endif /* USE_INOTIFY */
}

FSWatcher::~FSWatcher() {
    stop();
}

void FSWatcher::scanPath(const WatchState* ws,
                         const boost::filesystem::path& watchPath) {
    if (fs::is_directory(watchPath)) {
        fs::directory_iterator end;
        for (fs::directory_iterator it(watchPath); it != end; ++it) {
            if (fs::is_regular_file(it->status())) {
                for (Watcher* watcher : ws->watchers) {
                    watcher->updated(it->path());
                }
            }
        }
    }
}

void FSWatcher::operator()() {
#ifdef USE_INOTIFY
#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define EVENT_BUF_LEN     ( 1024 * ( EVENT_SIZE + 16 ) )
    struct pollfd fds[2];
    nfds_t nfds;
    char buf[EVENT_BUF_LEN];

    int fd = inotify_init1(IN_NONBLOCK);
    if (fd < 0) {
        LOG(ERROR) << "Could not initialize inotify: "
                   << strerror(errno);
        return;
    }
    for (const path_map_t::value_type& w : regWatches) {
        int wd = inotify_add_watch( fd, w.first.c_str(),
                                    IN_CLOSE_WRITE | IN_DELETE |
                                    IN_MOVED_TO| IN_MOVED_FROM);
        if (wd < 0) {
            LOG(ERROR) << "Could not add inotify watch for "
                       << w.first << ": "
                       << strerror(errno);
            goto cleanup;
        }
        activeWatches[wd] = &w.second;
        if (initialScan)
            scanPath(&w.second, w.first);
    }

    nfds = 2;
    // eventfd input
    fds[0].fd = eventFd;
    fds[0].events = POLLIN;

    // inotify input
    fds[1].fd = fd;
    fds[1].events = POLLIN;


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
                            const WatchState* ws = activeWatches.at(event->wd);
                            for (Watcher* watcher : ws->watchers) {
                                if ((event->mask & IN_CLOSE_WRITE) ||
                                    (event->mask & IN_MOVED_TO)) {
                                    watcher->updated(ws->watchPath / event->name);
                                } else if ((event->mask & IN_DELETE) ||
                                           (event->mask & IN_MOVED_FROM)) {
                                    watcher->deleted(ws->watchPath / event->name);
                                }
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

} /* namespace opflexagent */
