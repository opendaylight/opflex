/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Test suite for Processor class.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <boost/test/unit_test.hpp>
#include <opflex/comms/comms-internal.hpp>
#include <opflex/logging/internal/logging.hpp>
#include <cstdlib>

using opflex::comms::internal::peers;
using opflex::comms::comms_passive_listener;
using opflex::comms::comms_active_connection;

BOOST_AUTO_TEST_SUITE(asynchronous_sockets)

/**
 * A fixture for communications tests
 */
class CommsFixture {
  private:
    uv_idle_t  * idler;
    uv_timer_t * timer;

  protected:
    CommsFixture()
        :
            idler((typeof(idler))malloc(sizeof(*idler))),
            timer((typeof(timer))malloc(sizeof(*timer))) {

        LOG(DEBUG) << "idler = " << idler;
        LOG(DEBUG) << "timer = " << timer;

        int rc = opflex::comms::initCommunicationLoop(uv_default_loop());

        BOOST_CHECK(!rc);

        BOOST_CHECK(d_intr_list_is_empty(&peers.online));
        BOOST_CHECK(d_intr_list_is_empty(&peers.retry));
        BOOST_CHECK(d_intr_list_is_empty(&peers.attempting));
        BOOST_CHECK(d_intr_list_is_empty(&peers.retry_listening));
        BOOST_CHECK(d_intr_list_is_empty(&peers.listening));

        uv_idle_init(uv_default_loop(), idler);
        uv_idle_start(idler, check_peer_db_cb);

        uv_timer_init(uv_default_loop(), timer);
        uv_timer_start(timer, timeout_cb, 500, 0);
        uv_unref((uv_handle_t*) timer);

    }

    ~CommsFixture() {

        uv_timer_stop(timer);
        uv_idle_stop(idler);
        uv_close((uv_handle_t *)timer, (uv_close_cb)free);
        uv_close((uv_handle_t *)idler, (uv_close_cb)free);

        cleanup_dlist(&peers.online);
        cleanup_dlist(&peers.retry);
        cleanup_dlist(&peers.attempting);
        cleanup_dlist(&peers.retry_listening);
        cleanup_dlist(&peers.listening);

        opflex::comms::finiCommunicationLoop(uv_default_loop());

    }

    typedef void (*pc)(void);

    static size_t required_final_peers;
    static pc required_post_conditions;
    static bool expect_timeout;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
    static void cleanup_dlist(d_intr_head_t *head) {

        LOG(DEBUG) << head;

        opflex::comms::internal::Peer * peer;

        D_INTR_LIST_FOR_EACH(peer, head, peer_hook) {
            LOG(DEBUG) << "unlinking " << peer;
            d_intr_list_unlink(&peer->peer_hook);

            LOG(DEBUG) << "deleting " << peer;
            peer->down();
        }
    }
#pragma clang diagnostic pop

    static size_t count_final_peers() {

        static std::string oldDbgLog;
        std::stringstream dbgLog;
        std::string newDbgLog;

        size_t n_peers = 0;

        if(!d_intr_list_is_empty(&peers.online)) {
            ++n_peers;
            dbgLog << " online";
        }
        if(!d_intr_list_is_empty(&peers.retry)) {
            ++n_peers;
            dbgLog << " retry-connecting";
        }
        if(!d_intr_list_is_empty(&peers.attempting)) {
            /* this is not a "final" state, from a test's perspective */
            dbgLog << " attempting";
        }
        if(!d_intr_list_is_empty(&peers.retry_listening)) {
            ++n_peers;
            dbgLog << " retry-listening";
        }
        if(!d_intr_list_is_empty(&peers.listening)) {
            ++n_peers;
            dbgLog << " listening";
        }

        dbgLog << " " << n_peers << "\0";

        newDbgLog = dbgLog.str();

        if (oldDbgLog != newDbgLog) {
            oldDbgLog = newDbgLog;

            LOG(DEBUG) << newDbgLog;
        }

        return n_peers;
    }

    static void check_peer_db_cb(uv_idle_t * handle) {

        (void) handle;

        if (count_final_peers() < required_final_peers) {
            return;
        }

        (*required_post_conditions)();

        uv_idle_stop(handle);
        uv_stop(uv_default_loop());

    }

    static void timeout_cb(uv_timer_t * handle) {

        LOG(DEBUG);

        if (!expect_timeout) {
            BOOST_CHECK(!"the test has timed out");
        }

        (*required_post_conditions)();

        uv_stop(uv_default_loop());
    }

