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

#include <boost/test/unit_test.hpp>
#include <modelgbp/dmtree/Root.hpp>

#include <opflexagent/logging.h>
#include <opflexagent/test/BaseFixture.h>
#include "Policies.h"
#include <modelgbp/netflow/CollectorVersionEnumT.hpp>

namespace opflexagent {

using std::shared_ptr;
using namespace modelgbp;

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
        exportCfg->setSamplingRate(20);
        exportCfg->setVersion(CollectorVersionEnumT::CONST_V5);
        exportCfg->setDscp(2);
        mutator.commit();
    }

    virtual ~NetflowFixture() {}

    shared_ptr<platform::Config> config;
    shared_ptr<netflow::ExporterConfig> exportCfg;
};
BOOST_AUTO_TEST_SUITE(NetflowManager_test)

static bool checkNetFlow(boost::optional<shared_ptr<ExporterConfigState>> pExpst,
                         const URI& netflowUri) {
    if (!pExpst)
        return false;
    LOG(DEBUG) << "checkNetFlow" << pExpst.get()->getUri();
    return netflowUri == pExpst.get()->getUri();
}

static bool checkNetFlowTimeout(boost::optional<shared_ptr<ExporterConfigState>> pExpst,
                                shared_ptr<netflow::ExporterConfig>& pExportCfg) {
    if (!pExpst)
        return false;
    LOG(DEBUG) << "checkNetFlowTimeout" << pExpst.get()->getActiveFlowTimeOut();
    return pExpst.get()->getActiveFlowTimeOut() == pExportCfg->getActiveFlowTimeOut();
}
static bool checkNetFlowDstAddress(boost::optional<shared_ptr<ExporterConfigState>> pExpst,
                                   shared_ptr<netflow::ExporterConfig>& pExportCfg) {
    if (!pExpst)
        return false;
    const string addr = pExportCfg->getDstAddr("");
    LOG(DEBUG) << "checkNetFlowDstAddress " << addr;
    return pExpst.get()->getDstAddress() == addr;
}
static bool checkNetFlowDstPort(boost::optional<shared_ptr<ExporterConfigState>> pExpst,
                                shared_ptr<netflow::ExporterConfig>& pExportCfg) {
    if (!pExpst)
        return false;
    LOG(DEBUG) << "checkNetFlowDstPort " << pExpst.get()->getDestinationPort();
    return pExpst.get()->getDestinationPort() == pExportCfg->getDstPort();
}
static bool checkNetFlowVers(boost::optional<shared_ptr<ExporterConfigState>> pExpst,
                             shared_ptr<netflow::ExporterConfig>& pExportCfg) {
    if (!pExpst) return false;
    LOG(DEBUG) << "checkNetFlowVers " << std::to_string(pExpst.get()->getVersion());
    return pExpst.get()->getVersion() == pExportCfg->getVersion().get();
}
BOOST_FIXTURE_TEST_CASE( verify_artifacts, NetflowFixture ) {
    WAIT_FOR(checkNetFlow(agent.getNetFlowManager().getExporterConfigState(exportCfg->getURI()),
                          exportCfg->getURI()), 500);
    WAIT_FOR(checkNetFlowTimeout(agent.getNetFlowManager().getExporterConfigState(exportCfg->getURI()), exportCfg), 500);
    WAIT_FOR(checkNetFlowDstAddress(agent.getNetFlowManager().getExporterConfigState(exportCfg->getURI()), exportCfg), 500);
    WAIT_FOR(checkNetFlowDstPort(agent.getNetFlowManager().getExporterConfigState(exportCfg->getURI()), exportCfg), 500);
    WAIT_FOR(checkNetFlowVers(agent.getNetFlowManager().getExporterConfigState(exportCfg->getURI()), exportCfg), 500);

    // remove exporter
    Mutator mutator(framework, "policyreg");
    exportCfg->remove();
    mutator.commit();

    WAIT_FOR(!agent.getNetFlowManager().anyExporters(), 500)
}

BOOST_AUTO_TEST_SUITE_END()
}
