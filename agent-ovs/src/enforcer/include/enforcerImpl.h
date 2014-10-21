/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */


#ifndef _OPFLEX_ENFORCER_ENFORCERIMPL_H_
#define _OPFLEX_ENFORCER_ENFORCERIMPL_H_

#include <vector>
#include <boost/scoped_ptr.hpp>

#include <enforcer.h>
#include <internal/modb.h>
#include <inventory.h>
#include <flowManager.h>


namespace opflex {
namespace enforcer {

/*
 * Implementation of the Enforcer interface.
 */
class EnforcerImpl : public Enforcer,
                     public modb::ObjectListener {
public:
    EnforcerImpl() {
        invtManager.reset(new Inventory(changeInfoOut));
        flowManager.reset(
                new FlowManager(*invtManager, changeInfoOut));
    }
    ~EnforcerImpl() {}

    /**
     * Module start.
     */
    void Start();

    /**
     * Module stop.
     */
    void Stop();

    /* Interface: ObjectListener */

    /**
     * Handle MODB notifications.
     */
    void objectUpdated(modb::class_id_t cid, const modb::URI& uri);

private:
    ChangeList changeInfoOut;
    boost::scoped_ptr<Inventory> invtManager;
    boost::scoped_ptr<FlowManager> flowManager;
};

}   // namespace enforcer
}   // namespace opflex

#endif // _OPFLEX_ENFORCER_ENFORCERIMPL_H_
