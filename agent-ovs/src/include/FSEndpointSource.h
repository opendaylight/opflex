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

#include <string>

#include <boost/filesystem.hpp>
#include <boost/unordered_map.hpp>
#include <boost/thread.hpp>

#include "EndpointSource.h"

#pragma once
#ifndef OVSAGENT_FSENDPOINTSOURCE_H
#define OVSAGENT_FSENDPOINTSOURCE_H

namespace ovsagent {

/**
 * An endpoint source that gets information about endpoints from JSON
 * files on the filesystem.  If supported, it will set an inotify
 * watch on the directory
 */
class FSEndpointSource : public EndpointSource {
public:
    /**
     * Instantiate a new endpoint source using the specified endpoint
     * manager.  It will set a watch on the given path.
     */
    FSEndpointSource(EndpointManager* manager, 
                     const std::string& endpointDir);

    /**
     * Destroy the endpoint source and clean up all state
     */
    virtual ~FSEndpointSource();

    // see EndpointSource
    virtual void start();

    // see EndpointSource
    virtual void stop();

    /**
     * Polling thread function
     */
    void operator()();

private:
    /**
     * path to monitor
     */
    boost::filesystem::path endpointDir;

    /**
     * Thread for polling filesystem events
     */
    boost::thread* pollThread;

    /**
     * File descriptor for communicating with the polling thread
     */
    int eventFd;

    typedef boost::unordered_map<std::string, std::string> ep_map_t;

    /** 
     * EPs that are known to the filesystem watcher
     */
    ep_map_t knownEps;

    void scanPath();
    void readEndpoint(boost::filesystem::path filePath);
    void deleteEndpoint(boost::filesystem::path filePath);
};

} /* namespace ovsagent */

#endif /* OVSAGENT_FSENDPOINTSOURCE_H */
