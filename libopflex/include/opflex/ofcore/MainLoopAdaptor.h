/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file MainLoopAdaptor.h
 * @brief Interface definition for main loop adaptor
 */
/*
 * Copyright (c) 2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef OPFLEX_CORE_MAINLOOPADAPTOR_H
#define OPFLEX_CORE_MAINLOOPADAPTOR_H

namespace opflex {
namespace ofcore {

/**
 * \addtogroup ofcore
 * @{
 */

/**
 * An adaptor that allows integrating libopflex with an external main
 * loop.  If using a main loop adaptor, libopflex will not create any
 * threads of its own.
 */
class MainLoopAdaptor {
public:
    /**
     * Run one iteration of the main loop, and return when complete.
     * Call this for each iteration of your main loop after calling
     * OFFramework::start.  This function will not block.
     */
    virtual void runOnce() = 0;

    /**
     * Get the backend file descriptor.  This can be used to poll on
     * events and call runOnce in response to the events.
     *
     * @return a file descriptor for polling
     */
    virtual int getBackendFd() = 0;

    /**
     * Get the poll timeout for the backend file descriptor
     *
     * @return the timeout value in milliseconds, or -1 if there is no
     * timeout.
     */
    virtual int getBackendTimeout() = 0;
};

/** @} ofcore */

} /* namespace ofcore */
} /* namespace opflex */

#endif /* OPFLEX_CORE_MAINLOOPADAPTOR_H */
