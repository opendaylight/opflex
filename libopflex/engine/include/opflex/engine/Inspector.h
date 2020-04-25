/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file Inspector.h
 * @brief Interface definition file for MODB inspector
 */
/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef ENGINE_INSPECTOR_H
#define ENGINE_INSPECTOR_H

#include <string>

#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>
#include <uv.h>

#include "opflex/engine/internal/OpflexConnection.h"
#include "opflex/engine/internal/OpflexListener.h"
#include "opflex/engine/internal/MOSerializer.h"

namespace opflex {

namespace modb {
class ObjectStore;
}

namespace engine {

namespace internal {
class InspectorServerHandler;
}

/**
 * Provides a way to inspect the state of the managed object database
 * using a simple IPC mechanism.  Along with associated command-line
 * tools, can query for objects, dump the state of the database, and
 * other simple tasks.
 */
class Inspector : public internal::HandlerFactory,
                  private boost::noncopyable {
public:
    /**
     * Construct a new inspector.
     *
     * @param db the MODB to inspect
     */
    Inspector(modb::ObjectStore* db);

    /**
     * Destroy the inspector
     */
    virtual ~Inspector();

    /**
     * Set the name for the socket/pipe
     *
     * @param name A path name for the unix socket
     */
    void setSocketName(const std::string& name);

    /**
     * Start the inspector
     */
    virtual void start();

    /**
     * Cleanly stop the inspector
     */
    virtual void stop();

    // See HandlerFactory::newHandler
    virtual internal::OpflexHandler* newHandler(internal::OpflexConnection* conn);

    /**
     * Get the object store for this server
     */
    modb::ObjectStore& getStore() { return *db; }

    /**
     * Get the MOSerializer for the server
     */
    internal::MOSerializer& getSerializer() { return serializer; }

private:
    modb::ObjectStore* db;
    internal::MOSerializer serializer;
    boost::scoped_ptr<internal::OpflexListener> listener;

    std::string name;

    friend class internal::InspectorServerHandler;
};

} /* namespace engine */
} /* namespace opflex */

#endif /* ENGINE_INSPECTOR_H */
