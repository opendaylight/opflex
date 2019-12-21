/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Test suite for netflow manager
 *
 * Copyright (c) 2019 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <list>
#include <boost/test/unit_test.hpp>
#include <boost/assign/list_of.hpp>
#include <modelgbp/dmtree/Root.hpp>
#include <opflex/modb/Mutator.h>

#include <opflexagent/logging.h>
#include <opflexagent/test/BaseFixture.h>
#include "Policies.h"
#include <modelgbp/platform/Config.hpp>
#include <modelgbp/netflow/ExporterConfig.hpp>
#include <opflexagent/ExporterConfigState.h>

namespace opflexagent {

using std::shared_ptr;
using namespace modelgbp;
using namespace modelgbp::gbp;
class NetflowFixture : public BaseFixture {

public:
    NetflowFixture() : BaseFixture() {
        shared_ptr<policy::Universe> pUniverse =
            policy::Universe::resolve(framework).get();

        Mutator mutator(framework, "policyreg");
        config = pUniverse->addPlatformConfig("test");
        exportCfg = config->addNetflowExporterConfig("testexporter1");
        exportCfg->setDstAddr("1.2.3.4");
        exportCfg->setDstPort(1234);
        exportCfg->setActiveFlowTimeOut(60);
        mutator.commit();
    }

    virtual ~NetflowFixture() {}

    shared_ptr<policy::Space> space;
    shared_ptr<platform::Config> config;
    shared_ptr<netflow::ExporterConfig> exportCfg;

    boost::optional<shared_ptr<ExporterConfigState>> pExpst;
};
BOOST_AUTO_TEST_SUITE(NetflowManager_test)

static bool checkNetFlow(boost::optional<shared_ptr<ExporterConfigState>> pExpst,
                      const URI& netflowUri) {
    if (!pExpst)
        return false;
    if (netflowUri == pExpst.get()->getUri())
        return true;
    else
        return false;
}

static bool checkNetFlowTimeout(boost::optional<shared_ptr<ExporterConfigState>> pExpst,
                             shared_ptr<netflow::ExporterConfig> pExportCfg) {
    if (pExpst.get()->getActiveFlowTimeOut() == pExportCfg.get()->getActiveFlowTimeOut())
        return true;
    else
        return false;
}
static bool checkNetFlowDstAddress(boost::optional<shared_ptr<ExporterConfigState>> pExpst,
                                shared_ptr<netflow::ExporterConfig> pExportCfg) {
    const string addr = pExportCfg.get()->getDstAddr("");
    if (pExpst.get()->getDstAddress() == addr)
        return true;
    else
        return false;
}
static bool checkNetFlowDstPort(boost::optional<shared_ptr<ExporterConfigState>> pExpst,
                           shared_ptr<netflow::ExporterConfig> pExportCfg) {
    if (pExpst.get()->getDestinationPort() == pExportCfg.get()->getDstPort())
        return true;
    else
        return false;
}
BOOST_FIXTURE_TEST_CASE( verify_artifacts, NetflowFixture ) {
    WAIT_FOR(checkNetFlow(agent.getNetFlowManager().getExporterConfigState(exportCfg->getURI()),
                          exportCfg->getURI()), 500);
    WAIT_FOR(checkNetFlowTimeout(agent.getNetFlowManager().getExporterConfigState(exportCfg->getURI()), exportCfg), 500);
    WAIT_FOR(checkNetFlowDstAddress(agent.getNetFlowManager().getExporterConfigState(exportCfg->getURI()), exportCfg), 500);
    WAIT_FOR(checkNetFlowDstPort(agent.getNetFlowManager().getExporterConfigState(exportCfg->getURI()), exportCfg), 500);
}

BOOST_AUTO_TEST_SUITE_END()
}
