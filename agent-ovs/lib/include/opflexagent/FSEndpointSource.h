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
#ifndef OPFLEXAGENT_FSENDPOINTSOURCE_H
#define OPFLEXAGENT_FSENDPOINTSOURCE_H

#include <opflexagent/EndpointSource.h>
#include <opflexagent/FSWatcher.h>

#include <boost/filesystem.hpp>

#include <unordered_map>
#include <string>

namespace opflexagent {

/**
 * An endpoint source that gets information about endpoints from JSON
 * files on the filesystem.  If supported, it will set an inotify
 * watch on the directory
 */
class FSEndpointSource
    : public EndpointSource, public FSWatcher::Watcher {
public:
    /**
     * Instantiate a new endpoint source using the specified endpoint
     * manager.  It will set a watch on the given path.
     */
    FSEndpointSource(EndpointManager* manager,
                     FSWatcher& listener,
                     const std::string& endpointDir);

    /**
     * Destroy the endpoint source and clean up all state
     */
    virtual ~FSEndpointSource() {}

    // See Watcher
    virtual void updated(const boost::filesystem::path& filePath);
    // See Watcher
    virtual void deleted(const boost::filesystem::path& filePath);

private:
    typedef std::unordered_map<std::string, std::string> ep_map_t;

    /**
     * EPs that are known to the filesystem watcher
     */
    ep_map_t knownEps;
};

} /* namespace opflexagent */

#endif /* OPFLEXAGENT_FSENDPOINTSOURCE_H */
