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
#ifndef OPFLEX_SPANLISTENER_H
#define OPFLEX_SPANLISTENER_H

#include <opflexagent/SpanSessionState.h>

namespace opflexagent {

using namespace std;

/**
 * class defining api for listening to span updates
 */
class SpanListener  {
public:
    /**
     * destroy span listener and clean up all state.
     */
    virtual ~SpanListener() {};

    /**
     * called when span session has been deleted
     * @param[in] seSt shared pointer to SessionState object
     */
    virtual void sessionDeleted(shared_ptr<SessionState> seSt) {};

    /**
     * Called when span objects are updated.
     */
     virtual void spanUpdated(const opflex::modb::URI&) {}

};
}
#endif // OPFLEX_SPANLISTENER_H
