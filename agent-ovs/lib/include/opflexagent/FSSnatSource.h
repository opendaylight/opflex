/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for filesystem snat source
 *
 * Copyright (c) 2019 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef OPFLEXAGENT_FSSNATSOURCE_H
#define OPFLEXAGENT_FSSNATSOURCE_H

#include <opflexagent/SnatSource.h>
#include <opflexagent/FSWatcher.h>

#include <boost/filesystem.hpp>

#include <string>
#include <unordered_map>

namespace opflexagent {

/**
 * An snat source that gets information about snats from JSON
 * files on the filesystem.  If supported, it will set an inotify
 * watch on the directory
 */
class FSSnatSource
    : public SnatSource, public FSWatcher::Watcher {
public:
    /**
     * Instantiate a new snat source using the specified snat
     * manager.  It will set a watch on the given path.
     */
    FSSnatSource(SnatManager* manager,
                 FSWatcher& listener,
                 const std::string& snatDir);

    /**
     * Destroy the snat source and clean up all state
     */
    virtual ~FSSnatSource() {}

    // See Watcher
    virtual void updated(const boost::filesystem::path& filePath);
    // See Watcher
    virtual void deleted(const boost::filesystem::path& filePath);

private:
    typedef std::pair<std::string, std::string> snat_pair_t;
    // Map filePath to <snat-ip, uuid>
    typedef std::unordered_map<std::string, snat_pair_t> snat_map_t;

    /**
     * Snats that are known to the filesystem watcher
     */
    snat_map_t knownSnats;
};

} /* namespace opflexagent */

#endif /* OPFLEXAGENT_FSSNATSOURCE_H */
