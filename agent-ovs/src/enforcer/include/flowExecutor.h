/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef _OPFLEX_ENFORCER_FLOWEXECUTOR_H_
#define _OPFLEX_ENFORCER_FLOWEXECUTOR_H_

#include <flow/tableState.h>

namespace opflex {
namespace enforcer {

/**
 * @brief Abstract base class that can execute a set of OpenFlow
 * table modifications.
 */
class FlowExecutor {
public:
    virtual bool Execute(flow::TableState::TABLE_TYPE t,
            const flow::FlowEdit& fe) = 0;
};

}   // namespace enforcer
}   // namespace opflex


#endif // _OPFLEX_ENFORCER_FLOWEXECUTOR_H_
