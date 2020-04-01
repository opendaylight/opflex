/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Test suite for service manager
 *
 * Copyright (c) 2020 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <boost/test/unit_test.hpp>
#include <opflex/modb/Mutator.h>
#include <opflexagent/test/ModbFixture.h>
#include <opflexagent/test/BaseFixture.h>
#include <opflexagent/logging.h>
#include <modelgbp/svc/ServiceUniverse.hpp>
#include <modelgbp/svc/ServiceModeEnumT.hpp>
#include <modelgbp/observer/SvcStatUniverse.hpp>
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#ifdef HAVE_PROMETHEUS_SUPPORT
#include <opflexagent/PrometheusManager.h>
#endif

namespace opflexagent {

using boost::optional;
using std::shared_ptr;
using std::string;
using std::size_t;
using namespace modelgbp::gbp;
using namespace modelgbp::gbpe;
using namespace modelgbp::svc;
using namespace opflex::modb;
using modelgbp::observer::SvcStatUniverse;

class ServiceManagerFixture : public ModbFixture {
    typedef opflex::ofcore::OFConstants::OpflexElementMode opflex_elem_t;
public:
    ServiceManagerFixture(opflex_elem_t mode = opflex_elem_t::INVALID_MODE)
        : ModbFixture(mode) {
        createObjects();
    }

    virtual ~ServiceManagerFixture() {
    }

