/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for SpanManager listener
 *
 * Copyright (c) 2019 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef OPFLEXAGENT_SPANMANAGER_H
#define OPFLEXAGENT_SPANMANAGER_H

#include <opflex/ofcore/OFFramework.h>
#include <opflex/modb/ObjectListener.h>
#include <modelgbp/span/Universe.hpp>
#include <boost/asio.hpp>
#include <thread>

using boost::asio::deadline_timer;


namespace opflexagent {

/**
 * An abstract interface for classes interested in updates related to
 * the span artifacts
 */
class SpanManager {
public:
    /**
     * Instantiate a new span manager
     */
    SpanManager(opflex::ofcore::OFFramework& framework_);

    /**
     * Destroy the span manager and clean up all state
     */
    ~SpanManager() {};

    /**
     * Start the span manager
     */
    void start();

    /**
     * Stop the span manager
     */
    void stop();
    /**
     * called when span timer expires. It invokes the update method for span artifacts.
     * @param[in] ec error code.
     */
    void on_timer_span(const boost::system::error_code& ec);

    /**
     * Listener for changes related to span
     */
    class SpanUniverseListener : public opflex::modb::ObjectListener {
    public:
        /**
         * constructor for SpanUniverseListener
         * @param[in] spanmanager reference to span manager
         */
        SpanUniverseListener(SpanManager& spanmanager);
        virtual ~SpanUniverseListener();

        /**
         * callback for handling updates to Span universe
         * @param[in] class_id class id of updated object
         * @param[in] uri of updated object
         */
        virtual void objectUpdated(opflex::modb::class_id_t class_id,
                                   const opflex::modb::URI& uri);
    private:
        SpanManager& spanmanager;
    };

    /**
     * instance of span universe listener class.
     */
    SpanUniverseListener spanUniverseListener;
    friend class SpanUniverseListener;

private:
    opflex::ofcore::OFFramework& framework;
};
}


#endif /* OPFLEXAGENT_SPANMANAGER_H */
