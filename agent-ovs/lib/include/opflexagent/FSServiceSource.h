/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for filesystem service source
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef OPFLEXAGENT_FSSERVICESOURCE_H
#define OPFLEXAGENT_FSSERVICESOURCE_H

#include <opflexagent/ServiceSource.h>
#include <opflexagent/FSWatcher.h>

#include <boost/filesystem.hpp>

#include <string>
#include <unordered_map>

namespace opflexagent {

/**
 * An service source that gets information about services from JSON
 * files on the filesystem.  If supported, it will set an inotify
 * watch on the directory
 */
class FSServiceSource
    : public ServiceSource, public FSWatcher::Watcher {
public:
    /**
     * Instantiate a new service source using the specified service
     * manager.  It will set a watch on the given path.
     */
    FSServiceSource(ServiceManager* manager,
                    FSWatcher& listener,
                    const std::string& serviceDir);

    /**
     * Destroy the service source and clean up all state
     */
    virtual ~FSServiceSource() {}

    // See Watcher
    virtual void updated(const boost::filesystem::path& filePath);
    // See Watcher
    virtual void deleted(const boost::filesystem::path& filePath);

private:
    typedef std::unordered_map<std::string, std::string> serv_map_t;

    /**
     * Services that are known to the filesystem watcher
     */
    serv_map_t knownServs;
};

} /* namespace opflexagent */

#endif /* OPFLEXAGENT_FSSERVICESOURCE_H */