    void removeServiceObjects(void);
    void updateServices(bool isLB);
    void checkServiceState(bool isCreate);
    void checkServiceExists(bool isCreate);
    void createServices(bool isLB);
    Service as;
    /**
     * Following commented commands didnt work in test environment, but will work from a regular terminal:
     * const string cmd = "curl --no-proxy \"*\" --compressed --silent http://127.0.0.1:9612/metrics 2>&1";
     * const string cmd = "curl --no-proxy \"127.0.0.1\" http://127.0.0.1:9612/metrics;";
     * --no-proxy, even though specified somehow tries to connect to SOCKS proxy and connection
     * fails to port 1080 :(
     */
    const string cmd = "curl --proxy \"\" --compressed --silent http://127.0.0.1:9612/metrics 2>&1;";
private:
    Service::ServiceMapping sm1;
    Service::ServiceMapping sm2;
};

/*
 * Service delete
 * - delete svc objects
 * - check if observer objects go away from caller
 */
void ServiceManagerFixture::removeServiceObjects (void)
{
    servSrc.removeService(as.getUUID());
}

void ServiceManagerFixture::createServices (bool isLB)
{
    as.setUUID("ed84daef-1696-4b98-8c80-6b22d85f4dc2");
    as.setDomainURI(URI(rd0->getURI()));
    if (isLB)
        as.setServiceMode(Service::LOADBALANCER);
    else
        as.setServiceMode(Service::LOCAL_ANYCAST);

    sm1.setServiceIP("169.254.169.254");
    sm1.setServiceProto("udp");
    sm1.setServicePort(53);
    sm1.addNextHopIP("169.254.169.1");
    sm1.addNextHopIP("169.254.169.2");
    sm1.setNextHopPort(5353);
    as.addServiceMapping(sm1);

    sm2.setServiceIP("fe80::a9:fe:a9:fe");
    sm2.setServiceProto("tcp");
    sm2.setServicePort(80);
    sm2.addNextHopIP("fe80::a9:fe:a9:1");
    sm2.addNextHopIP("fe80::a9:fe:a9:2");
    as.addServiceMapping(sm2);

    as.addAttribute("name", "coredns");
    as.addAttribute("scope", "cluster");
    as.addAttribute("namespace", "kube-system");

    servSrc.updateService(as);
}

void ServiceManagerFixture::updateServices (bool isLB)
{
    if (isLB)
        as.setServiceMode(Service::LOADBALANCER);
    else
        as.setServiceMode(Service::LOCAL_ANYCAST);
    // simulating an update. without clearing the service mappings,
    // service mapping count will increase, since each new SM will
    // have a new service hash before getting added to sm_set
    as.clearServiceMappings();

    sm1.setServiceIP("169.254.169.254");
    sm1.setServiceProto("udp");
    sm1.setServicePort(54);
    sm1.addNextHopIP("169.254.169.3");
    sm1.addNextHopIP("169.254.169.4");
    sm1.setNextHopPort(5354);
    as.addServiceMapping(sm1);

    sm2.setServiceIP("fe80::a9:fe:a9:fe");
    sm2.setServiceProto("tcp");
    sm2.setServicePort(81);
    sm2.addNextHopIP("fe80::a9:fe:a9:3");
    sm2.addNextHopIP("fe80::a9:fe:a9:4");
    as.addServiceMapping(sm2);

    as.clearAttributes();
    as.addAttribute("name", "core-dns");
    as.addAttribute("scope", "cluster");

    servSrc.updateService(as);
}

// Check service state during create vs update
void ServiceManagerFixture::checkServiceState (bool isCreate)
{
    optional<shared_ptr<ServiceUniverse> > su =
        ServiceUniverse::resolve(agent.getFramework());
    BOOST_CHECK(su);
    optional<shared_ptr<modelgbp::svc::Service> > opService =
                        su.get()->resolveSvcService(as.getUUID());
    BOOST_CHECK(opService);

    // Check if service props are fine
    uint8_t mode = ServiceModeEnumT::CONST_ANYCAST;
    if (Service::LOADBALANCER == as.getServiceMode()) {
        mode = ServiceModeEnumT::CONST_LB;
    }
    // Wait for the create/update to get reflected
    WAIT_FOR_DO_ONFAIL(
       (opService && (opService.get()->getMode(255) == mode)),
       500, // usleep(1000) * 500 = 500ms
       (opService = su.get()->resolveSvcService(as.getUUID())),
       if (opService) {
           BOOST_CHECK_EQUAL(opService.get()->getMode(255), mode);
       });

    shared_ptr<modelgbp::svc::Service> pService = opService.get();
    BOOST_CHECK_EQUAL(pService->getDom(""), rd0->getURI().toString());

    // Check if SM objects are created fine
    WAIT_FOR_DO_ONFAIL(pService->resolveSvcServiceMapping(sm1.getServiceIP().get(),
                                                          sm1.getServiceProto().get(),
                                                          sm1.getServicePort().get()),
                        500,,
                        LOG(ERROR) << "ServiceMapping Obj1 not resolved";);
    WAIT_FOR_DO_ONFAIL(pService->resolveSvcServiceMapping(sm2.getServiceIP().get(),
                                                          sm2.getServiceProto().get(),
                                                          sm2.getServicePort().get()),
                        500,,
                        LOG(ERROR) << "ServiceMapping Obj2 not resolved";);

    std::vector<shared_ptr<ServiceMapping> > outSM;
    pService->resolveSvcServiceMapping(outSM);
    if (isCreate) {
        BOOST_CHECK_EQUAL(outSM.size(), 2);
    } else {
        // remove + add can lead to bulking, so wait till the state becomes fine
        WAIT_FOR_DO_ONFAIL((outSM.size() == 2),
            500, // usleep(1000) * 500 = 500ms
            outSM.clear();pService->resolveSvcServiceMapping(outSM),
            BOOST_CHECK_EQUAL(outSM.size(), 2););
    }
    BOOST_CHECK_EQUAL(as.getServiceMappings().size(), 2);

    // Check if attr objects are resolved fine
    const Service::attr_map_t& attr_map = as.getAttributes();
    for (const std::pair<const string, string>& ap : attr_map) {
        WAIT_FOR_DO_ONFAIL(pService->resolveSvcServiceAttribute(ap.first),
                           500,,
                           LOG(ERROR) << "ServiceAttribute Obj not resolved";);
        auto sa = pService->resolveSvcServiceAttribute(ap.first);
        BOOST_CHECK(sa);
        BOOST_CHECK_EQUAL(sa.get()->getValue(""), ap.second);
    }

    std::vector<shared_ptr<ServiceAttribute> > outSA;
    pService->resolveSvcServiceAttribute(outSA);
    if (isCreate) {
        BOOST_CHECK_EQUAL(outSA.size(), 3);
        BOOST_CHECK_EQUAL(as.getAttributes().size(), 3);
    } else {
        // remove + add can lead to bulking, so wait till the state becomes fine
        WAIT_FOR_DO_ONFAIL((outSA.size() == 2),
            500, // usleep(1000) * 500 = 500ms
            outSA.clear();pService->resolveSvcServiceAttribute(outSA),
            BOOST_CHECK_EQUAL(outSA.size(), 2););
        BOOST_CHECK_EQUAL(as.getAttributes().size(), 2);
    }

    optional<shared_ptr<SvcStatUniverse> > ssu =
                          SvcStatUniverse::resolve(framework);
    BOOST_CHECK(ssu);
    // Check observer stats object for LB services only
    if (Service::LOADBALANCER == as.getServiceMode()) {
        optional<shared_ptr<SvcCounter> > opServiceCntr =
                        ssu.get()->resolveGbpeSvcCounter(as.getUUID());
        BOOST_CHECK(opServiceCntr);
        shared_ptr<SvcCounter> pServiceCntr = opServiceCntr.get();

        const Service::attr_map_t& svcAttr = as.getAttributes();

        auto nameItr = svcAttr.find("name");
        if (nameItr != svcAttr.end()) {
            BOOST_CHECK_EQUAL(pServiceCntr->getName(""),
                              nameItr->second);
        }

        auto nsItr = svcAttr.find("namespace");
        if (nsItr != svcAttr.end()) {
            BOOST_CHECK_EQUAL(pServiceCntr->getNamespace(""),
                              nsItr->second);
        }

        auto scopeItr = svcAttr.find("scope");
        if (scopeItr != svcAttr.end()) {
            BOOST_CHECK_EQUAL(pServiceCntr->getScope(""),
                              scopeItr->second);
        }
    } else {
        WAIT_FOR_DO_ONFAIL(!(ssu.get()->resolveGbpeSvcCounter(as.getUUID())),
                           500,,
                           LOG(ERROR) << "svc obj still present uuid: " << as.getUUID(););
    }
}

// Check service presence during create vs delete
void ServiceManagerFixture::checkServiceExists (bool isCreate)
{
    optional<shared_ptr<ServiceUniverse> > su =
        ServiceUniverse::resolve(agent.getFramework());
    BOOST_CHECK(su);

    optional<shared_ptr<SvcStatUniverse> > ssu =
                          SvcStatUniverse::resolve(framework);
    if (isCreate) {
        WAIT_FOR_DO_ONFAIL(su.get()->resolveSvcService(as.getUUID()),
                           500,,
                           LOG(ERROR) << "Service cfg Obj not resolved";);
        if (Service::LOADBALANCER == as.getServiceMode()) {
            WAIT_FOR_DO_ONFAIL(ssu.get()->resolveGbpeSvcCounter(as.getUUID()),
                               500,,
                               LOG(ERROR) << "SvcCounter obs Obj not resolved";);
            optional<shared_ptr<SvcCounter> > opSvcCounter =
                                ssu.get()->resolveGbpeSvcCounter(as.getUUID());
            BOOST_CHECK(opSvcCounter);
            for (const auto& sm : as.getServiceMappings()) {
                for (const auto& ip : sm.getNextHopIPs()) {
                    WAIT_FOR_DO_ONFAIL(opSvcCounter.get()->resolveGbpeSvcTargetCounter(ip),
                                       500,,
                                       LOG(ERROR) << "SvcTargetCounter obs Obj not resolved";);
                }
            }
        } else {
            BOOST_CHECK(!ssu.get()->resolveGbpeSvcCounter(as.getUUID()));
        }
    } else {
        WAIT_FOR_DO_ONFAIL(!su.get()->resolveSvcService(as.getUUID()),
                           500,,
                           LOG(ERROR) << "Service cfg Obj still present";);
        WAIT_FOR_DO_ONFAIL(!ssu.get()->resolveGbpeSvcCounter(as.getUUID()),
                           500,,
                           LOG(ERROR) << "Service obs Obj still present";);
    }
}

BOOST_AUTO_TEST_SUITE(ServiceManager_test)

BOOST_FIXTURE_TEST_CASE(testCreateAnycast, ServiceManagerFixture) {
    LOG(DEBUG) << "############# SERVICE CREATE CHECK START ############";
    LOG(DEBUG) << "#### SERVICE CREATE START ####";
    createServices(false);
    LOG(DEBUG) << "#### SERVICE CREATE END ####";
    checkServiceExists(true);
    checkServiceState(true);
#ifdef HAVE_PROMETHEUS_SUPPORT
    const string& output = BaseFixture::getOutputFromCommand(cmd);
    size_t pos = std::string::npos;
    pos = output.find("opflex_svc_active_total 0.000000");
    BOOST_CHECK_NE(pos, std::string::npos);
    pos = output.find("opflex_svc_created_total 0.000000");
    BOOST_CHECK_NE(pos, std::string::npos);
    pos = output.find("opflex_svc_removed_total 0.000000");
    BOOST_CHECK_NE(pos, std::string::npos);
#endif
    LOG(DEBUG) << "############# SERVICE CREATE CHECK END ############";
}

BOOST_FIXTURE_TEST_CASE(testCreateLB, ServiceManagerFixture) {
    LOG(DEBUG) << "############# SERVICE CREATE CHECK START ############";
    LOG(DEBUG) << "#### SERVICE CREATE START ####";
    createServices(true);
    LOG(DEBUG) << "#### SERVICE CREATE END ####";
    checkServiceExists(true);
    checkServiceState(true);
#ifdef HAVE_PROMETHEUS_SUPPORT
    const string& output = BaseFixture::getOutputFromCommand(cmd);
    size_t pos = std::string::npos;
    pos = output.find("opflex_svc_active_total 1.000000");
    BOOST_CHECK_NE(pos, std::string::npos);
    pos = output.find("opflex_svc_created_total 1.000000");
    BOOST_CHECK_NE(pos, std::string::npos);
    pos = output.find("opflex_svc_removed_total 0.000000");
    BOOST_CHECK_NE(pos, std::string::npos);
#endif
    LOG(DEBUG) << "############# SERVICE CREATE CHECK END ############";
}

BOOST_FIXTURE_TEST_CASE(testUpdate, ServiceManagerFixture) {
    LOG(DEBUG) << "#### SERVICE CREATE START ####";
    createServices(true);
    LOG(DEBUG) << "#### SERVICE CREATE END ####";
    LOG(DEBUG) << "############# SERVICE UPDATE START ############";
    checkServiceExists(true);

    // update to anycast
    updateServices(false);
    checkServiceState(false);
#ifdef HAVE_PROMETHEUS_SUPPORT
    const string& output1 = BaseFixture::getOutputFromCommand(cmd);
    size_t pos = std::string::npos;
    pos = output1.find("opflex_svc_active_total 0.000000");
    BOOST_CHECK_NE(pos, std::string::npos);
    pos = output1.find("opflex_svc_created_total 1.000000");
    BOOST_CHECK_NE(pos, std::string::npos);
    pos = output1.find("opflex_svc_removed_total 1.000000");
    BOOST_CHECK_NE(pos, std::string::npos);
#endif

    // update to lb
    updateServices(true);
    checkServiceState(false);
#ifdef HAVE_PROMETHEUS_SUPPORT
    const string& output2 = BaseFixture::getOutputFromCommand(cmd);
    pos = std::string::npos;
    pos = output2.find("opflex_svc_active_total 1.000000");
    BOOST_CHECK_NE(pos, std::string::npos);
    pos = output2.find("opflex_svc_created_total 2.000000");
    BOOST_CHECK_NE(pos, std::string::npos);
    pos = output2.find("opflex_svc_removed_total 1.000000");
    BOOST_CHECK_NE(pos, std::string::npos);
#endif
    LOG(DEBUG) << "############# SERVICE UPDATE END ############";
}

BOOST_FIXTURE_TEST_CASE(testDeleteLB, ServiceManagerFixture) {
    LOG(DEBUG) << "#### SERVICE CREATE START ####";
    createServices(true);
    LOG(DEBUG) << "#### SERVICE CREATE END ####";
    LOG(DEBUG) << "############# SERVICE DELETE START ############";
    checkServiceExists(true);
    removeServiceObjects();
    checkServiceExists(false);
#ifdef HAVE_PROMETHEUS_SUPPORT
    const string& output1 = BaseFixture::getOutputFromCommand(cmd);
    size_t pos = std::string::npos;
    pos = output1.find("opflex_svc_active_total 0.000000");
    BOOST_CHECK_NE(pos, std::string::npos);
    pos = output1.find("opflex_svc_created_total 1.000000");
    BOOST_CHECK_NE(pos, std::string::npos);
    pos = output1.find("opflex_svc_removed_total 1.000000");
    BOOST_CHECK_NE(pos, std::string::npos);
#endif

    createServices(true);
    checkServiceExists(true);
#ifdef HAVE_PROMETHEUS_SUPPORT
    const string& output2 = BaseFixture::getOutputFromCommand(cmd);
    pos = output2.find("opflex_svc_active_total 1.000000");
    BOOST_CHECK_NE(pos, std::string::npos);
    pos = output2.find("opflex_svc_created_total 2.000000");
    BOOST_CHECK_NE(pos, std::string::npos);
    pos = output2.find("opflex_svc_removed_total 1.000000");
    BOOST_CHECK_NE(pos, std::string::npos);
#endif

    removeServiceObjects();
    checkServiceExists(false);
#ifdef HAVE_PROMETHEUS_SUPPORT
    const string& output3 = BaseFixture::getOutputFromCommand(cmd);
    pos = output3.find("opflex_svc_active_total 0.000000");
    BOOST_CHECK_NE(pos, std::string::npos);
    pos = output3.find("opflex_svc_created_total 2.000000");
    BOOST_CHECK_NE(pos, std::string::npos);
    pos = output3.find("opflex_svc_removed_total 2.000000");
    BOOST_CHECK_NE(pos, std::string::npos);
#endif
    LOG(DEBUG) << "############# SERVICE DELETE END ############";
}

BOOST_FIXTURE_TEST_CASE(testDeleteAnycast, ServiceManagerFixture) {
    LOG(DEBUG) << "#### SERVICE CREATE START ####";
    createServices(false);
    LOG(DEBUG) << "#### SERVICE CREATE END ####";
    LOG(DEBUG) << "############# SERVICE DELETE START ############";
    checkServiceExists(true);
    removeServiceObjects();
    checkServiceExists(false);
#ifdef HAVE_PROMETHEUS_SUPPORT
    const string& output1 = BaseFixture::getOutputFromCommand(cmd);
    size_t pos = std::string::npos;
    pos = output1.find("opflex_svc_active_total 0.000000");
    BOOST_CHECK_NE(pos, std::string::npos);
    pos = output1.find("opflex_svc_created_total 0.000000");
    BOOST_CHECK_NE(pos, std::string::npos);
    pos = output1.find("opflex_svc_removed_total 0.000000");
    BOOST_CHECK_NE(pos, std::string::npos);
#endif
    LOG(DEBUG) << "############# SERVICE DELETE END ############";
}

BOOST_AUTO_TEST_SUITE_END()
}
