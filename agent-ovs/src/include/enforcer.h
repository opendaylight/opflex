/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef _OPFLEX_ENFORCER_ENFORCER_H_
#define _OPFLEX_ENFORCER_ENFORCER_H_



namespace opflex {
namespace enforcer {


/**
 * @brief Class to enforce traffic flow policies by programming OpenFlow
 * flow-tables.
 * The enforcer listens for updates from MODB, translates them into a set of
 * flow-modifications and then executes them.
 */

class Enforcer {
public:
    virtual ~Enforcer() = 0;

    /**
     * Get the instance of Enforcer object.
     */
    static Enforcer *getInstance();

    /**
     * Module initialization.
     */
    static void Init();

    /**
     * Module start.
     */
    static void Start();

    /**
     * Module start.
     */
    static void Stop();

protected:
    Enforcer();
};

}   // namespace enforcer
}   // namespace opflex

#endif // _OPFLEX_ENFORCER_ENFORCER_H_
