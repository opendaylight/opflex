/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef _OPFLEX_ENFORCER_INTERNAL_MODEL_H_
#define _OPFLEX_ENFORCER_INTERNAL_MODEL_H_

namespace opflex {
namespace model {

/*
 * TODO TEMPORARY HACK - these should come from headers generated from model definition
 */
enum {
    CLASS_ID_POLICY,
    CLASS_ID_ENDPOINT,
    CLASS_ID_ENDPOINT_GROUP,
    CLASS_ID_CONDITION_GROUP,
    CLASS_ID_BD,
    CLASS_ID_FD,
    CLASS_ID_SUBNET,
    CLASS_ID_L3CTX,
    CLASS_ID_END
};
/* END HACK */

}
}


#endif // _OPFLEX_ENFORCER_INTERNAL_MODEL_H_
