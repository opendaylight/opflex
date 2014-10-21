/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */


#ifndef MODBFIXTURE_H_
#define MODBFIXTURE_H_

#include <internal/modb.h>
#include <internal/model.h>

using namespace std;
using namespace opflex::modb;
using namespace opflex::model;

class ModbFixture {
public:
    ModbFixture() {
        EpBuilder epb0;
        initObjs.push_back(epb0.reset("/t0/ep/0").setOfport("8").setMac("be:ad:fe:ed:ce:de")
                               .setIpAddr("10.20.44.32").setGroup("/t0/epg/0").build());

        initObjs.push_back(epb0.reset("/t0/ep/1").setMac("ce:ad:fe:ed:ce:de").build());

        initObjs.push_back(epb0.reset("/t0/ep/2").setMac("de:ad:fe:ed:ce:de").setIpAddr("10.20.44.35")
                               .setGroup("/t0/epg/0").build());

        EpgBuilder epgb0;
        initObjs.push_back(epgb0.reset("/t0/epg/0").setId("10")
                                .setNetwork("/t0/net/subnet/0").build());
        initObjs.push_back(epgb0.reset("/t0/epg/1").setId("11")
                                .setNetwork("/t0/net/l3/0").build());

        SnBuilder snb0;
        initObjs.push_back(snb0.reset("/t0/net/subnet/0").setParent("/t0/net/bd/0")
                               .setId("500").setGateway("10.20.44.255").build());

        BdBuilder bdb0;
        initObjs.push_back(bdb0.reset("/t0/net/bd/0").setParent("/t0/net/l3/0").setId("600").build());

        L3Builder l3b0;
        initObjs.push_back(l3b0.reset("/t0/net/l3/0").setId("700").build());

        PolicyBuilder pb;
        initObjs.push_back(pb.reset("/t0/policy/0").setSEpg("/t0/epg/0").setDEpg("/t0/epg/110").build());

        for (int i = 0;i < initObjs.size();++i) {
            opflex::modb::Add(initObjs[i]);
        }
    }

    ~ModbFixture() {

    }

    std::vector<MoBasePtr> initObjs;

protected:

    template <class_id_t cid>
    class MOBuilder {
    public:
        MoBasePtr build() { return mo; }
    protected:
        MOBuilder() {}
        MOBuilder& reset(const string& uriStr) {
            mo.reset(new MoBase(cid, URI(uriStr)));
            return *this;
        }
        MOBuilder& setProperty(const string& prop, const string& parent) {
            mo->Set(prop, parent);
            return *this;
        }
    private:
        MoBasePtr mo;
    };

    class EpBuilder : public MOBuilder<CLASS_ID_ENDPOINT> {
    public:
        EpBuilder& reset(const string& uri) { MOBuilder::reset(uri); return *this; }
        EpBuilder& setOfport(const string& ofport) { setProperty("ofport", ofport); return *this; }
        EpBuilder& setMac(const string& mac) { setProperty("macAddr", mac); return *this; }
        EpBuilder& setGroup(const string& epgUri) { setProperty("epgUri", epgUri); return *this; }
        EpBuilder& setIpAddr(const string& ipAddr) { setProperty("ipAddr", ipAddr); return *this; }
    };

    class EpgBuilder : public MOBuilder<CLASS_ID_ENDPOINT_GROUP> {
    public:
        EpgBuilder& reset(const string& uri) { MOBuilder::reset(uri); return *this; }
        EpgBuilder& setId(const string& id) { setProperty("id", id); return *this; }
        EpgBuilder& setNetwork(const string& netDomUri) { setProperty("networkDomainUri", netDomUri); return *this; }
    };

    class SnBuilder : public MOBuilder<CLASS_ID_SUBNET> {
    public:
        SnBuilder& reset(const string& uri) { MOBuilder::reset(uri); return *this; }
        SnBuilder& setId(const string& id) { setProperty("id", id); return *this; }
        SnBuilder& setParent(const string& parent) { setProperty("parentUri", parent); return *this; }
        SnBuilder& setGateway(const string& gw) { setProperty("gateway", gw); return *this; }
    };

    class BdBuilder : public MOBuilder<CLASS_ID_BD> {
    public:
        BdBuilder& reset(const string& uri) { MOBuilder::reset(uri); return *this; }
        BdBuilder& setId(const string& id) { setProperty("id", id); return *this; }
        BdBuilder& setParent(const string& parent) { setProperty("parentUri", parent); return *this; }
    };

    class FdBuilder : public MOBuilder<CLASS_ID_FD> {
    public:
        FdBuilder& reset(const string& uri) { MOBuilder::reset(uri); return *this; }
        FdBuilder& setId(const string& id) { setProperty("id", id); return *this; }
        FdBuilder& setParent(const string& parent) { setProperty("parentUri", parent); return *this; }
    };

    class L3Builder : public MOBuilder<CLASS_ID_L3CTX> {
    public:
        L3Builder& reset(const string& uri) { MOBuilder::reset(uri); return *this; }
        L3Builder& setId(const string& id) { setProperty("id", id); return *this; }
    };

    class PolicyBuilder : public MOBuilder<CLASS_ID_POLICY> {
    public:
        PolicyBuilder& reset(const string& uri) { MOBuilder::reset(uri); return *this; }
        PolicyBuilder& setSEpg(const string& sepg) { setProperty("sEpgUri", sepg); return *this; }
        PolicyBuilder& setDEpg(const string& depg) { setProperty("dEpgUri", depg); return *this; }
        PolicyBuilder& setActions(const string& act) { setProperty("actions", act); return *this; }
    };
};


#endif /* MODBFIXTURE_H_ */
