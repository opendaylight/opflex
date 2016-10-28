/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file OFConstants.h
 * @brief Constants definition for the Opflex framework
 */
/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef OPFLEX_CORE_CONSTANTS_H
#define OPFLEX_CORE_CONSTANTS_H

namespace opflex {
namespace ofcore {

/**
 * \addtogroup ofcore
 * @{
 */

/**
 * Define constants that are generally useful for the Opflex framework
 */
class OFConstants {
public:
    /**
     * The set of possible OpFlex roles.  Each elements in an Opflex
     * system has one or more roles.  An opflex client as provided by
     * OFFramework has the role of POLICY_ELEMENT.
     */
    enum OpflexRole {
        /**
         * This is a client connection being used to boostrap the list
         * of peers
         */
        BOOTSTRAP = 0,
        /**
         * The policy element role
         */
        POLICY_ELEMENT = 1,
        /**
         * The policy repository role
         */
        POLICY_REPOSITORY = 2,
        /**
         * The endpoint registry role
         */
        ENDPOINT_REGISTRY = 4,
        /**
         * The observer role
         */
        OBSERVER = 8
    };
};

/** @} ofcore */

} /* namespace ofcore */
} /* namespace opflex */

#endif /* OPFLEX_CORE_CONSTANTS_H */
