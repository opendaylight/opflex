/*
 * Test suite for class FlowManager
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <boost/test/unit_test.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/foreach.hpp>
#include <boost/assign/list_of.hpp>

#include <internal/modb.h>
#include <internal/model.h>
#include <flowManager.h>
#include <flowExecutor.h>

#include <test/modbFixture.h>
extern "C" {
#include <ovs/lib/dynamic-string.h>
#include <ovs/lib/ofp-print.h>
}

using namespace std;
using namespace opflex::enforcer;
using namespace opflex::enforcer::flow;
using namespace opflex::modb;
using namespace opflex::model;

class TestFlowExecutor : public FlowExecutor {
public:
    TestFlowExecutor() {}
    ~TestFlowExecutor() {}

    bool Execute(TableState::TABLE_TYPE t, const FlowEdit& flowEdits) {
        const char *modStr[] = {"add", "mod", "del"};
        cout << "TestFlowExecutor: doing " << flowEdits.edits.size() << " changes" << endl;
        BOOST_FOREACH(const FlowEdit::EntryEdit& ed, flowEdits.edits) {
            const FlowEntry *fe = dynamic_cast<const FlowEntry*>(ed.second);
            struct ds strBuf;
            ds_init(&strBuf);
            ofp_print_flow_stats(&strBuf, fe->entry);
            cout << modStr[ed.first] << " " << ds_cstr(&strBuf) << endl;
            ds_destroy(&strBuf);
        }

        return true;
    }
};

class RendererFixture : public ModbFixture {
public:
    RendererFixture() {
        invt.reset(new Inventory(cl));
        renderer.reset(new FlowManager(*invt.get(), cl));
        executor = new TestFlowExecutor();
        renderer->SetExecutor(executor);

        renderer->Start();
        invt->Start();
        for (int i = 0; i < initObjs.size(); ++i) {
            DoUpdate(initObjs[i]);
        }
        cl.clear();
    }
    ~RendererFixture() {
        invt->Stop();
        renderer->Stop();
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

    ChangeList cl;
    boost::scoped_ptr<Inventory> invt;
    boost::scoped_ptr<FlowManager> renderer;
    FlowExecutor *executor;
    MoBasePtr nullObj;
};

BOOST_AUTO_TEST_SUITE(renderer_test)

BOOST_FIXTURE_TEST_CASE(epg, RendererFixture) {
    URI uri1("/t0/epg/0");
    URI uri2("/t0/net/l3/0");

    cl.push_back(ChangeInfo(0, ChangeInfo::ADD, invt->Find(uri1), nullObj));
    renderer->Generate();
    // no change
    cl.push_back(ChangeInfo(0, ChangeInfo::MOD, invt->Find(uri1), nullObj));
    renderer->Generate();
    // network-domain change
    MoBasePtr epg = invt->Find(uri1);
    epg->Set("networkDomainUri", uri2.toString());
    opflex::modb::Add(epg);
    cl.push_back(ChangeInfo(0, ChangeInfo::MOD, invt->Find(uri1), nullObj));
    renderer->Generate();
    // remove
    cl.push_back(ChangeInfo(0, ChangeInfo::REMOVE, nullObj, invt->Find(uri1)));
    renderer->Generate();
}

BOOST_FIXTURE_TEST_CASE(localEp, RendererFixture) {
    URI uri1("/t0/ep/0");
    URI uri2("/t0/epg/1");

    cl.push_back(ChangeInfo(0, ChangeInfo::ADD, invt->Find(uri1), nullObj));
    renderer->Generate();
    // endpoint group change
    MoBasePtr epg = invt->Find(uri1);
    const string& oldEpg = epg->Get("epgUri");
    epg->Set("epgUri", uri2.toString());
    opflex::modb::Add(epg);
    cl.push_back(ChangeInfo(0, ChangeInfo::MOD, invt->Find(uri1), nullObj));
    renderer->Generate();
    // OFport change + endpoint group change back to old
    epg->Set("ofport", "20");
    epg->Set("epgUri", oldEpg);
    opflex::modb::Add(epg);
    cl.push_back(ChangeInfo(0, ChangeInfo::MOD, invt->Find(uri1), nullObj));
    renderer->Generate();
}

BOOST_FIXTURE_TEST_CASE(remoteEp, RendererFixture) {
    URI uri1("/t0/ep/2");
    URI uri2("/t0/epg/1");

    cl.push_back(ChangeInfo(0, ChangeInfo::ADD, invt->Find(uri1), nullObj));
    renderer->Generate();

    // endpoint group change
    MoBasePtr epg = invt->Find(uri1);
    const string& oldEpg = epg->Get("epgUri");
    epg->Set("epgUri", uri2.toString());
    opflex::modb::Add(epg);
    cl.push_back(ChangeInfo(0, ChangeInfo::MOD, invt->Find(uri1), nullObj));
    renderer->Generate();

    // endpoint group change back to old
    epg->Set("epgUri", oldEpg);
    opflex::modb::Add(epg);
    cl.push_back(ChangeInfo(0, ChangeInfo::MOD, invt->Find(uri1), nullObj));
    renderer->Generate();
}

BOOST_FIXTURE_TEST_CASE(subnetArp, RendererFixture) {
    URI uri1("/t0/net/subnet/0");

    cl.push_back(ChangeInfo(0, ChangeInfo::ADD, invt->Find(uri1), nullObj));
    renderer->Generate();
}

BOOST_FIXTURE_TEST_CASE(policy, RendererFixture) {
    URI uri1("/t0/policy/0");
    URI uri2("/t0/epg/110");

    EpgBuilder epgb;
    DoAdd(epgb.reset(uri2.toString()).setId("110").build());
    cl.clear();

    cl.push_back(ChangeInfo(0, ChangeInfo::ADD, invt->Find(uri1), nullObj));
    renderer->Generate();

    MoBasePtr plcy = invt->Find(uri1);
    plcy->Set("actions", "drop");
    opflex::modb::Add(plcy);
    cl.push_back(ChangeInfo(0, ChangeInfo::MOD, plcy, nullObj));
    renderer->Generate();
}

BOOST_AUTO_TEST_SUITE_END()

