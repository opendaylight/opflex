/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for Span Listener
 *
 * Copyright (c) 2019 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef OPFLEX_NETFOWLISTENER_H
#define OPFLEX_NETFOWLISTENER_H

#include <opflexagent/ExporterConfigState.h>

namespace opflexagent {

using namespace std;

/**
 * class defining api for listening to net flow updates
 */
class NetFlowListener  {
public:
    /**
     * destroy span listener and clean up all state.
     */
    virtual ~NetFlowListener() {};

    /**
     * called when netflow exporterconfig has been deleted
     * @param[in] seSt shared pointer to exporterconfig object
     */
    virtual void netflowDeleted(shared_ptr<ExporterConfigState> exporterconfigSt) {};

    /**
     * Called when netflow objects are updated.
     */
     virtual void netflowUpdated(const opflex::modb::URI&) {}

     /**
      * delete all netflow config
      */
     virtual void netflowDeleted() {};

};
}
#endif // OPFLEX_NETFOWLISTENER_H
