/*
 * Test suite for class Inventory.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <vector>
#include <boost/test/unit_test.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/foreach.hpp>
#include <boost/assign/list_of.hpp>

#include <internal/modb.h>
#include <internal/model.h>
#include <inventory.h>

#include <test/modbFixture.h>

using namespace std;
using namespace opflex::enforcer;
using namespace opflex::modb;
using namespace opflex::model;
using namespace boost::assign;

class InventoryFixture : public ModbFixture {
public:
    InventoryFixture() {
        invt.reset(new Inventory(cl));
        invt->Start();
        for (int i = 0; i < initObjs.size(); ++i) {
            DoUpdate(initObjs[i]);
        }
        cl.clear();
    }
    ~InventoryFixture() {
        invt->Stop();
    }
    void DoAdd(MoBasePtr mo) {
        opflex::modb::Add(mo);
        DoUpdate(mo);
    }
    void DoRemove(const URI& uri) {
        MoBasePtr mo = invt->Find(uri);
        opflex::modb::Remove(uri);
        DoUpdate(mo);
    }
    void DoUpdate(MoBasePtr mo) {
        invt->Update(mo->GetClass(), mo->GetUri());
    }
    void DoUpdate(const URI& uri) {
        MoBasePtr mo = invt->Find(uri);
        invt->Update(mo->GetClass(), mo->GetUri());
    }
    void ExpectChangeList(const std::vector<URI>& clIn) {
        BOOST_CHECK_MESSAGE(cl.size() == clIn.size(),
                "expected " << clIn.size() << ", got " << cl.size());

        std::vector<URI>::const_iterator itr = clIn.begin();
        BOOST_FOREACH(ChangeInfo& ci, cl) {
            BOOST_CHECK_MESSAGE(ci.GetUri() == *itr,
                    "expected " << itr->toString().c_str() << ", got "
                         << ci.GetUri().toString().c_str());
            ++itr;
            if (itr == clIn.end()) {
                break;
            }
        }
        cl.clear();
    }

    ChangeList cl;
    boost::scoped_ptr<Inventory> invt;
    MoBasePtr nullObj;
};

BOOST_AUTO_TEST_SUITE(inventory_test)

BOOST_FIXTURE_TEST_CASE(lookup, InventoryFixture) {
    URI uri1("/t0/ep/0");
    URI uri2("/t0/ep/100");

    // Lookup existing object
    BOOST_CHECK(invt->Find(uri1) != nullObj);

    // Lookup non-existing object
    invt->Update(CLASS_ID_ENDPOINT, uri2);
    BOOST_CHECK(invt->Find(uri2) == nullObj);

    // Add object, then lookup
    EpBuilder epb;
    DoAdd(epb.reset(uri2.toString()).setMac("100").build());
    MoBasePtr mo = invt->Find(uri2);
    BOOST_CHECK(mo != nullObj);

    // Modify object, then lookup
    mo->Set("ipAddr", "1");
    BOOST_CHECK(invt->Find(uri2) != nullObj);

    // Remove object then lookup
    DoRemove(uri2);
    BOOST_CHECK(invt->Find(uri2) == nullObj);
}

BOOST_FIXTURE_TEST_CASE(lookupNet, InventoryFixture) {

    URI uri1("/t0/epg/0");
    URI uri1net = URI(invt->Find(uri1)->Get("networkDomainUri"));

    URI uri2("/t0/net/subnet/0");
    URI uri3("/t0/net/bd/0");
    URI uri4("/t0/net/l3/0");

    MoBasePtr sn = invt->FindNetObj(uri1net, CLASS_ID_SUBNET);
    BOOST_CHECK(sn != nullObj);
    BOOST_CHECK(sn->GetUri() == uri2);

    MoBasePtr bd = invt->FindNetObj(uri1net, CLASS_ID_BD);
    BOOST_CHECK(bd != nullObj);
    BOOST_CHECK(bd->GetUri() == uri3);

    MoBasePtr l3 = invt->FindNetObj(uri1net, CLASS_ID_L3CTX);
    BOOST_CHECK(l3 != nullObj);
    BOOST_CHECK(l3->GetUri() == uri4);
}

BOOST_FIXTURE_TEST_CASE(backlink_net, InventoryFixture) {
    URI uri1("/t0/ep/1");
    URI uri2("/t0/epg/100");
    URI uri3("/t0/net/fd/100");
    URI uri4("/t0/net/bd/100");
    URI uri5("/t0/net/l3/100");

    MoBasePtr ep = invt->Find(uri1);
    ep->Set("epgUri", uri2.toString());
    DoAdd(ep);
    ExpectChangeList(list_of(uri1));

    EpgBuilder epgb;
    DoAdd(epgb.reset(uri2.toString()).setId("100").setNetwork(uri3.toString()).build());
    ExpectChangeList(list_of(uri2)(uri1));

    FdBuilder fdb;
    DoAdd(fdb.reset(uri3.toString()).setParent(uri4.toString()).build());
    ExpectChangeList(list_of(uri3)(uri2)(uri1));

    BdBuilder bdb;
    DoAdd(bdb.reset(uri4.toString()).setParent(uri5.toString()).build());
    ExpectChangeList(list_of(uri4)(uri3)(uri2)(uri1));

    L3Builder l3b;
    DoAdd(l3b.reset(uri5.toString()).build());
    ExpectChangeList(list_of(uri5)(uri4)(uri3)(uri2)(uri1));
}

BOOST_FIXTURE_TEST_CASE(backlink_multi, InventoryFixture) {
    URI uri1("/t0/ep/0");
    URI uri2("/t0/ep/2");
    URI uri3("/t0/ep/100");
    URI uri4("/t0/epg/0");
    URI uri5("/t0/policy/0");

    EpBuilder epb;
    DoAdd(epb.reset(uri3.toString()).setMac("100").setGroup(uri4.toString()).build());

    DoUpdate(uri4);
    ExpectChangeList(list_of(uri3)(uri4)(uri1)(uri3)(uri2)(uri5));

    DoRemove(uri1);
    DoRemove(uri2);
    DoRemove(uri3);
    DoRemove(uri5);
    DoUpdate(uri4);
    ExpectChangeList(list_of(uri1)(uri2)(uri3)(uri5)(uri4));
}

BOOST_FIXTURE_TEST_CASE(backlink_policy, InventoryFixture) {
    URI uri1("/t0/policy/0");
    URI uri2("/t0/epg/0");
    URI uri3("/t0/epg/110");
    URI uri4("/t0/ep/0");
    URI uri5("/t0/ep/2");

    EpgBuilder epgb;
    DoAdd(epgb.reset(uri3.toString()).setId("110").build());

    ExpectChangeList(list_of(uri3)(uri1));

    DoUpdate(uri2);
    ExpectChangeList(list_of(uri2)(uri4)(uri5)(uri1));
}

BOOST_AUTO_TEST_SUITE_END()


