/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for filesystem drop log config source
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef OPFLEXAGENT_PACKETDROPLOGCONFIGSOURCE_H
#define OPFLEXAGENT_PACKETDROPLOGCONFIGSOURCE_H

#include <opflexagent/FSWatcher.h>
#include <boost/filesystem.hpp>

#include <opflexagent/PacketDropLogConfig.h>

#include <string>

namespace opflexagent {

class ExtraConfigManager;

/**
 * An routing domain config source that gets extra configuration
 * information for routing domains from the filesystem.  This serves
 * as a temporary bypass to work around a model issue with ACI and
 * will be removed in a future version.
 */
class FSPacketDropLogConfigSource : public FSWatcher::Watcher {
public:
    /**
     * Instantiate a new routing domain config source.  It will set a
     * watch on the given path.
     */
    FSPacketDropLogConfigSource(ExtraConfigManager* manager,
                     FSWatcher& listener,
                     const std::string& PktDropLogConfigDir,
                     const opflex::modb::URI& uri);

    /**
     * Destroy the routing domain config source and clean up all state
     */
    virtual ~FSPacketDropLogConfigSource() {}

    // See Watcher
    virtual void updated(const boost::filesystem::path& filePath);
    // See Watcher
    virtual void deleted(const boost::filesystem::path& filePath);

private:
    ExtraConfigManager* manager;
    PacketDropLogConfig dropCfg;
    DropFlowMap dropFlowMap;
};

} /* namespace opflexagent */

#endif /* OPFLEXAGENT_PACKETDROPLOGCONFIGSOURCE_H */
