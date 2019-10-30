/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file InspectorClient.h
 * @brief Interface definition file for MODB inspector client
 */
/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef ENGINE_INSPECTORCLIENTIMPL_H
#define ENGINE_INSPECTORCLIENTIMPL_H

#include <string>

#include "opflex/ofcore/InspectorClient.h"
#include "opflex/modb/internal/ObjectStore.h"
#include "opflex/engine/internal/InspectorClientHandler.h"
#include "opflex/engine/internal/InspectorClientConn.h"
#include "opflex/engine/internal/MOSerializer.h"
#include "opflex/gbp/Policy.h"

namespace opflex {
namespace engine {

class Cmd;

/**
 * Inspect the state of a a managed object database using the
 * inspector protocol.  Can query for objects, dump the database
 * state, and other simple tasks.
 */
class InspectorClientImpl : public ofcore::InspectorClient,
                            public internal::HandlerFactory,
                            public internal::MOSerializer::Listener {
public:
    /**
     * Construct a new inspector client
     *
     * @param name the unix socket name
     * @param model the model metadata object
     */
    InspectorClientImpl(const std::string& name,
                        const modb::ModelMetadata& model);

    /**
     * Destroy the inspector client
     */
    virtual ~InspectorClientImpl();

    /**
     * Get the object store
     *
     * @return the object store for this inspector client
     */
    modb::ObjectStore& getStore() { return db; }

    /**
     * Get the connection
     *
     * @return the connection object
     */
    internal::InspectorClientConn& getConn() { return conn; }

    // ***************
    // InspectorClient
    // ***************

    virtual void setFollowRefs(bool enabled);
    virtual void setRecursive(bool enabled);
    virtual void addQuery(const std::string& subject,
                          const modb::URI& uri);
    virtual void addClassQuery(const std::string& subject);
    virtual void execute();
    virtual void dumpToFile(FILE* file);
    virtual size_t loadFromFile(FILE* file);
    virtual void prettyPrint(std::ostream& output,
                             bool tree = true,
                             bool includeProps = true,
                             bool utf8 = true,
                             size_t truncate = 0);

    // **************
    // HandlerFactory
    // **************

    virtual internal::OpflexHandler*
    newHandler(internal::OpflexConnection* conn);

    // **********************
    // MOSerializer::Listener
    // **********************

    virtual void remoteObjectUpdated(modb::class_id_t class_id,
                                     const modb::URI& uri,
                                     gbp::PolicyUpdateOp op);

private:
    internal::InspectorClientConn conn;
    util::ThreadManager threadManager;
    modb::ObjectStore db;
    internal::MOSerializer serializer;
    modb::mointernal::StoreClient* storeClient;

    std::list<Cmd*> commands;
    unsigned int pendingRequests;
    bool followRefs;
    bool recursive;

    friend class internal::InspectorClientHandler;

    void executeCommands();
};

} /* namespace engine */
} /* namespace opflex */

#endif /* ENGINE_INSPECTORCLIENTIMPL_H */
