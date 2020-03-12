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

namespace opflexagent {

using boost::optional;
using std::shared_ptr;
using std::string;
using namespace modelgbp::gbp;
using namespace modelgbp::gbpe;
using namespace modelgbp::svc;
using namespace opflex::modb;

class ServiceManagerFixture : public ModbFixture {
    typedef opflex::ofcore::OFConstants::OpflexElementMode opflex_elem_t;
public:
    ServiceManagerFixture(opflex_elem_t mode = opflex_elem_t::INVALID_MODE)
        : ModbFixture(mode) {
        createObjects();
        LOG(DEBUG) << "############# SERVICE CREATE START ############";
        createServices();
        LOG(DEBUG) << "############# SERVICE CREATE END ############";
    }

    virtual ~ServiceManagerFixture() {
    }

    void removeServiceObjects(void);
    void updateServices(void);
    void checkServiceState(bool isCreate);
    Service as;
private:
    Service::ServiceMapping sm1;
    Service::ServiceMapping sm2;
    void createServices(void);
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

void ServiceManagerFixture::createServices (void)
{
    as.setUUID("ed84daef-1696-4b98-8c80-6b22d85f4dc2");
    as.setDomainURI(URI(rd0->getURI()));
    as.setServiceMode(Service::LOADBALANCER);

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
    as.addAttribute("namespace", "kube-system");

    servSrc.updateService(as);
}

void ServiceManagerFixture::updateServices (void)
{
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

    servSrc.updateService(as);
}

void ServiceManagerFixture::checkServiceState (bool isCreate)
{
    optional<shared_ptr<ServiceUniverse> > su =
        ServiceUniverse::resolve(agent.getFramework());
    BOOST_CHECK(su);
    optional<shared_ptr<modelgbp::svc::Service> > opService =
                        su.get()->resolveSvcService(as.getUUID());
    BOOST_CHECK(opService);

    // Check if service props are fine
    if (isCreate) {
        uint8_t mode = ServiceModeEnumT::CONST_LB;
        BOOST_CHECK_EQUAL(opService.get()->getMode(255), mode);
    } else {
        // Wait for the update to get reflected
        uint8_t mode = ServiceModeEnumT::CONST_ANYCAST;
        WAIT_FOR_DO_ONFAIL(
           (opService && (opService.get()->getMode(255) == mode)),
           500, // usleep(1000) * 500 = 500ms
           (opService = su.get()->resolveSvcService(as.getUUID())),
           if (opService) {
               BOOST_CHECK_EQUAL(opService.get()->getMode(255), mode);
           });
    }

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
        BOOST_CHECK_EQUAL(outSA.size(), 2);
        BOOST_CHECK_EQUAL(as.getAttributes().size(), 2);
    } else {
        // remove + add can lead to bulking, so wait till the state becomes fine
        WAIT_FOR_DO_ONFAIL((outSA.size() == 1),
            500, // usleep(1000) * 500 = 500ms
            outSA.clear();pService->resolveSvcServiceAttribute(outSA),
            BOOST_CHECK_EQUAL(outSA.size(), 1););
        BOOST_CHECK_EQUAL(as.getAttributes().size(), 1);
    }

}

BOOST_AUTO_TEST_SUITE(ServiceManager_test)

BOOST_FIXTURE_TEST_CASE(testCreate, ServiceManagerFixture) {
    LOG(DEBUG) << "############# SERVICE CREATE CHECK START ############";
    optional<shared_ptr<ServiceUniverse> > su =
        ServiceUniverse::resolve(agent.getFramework());
    BOOST_CHECK(su);
    WAIT_FOR_DO_ONFAIL(su.get()->resolveSvcService(as.getUUID()),
                        500,,
                        LOG(ERROR) << "Service Obj not resolved";);
    optional<shared_ptr<modelgbp::svc::Service> > opService =
                        su.get()->resolveSvcService(as.getUUID());
    checkServiceState(true);
    LOG(DEBUG) << "############# SERVICE CREATE CHECK END ############";
}

BOOST_FIXTURE_TEST_CASE(testUpdate, ServiceManagerFixture) {
    LOG(DEBUG) << "############# SERVICE UPDATE START ############";
    optional<shared_ptr<ServiceUniverse> > su =
        ServiceUniverse::resolve(agent.getFramework());
    BOOST_CHECK(su);
    WAIT_FOR_DO_ONFAIL(su.get()->resolveSvcService(as.getUUID()),
                        500,,
                        LOG(ERROR) << "Service Obj not resolved";);
    optional<shared_ptr<modelgbp::svc::Service> > opService =
                        su.get()->resolveSvcService(as.getUUID());
    shared_ptr<modelgbp::svc::Service> pService = opService.get();
    updateServices();
    checkServiceState(false);
    LOG(DEBUG) << "############# SERVICE UPDATE END ############";
}

BOOST_FIXTURE_TEST_CASE(testDelete, ServiceManagerFixture) {
    LOG(DEBUG) << "############# SERVICE DELETE START ############";
    optional<shared_ptr<ServiceUniverse> > su =
        ServiceUniverse::resolve(agent.getFramework());
    BOOST_CHECK(su);
    WAIT_FOR_DO_ONFAIL(su.get()->resolveSvcService(as.getUUID()),
                        500,,
                        LOG(ERROR) << "Service Obj not resolved";);
    removeServiceObjects();
    WAIT_FOR_DO_ONFAIL(!su.get()->resolveSvcService(as.getUUID()),
                        500,,
                        LOG(ERROR) << "Service Obj still present";);
    LOG(DEBUG) << "############# SERVICE DELETE END ############";
}

BOOST_AUTO_TEST_SUITE_END()
}