    void loop_until_final(size_t final_peers, pc post_conditions, bool timeout = false) {

        LOG(DEBUG);

        required_final_peers = final_peers;
        required_post_conditions = post_conditions;
        expect_timeout = timeout;

        uv_run(uv_default_loop(), UV_RUN_DEFAULT);

    }

};

size_t CommsFixture::required_final_peers;
CommsFixture::pc CommsFixture::required_post_conditions;
bool CommsFixture::expect_timeout;

BOOST_FIXTURE_TEST_CASE( test_initialization, CommsFixture ) {

    LOG(DEBUG);

}

void pc_successful_connect(void) {

    LOG(DEBUG);

    /* empty */
    BOOST_CHECK(d_intr_list_is_empty(&peers.retry));
    BOOST_CHECK(d_intr_list_is_empty(&peers.retry_listening));
    BOOST_CHECK(d_intr_list_is_empty(&peers.attempting));

    /* non-empty */
    BOOST_CHECK(!d_intr_list_is_empty(&peers.online));
    BOOST_CHECK(!d_intr_list_is_empty(&peers.listening));

}

class DoNothingOnConnect {
  public:
    void operator()(opflex::comms::internal::CommunicationPeer * p) {
        LOG(INFO) << "chill out, we just had a" << (
                p->passive ? " pass" : "n act"
            ) << "ive connection";
    }
};

opflex::comms::internal::ConnectionHandler doNothingOnConnect = DoNothingOnConnect();

class StartPingingOnConnect {
  public:
    void operator()(opflex::comms::internal::CommunicationPeer * p) {
        p->startKeepAlive(300, 50, 100);
    }
};

opflex::comms::internal::ConnectionHandler startPingingOnConnect = StartPingingOnConnect();

BOOST_FIXTURE_TEST_CASE( test_ipv4, CommsFixture ) {

    LOG(DEBUG);

    int rc;

    rc = comms_passive_listener(doNothingOnConnect, "127.0.0.1", 65535);

    BOOST_CHECK_EQUAL(rc, 0);

    rc = comms_active_connection(doNothingOnConnect, "localhost", "65535");

    BOOST_CHECK_EQUAL(rc, 0);

    loop_until_final(2, pc_successful_connect);

}

BOOST_FIXTURE_TEST_CASE( test_ipv6, CommsFixture ) {

    LOG(DEBUG);

    int rc;

    rc = comms_passive_listener(doNothingOnConnect, "::1", 65534);

    BOOST_CHECK_EQUAL(rc, 0);

    rc = comms_active_connection(doNothingOnConnect, "localhost", "65534");

    BOOST_CHECK_EQUAL(rc, 0);

    loop_until_final(2, pc_successful_connect);

}

static void pc_non_existent(void) {

    LOG(DEBUG);

    /* empty */
    BOOST_CHECK(!d_intr_list_is_empty(&peers.retry));

    /* non-empty */
    BOOST_CHECK(d_intr_list_is_empty(&peers.retry_listening));
    BOOST_CHECK(d_intr_list_is_empty(&peers.attempting));
    BOOST_CHECK(d_intr_list_is_empty(&peers.online));
    BOOST_CHECK(d_intr_list_is_empty(&peers.listening));

}

BOOST_FIXTURE_TEST_CASE( test_non_existent_host, CommsFixture ) {

    LOG(DEBUG);

    int rc = comms_active_connection(doNothingOnConnect, "non_existent_host.", "65533");

    BOOST_CHECK_EQUAL(rc, 0);

    loop_until_final(1, pc_non_existent);

}

BOOST_FIXTURE_TEST_CASE( test_non_existent_service, CommsFixture ) {

    LOG(DEBUG);

    int rc = comms_active_connection(doNothingOnConnect, "127.0.0.1", "65533");

    BOOST_CHECK_EQUAL(rc, 0);

    loop_until_final(1, pc_non_existent);

}

BOOST_FIXTURE_TEST_CASE( test_keepalive, CommsFixture ) {

    LOG(DEBUG);

    int rc;

    rc = comms_passive_listener(startPingingOnConnect, "::1", 65532);

    BOOST_CHECK_EQUAL(rc, 0);

    rc = comms_active_connection(startPingingOnConnect, "::1", "65532");

    BOOST_CHECK_EQUAL(rc, 0);

    loop_until_final(3, pc_successful_connect, true); // 3 is to cause a timeout

}

BOOST_AUTO_TEST_SUITE_END()
