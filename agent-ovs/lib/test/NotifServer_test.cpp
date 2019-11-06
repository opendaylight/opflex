/*
 * Test suite for class NotifServer
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <opflexagent/NotifServer.h>
#include <opflexagent/test/BaseFixture.h>
#include <opflexagent/logging.h>

#include <boost/test/unit_test.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <opflex/modb/MAC.h>

#include <cstdio>

namespace opflexagent {

namespace ba = boost::asio;
using ba::local::stream_protocol;
using rapidjson::StringBuffer;
using rapidjson::Writer;
using rapidjson::Document;

BOOST_AUTO_TEST_SUITE(NotifServer_test)

static const std::string SOCK_NAME("/tmp/notif_test.sock");

class NotifFixture {
public:
    NotifFixture() : notif(io) {
        std::remove(SOCK_NAME.c_str());
        notif.setSocketName(SOCK_NAME);
        notif.start();
        io_service_thread.reset(new std::thread([this]() { io.run(); }));
    }

    ~NotifFixture() {
        notif.stop();

        if (io_service_thread) {
            io_service_thread->join();
            io_service_thread.reset();
        }
        std::remove(SOCK_NAME.c_str());
    }

    ba::io_service io;
    NotifServer notif;
    std::unique_ptr<std::thread> io_service_thread;
};

void readMessage(stream_protocol::socket& s, Document& result) {
    uint32_t rsize;
    ba::read(s, ba::buffer(&rsize, 4));
    rsize = ntohl(rsize);
    std::vector<uint8_t> rbuffer;
    rbuffer.resize(rsize+1);
    rbuffer[rsize] = '\0';

    ba::read(s, ba::buffer(rbuffer, rsize));
    result.Parse((const char*)&rbuffer[0]);

}

BOOST_FIXTURE_TEST_CASE(subscribe, NotifFixture) {

    struct stat buffer;
    WAIT_FOR(stat(SOCK_NAME.c_str(), &buffer) == 0, 500);

    LOG(INFO) << "Connecting";

    stream_protocol::socket s(io);
    s.connect(stream_protocol::endpoint(SOCK_NAME));

    StringBuffer r;
    Writer<StringBuffer> writer(r);
    writer.StartObject();
    writer.Key("method");
    writer.String("subscribe");
    writer.Key("params");
    writer.StartObject();
    writer.Key("type");
    writer.StartArray();
    writer.String("virtual-ip");
    writer.EndArray();
    writer.EndObject();
    writer.Key("id");
    writer.String("1");
    writer.EndObject();
    std::string t("test");
    uint32_t size = htonl(r.GetSize());
    ba::write(s, ba::buffer(&size, 4));
    ba::write(s, ba::buffer(r.GetString(), r.GetSize()));

    Document rdoc;
    readMessage(s, rdoc);

    {
        std::unordered_set<std::string> uuids;
        uuids.insert("4412dcd2-0cd0-4741-99d1-d8b3946e1fa9");
        uuids.insert("1cc9483a-8d7a-48d5-9c23-862401691e01");
        notif.dispatchVirtualIp(uuids,
                                opflex::modb::MAC("11:22:33:44:55:66"),
                                "1.2.3.4");
    }

    Document notif;
    readMessage(s, notif);

    BOOST_REQUIRE(notif.HasMember("method"));
    BOOST_REQUIRE(notif["method"].IsString());
    BOOST_CHECK_EQUAL("virtual-ip",
                      std::string(notif["method"].GetString()));
    BOOST_REQUIRE(notif.HasMember("params"));
    BOOST_REQUIRE(notif["params"].IsObject());

    const rapidjson::Value& p = notif["params"];
    BOOST_REQUIRE(p.HasMember("mac"));
    BOOST_REQUIRE(p["mac"].IsString());
    BOOST_CHECK_EQUAL("11:22:33:44:55:66",
                      std::string(p["mac"].GetString()));

    BOOST_REQUIRE(p.HasMember("ip"));
    BOOST_REQUIRE(p["ip"].IsString());
    BOOST_CHECK_EQUAL("1.2.3.4",
                      std::string(p["ip"].GetString()));

    BOOST_REQUIRE(p.HasMember("uuid"));
    BOOST_REQUIRE(p["uuid"].IsArray());
    std::set<std::string> us;
    const rapidjson::Value& uuids = p["uuid"];
    rapidjson::Value::ConstValueIterator it;
    for (it = uuids.Begin(); it != uuids.End(); ++it) {
        BOOST_REQUIRE(it->IsString());
        us.insert(it->GetString());
    }
    BOOST_CHECK_EQUAL(2, us.size());
    BOOST_CHECK(us.find("4412dcd2-0cd0-4741-99d1-d8b3946e1fa9") != us.end());
    BOOST_CHECK(us.find("1cc9483a-8d7a-48d5-9c23-862401691e01") != us.end());
}

BOOST_AUTO_TEST_SUITE_END()

}
