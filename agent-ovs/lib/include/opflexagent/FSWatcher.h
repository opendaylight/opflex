/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for filesystem endpoint source
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef OPFLEXAGENT_FSLISTENER_H
#define OPFLEXAGENT_FSLISTENER_H

#include <opflexagent/EndpointSource.h>

#include <boost/filesystem.hpp>
#include <boost/noncopyable.hpp>

#include <string>
#include <unordered_map>
#include <thread>

namespace opflexagent {

/**
 * Use an inotify watch on the local filesystem to watch for changes
 * to specific paths.
 */
class FSWatcher : private boost::noncopyable  {
public:
    FSWatcher();
    ~FSWatcher();

    /**
     * A watcher for updates to paths in the filesystem in the watch
     * directory.
     */
    class Watcher {
    public:
        virtual ~Watcher() {}
        /**
         * Called when the specified path is created or updated
         */
        virtual void updated(const boost::filesystem::path& filePath) = 0;
        /**
         * Called when the specified path is deleted
         */
        virtual void deleted(const boost::filesystem::path& filePath) = 0;
    };

    /**
     * Add a filesystem watcher that will watch the specified path.
     */
    void addWatch(const std::string& watchDir, Watcher& watcher);

    /**
     * Enable or disable an initial notification on update.
     *
     * @param scan true to enable the initial scan on start or false
     * to disable the scan.  Default true.
     */
    void setInitialScan(bool scan);

    /**
     * Start the listener on the currently registered set of watchers
     */
    void start();

    /**
     * Stop the listeners
     */
    void stop();

    /**
     * Polling thread function
     */
    void operator()();

private:
    struct WatchState : private boost::noncopyable {
        std::vector<Watcher*> watchers;
        boost::filesystem::path watchPath;
    };

    /**
     * Functor for storing a boost::filesystem::path as hash key
     */
    struct PathHash {
        /**
         * Hash the path
         */
        size_t operator()(const boost::filesystem::path& p) const noexcept;
    };

    typedef std::unordered_map<boost::filesystem::path,
                               WatchState, PathHash> path_map_t;

    /**
     * paths to monitor.
     */
    path_map_t regWatches;

    /**
     * Active watches
     */
    std::unordered_map<int, const WatchState*> activeWatches;

    /**
     * Thread for polling filesystem events
     */
    std::unique_ptr<std::thread> pollThread;

    /**
     * File descriptor for communicating with the polling thread
     */
    int eventFd;
    bool initialScan;

    void scanPath(const WatchState* ws,
                  const boost::filesystem::path& watchPath);
};

} /* namespace opflexagent */

#endif /* OPFLEXAGENT_FSLISTENER_H */
