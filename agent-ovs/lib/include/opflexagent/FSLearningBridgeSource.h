/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for filesystem learning bridge source
 *
 * Copyright (c) 2018 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef OPFLEXAGENT_FSLEARNINGBRIDGESOURCE_H
#define OPFLEXAGENT_FSLEARNINGBRIDGESOURCE_H

#include <opflexagent/LearningBridgeSource.h>
#include <opflexagent/FSWatcher.h>

#include <boost/filesystem.hpp>

#include <string>
#include <unordered_map>

namespace opflexagent {

/**
 * An learning bridge source that gets information about learning
 * bridges from JSON files on the filesystem.  If supported, it will
 * set an inotify watch on the directory
 */
class FSLearningBridgeSource
    : public LearningBridgeSource, public FSWatcher::Watcher {
public:
    /**
     * Instantiate a new learning bridge source using the specified
     * learning bridge manager.  It will set a watch on the given
     * path.
     */
    FSLearningBridgeSource(LearningBridgeManager* manager,
                           FSWatcher& listener,
                           const std::string& learningBridgeDir);

    /**
     * Destroy the learning bridge source and clean up all state
     */
    virtual ~FSLearningBridgeSource() {}

    // See Watcher
    virtual void updated(const boost::filesystem::path& filePath);
    // See Watcher
    virtual void deleted(const boost::filesystem::path& filePath);

private:
    typedef std::unordered_map<std::string, std::string> lbif_map_t;

    /**
     * Learning bridges interfaces that are known to the filesystem
     * watcher
     */
    lbif_map_t knownIfs;
};

} /* namespace opflexagent */

#endif /* OPFLEXAGENT_FSLEARNINGBRIDGESOURCE_H */
