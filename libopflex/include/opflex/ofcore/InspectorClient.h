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

#ifndef OFCORE_INSPECTORCLIENT_H
#define OFCORE_INSPECTORCLIENT_H

#include <string>
#include <boost/noncopyable.hpp>

#include "opflex/modb/URI.h"
#include "opflex/modb/ModelMetadata.h"

namespace opflex {
namespace ofcore {

/**
 * \addtogroup cpp
 * @{
 * \addtogroup ofcore
 * @{
 */

/**
 * Inspect the state of a a managed object database using the
 * inspector protocol.  Can query for objects, dump the database
 * state, and other simple tasks.
 *
 * Use the client by queuing a list of commands, which will then be
 * executed in sequence.  Commands will operate to change the state of
 * the client's view of the managed object database by querying the
 * server.  The client's view can then either be pretty-printed for
 * viewing by a user or dumped to a file.
 */
class InspectorClient : private boost::noncopyable {
public:
    /**
     * Destroy the inspector client
     */
    virtual ~InspectorClient() {}

    /**
     * Allocate a new inspector client for the given socket name
     *
     * @param name A path name for the unix socket
     * @param model the model metadata object
     */
    static InspectorClient* newInstance(const std::string& name,
                                        const modb::ModelMetadata& model);

    /**
     * Follow references for retrieved objects
     *
     * @param enabled set to true to enable reference following
     */
    virtual void setFollowRefs(bool enabled) = 0;

    /**
     * Download the whole subtree rather than just the specific object
     * for each query
     *
     * @param enabled set to true to enable recursive downloading
     */
    virtual void setRecursive(bool enabled) = 0;

    /**
     * Query for a particular managed object
     *
     * @param subject the subject (class name) of the object
     * @param uri the URI of the object
     */
    virtual void addQuery(const std::string& subject,
                          const modb::URI& uri) = 0;

    /**
     * Query for all managed objects of a particular type
     *
     * @param subject the subject (class name) to query for
     */
    virtual void addClassQuery(const std::string& subject) = 0;

    /**
     * Attempt to execute all queued inspector commands
     */
    virtual void execute() = 0;

    /**
     * Dump the current MODB view to the specified file using the
     * Opflex JSON wire format
     *
     * @param file the file name to write to
     */
    virtual void dumpToFile(FILE* file) = 0;

    /**
     * Load a set of managed objects from the given file into the
     * inspector's MODB view in order to display them.
     *
     * @param file the file to load from
     * @return the number of managed objects loaded
     */
    virtual size_t loadFromFile(FILE* file) = 0;

    /**
     * Pretty print the current MODB to the provided output stream.
     *
     * @param output the output stream to write to
     * @param tree print in a tree format
     * @param includeProps include the object properties
     * @param utf8 output tree using UTF-8 box drawing
     * @param truncate truncate lines to the specified number of
     * characters.  0 means do not truncate.
     */
    virtual void prettyPrint(std::ostream& output,
                             bool tree = true,
                             bool includeProps = true,
                             bool utf8 = true,
                             size_t truncate = 0) = 0;
};

/** @} ofcore */
/** @} cpp */

} /* namespace ofcore */
} /* namespace opflex */

#endif /* OFCORE_INSPECTORCLIENT_H */
