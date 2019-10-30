/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file Policy.h
 * @brief Interface definition file for GBP policy
 */
/*
 * Copyright (c) 2019 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef GBP_POLICY_H
#define GBP_POLICY_H

namespace opflex {
namespace gbp {

enum class PolicyUpdateOp {
    /**
     * add MO and merge children
     */
    ADD,
    /**
     * replace MO and its children
     */
    REPLACE,
    /**
     * delete MO alone
     */
    DELETE,
    /**
     * recursively delete MO and its children
     */
    DELETE_RECURSIVE,
};

} /* namespace gbp */
} /* namespace opflex */
#endif /* GBP_POLICY_H */
