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
#include <yajr/internal/comms.hpp>
#include <opflex/logging/internal/logging.hpp>
#include <cstdlib>
#include <boost/test/unit_test_log.hpp>

#include <dlfcn.h>
#include <cstring>

using namespace yajr::comms;

BOOST_AUTO_TEST_SUITE(asynchronous_sockets)

struct CommsTests {
    CommsTests() {
        LOG(INFO) << "global setup\n";

        boost::unit_test::unit_test_log_t::instance().set_threshold_level(::boost::unit_test::log_successful_tests);
    }
    ~CommsTests() {
        LOG(INFO) << "global teardown\n";
    }
};

BOOST_GLOBAL_FIXTURE( CommsTests );

/**
 * A fixture for communications tests
 */
class CommsFixture {
  private:
    uv_idle_t  idler_;
    uv_timer_t timer_;
    uv_loop_t * loop_;

  protected:
    CommsFixture() : loop_(uv_default_loop()) {

        LOG(INFO) << "\n\n\n\n\n\n\n\n";

        idler_.data = timer_.data = this;

        int rc = ::yajr::initLoop(loop_);

        BOOST_CHECK(!rc);

        for (size_t i=0; i < internal::Peer::LoopData::TOTAL_STATES; ++i) {
            BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerList(uv_default_loop(),
                        internal::Peer::LoopData::PeerState(i))
                    ->size(), 0);
        }

        internal::Peer::LoopData::getLoopData(loop_)->up();
        uv_timer_init(uv_default_loop(), &timer_);
        uv_timer_start(&timer_, timeout_cb, 7200, 0);
        uv_unref((uv_handle_t*) &timer_);

    }

    struct PeerDisposer {
        void operator()(internal::Peer *peer)
        {
            LOG(DEBUG) << peer << " destroy() with intent of deleting";
            peer->destroy();
        }
    };

    static void down_on_close(uv_handle_t * h) {
        internal::Peer::LoopData::getLoopData(h->loop)->down();
    }

    void cleanup() {

        LOG(DEBUG);

        uv_timer_stop(&timer_);
        uv_idle_stop(&idler_);

        uv_close((uv_handle_t *)&timer_, down_on_close);
        uv_close((uv_handle_t *)&idler_, down_on_close);

        ::yajr::finiLoop(uv_default_loop());

    }

    ~CommsFixture() {

        LOG(INFO) << "\n\n\n\n\n\n\n\n";

    }

    typedef void (*pc)(void);

    static size_t required_final_peers;
    static pc required_post_conditions;
    static bool expect_timeout;

    static size_t count_final_peers() {

        static std::string oldDbgLog;
        std::stringstream dbgLog;
        std::string newDbgLog;

        size_t final_peers = 0;

        size_t m;
        if((m=internal::Peer::LoopData::getPeerList(
                    uv_default_loop(),
                    internal::Peer::LoopData::ONLINE)->size())) {
            final_peers += m;
            dbgLog << " online: " << m;
        }
        if((m=internal::Peer::LoopData::getPeerList(
                    uv_default_loop(),
                    internal::Peer::LoopData::RETRY_TO_CONNECT)->size())) {
            final_peers += m;
            dbgLog << " retry-connecting: " << m;
        }
        if((m=internal::Peer::LoopData::getPeerList(
                    uv_default_loop(),
                    internal::Peer::LoopData::ATTEMPTING_TO_CONNECT)->size())) {
            /* this is not a "final" state, from a test's perspective */
            dbgLog << " attempting: " << m;
        }
        if((m=internal::Peer::LoopData::getPeerList(
                    uv_default_loop(),
                    internal::Peer::LoopData::RETRY_TO_LISTEN)->size())) {
            final_peers += m;
            dbgLog << " retry-listening: " << m;
        }
        if((m=internal::Peer::LoopData::getPeerList(
                    uv_default_loop(),
                    internal::Peer::LoopData::LISTENING)->size())) {
            final_peers += m;
            dbgLog << " listening: " << m;
        }

        dbgLog << " TOTAL FINAL: " << final_peers << "\0";

        newDbgLog = dbgLog.str();

        if (oldDbgLog != newDbgLog) {
            oldDbgLog = newDbgLog;

            LOG(DEBUG) << newDbgLog;
        }

        return final_peers;
    }

#if 0
    static void wait_for_zero_peers_cb(uv_idle_t * handle) {

        static size_t old_count;
        size_t count = internal::Peer::getCounter();

        if (count) {
            if (old_count != count) {
                LOG(DEBUG) << count << " peers left";
            }
            old_count = count;
            return;
        }
        LOG(DEBUG) << "no peers left";

        uv_idle_stop(handle);
        uv_stop(uv_default_loop());

    }
#endif

    static void dump_peer_db_brief() {

        static std::string oldDbgLog;
        std::stringstream dbgLog;
        std::string newDbgLog;

#if 1
        for (size_t i=0; i < internal::Peer::LoopData::TOTAL_STATES; ++i) {
            internal::Peer::List * pL = internal::Peer::LoopData::getPeerList(uv_default_loop(),
                        internal::Peer::LoopData::PeerState(i));

            dbgLog << "\n";

            dbgLog << " pL #" << i << " @" << pL;

            dbgLog
                << "[" << static_cast<void*>(&*pL->begin())
                << "->" << static_cast<void*>(&*pL->end()) << "]{"
                << (
                        static_cast<char*>(static_cast<void*>(&*pL->end()))
                    -
                        static_cast<char*>(static_cast<void*>(&*pL->begin()))
                   )
                << "}";
        }
#endif

        newDbgLog = dbgLog.str();

        if (oldDbgLog != newDbgLog) {
            oldDbgLog = newDbgLog;

            LOG(DEBUG) << newDbgLog;
        }

    }

    static void check_peer_db_cb(uv_idle_t * handle) {

        dump_peer_db_brief();

        static std::string oldDbgLog;
        std::stringstream dbgLog;
        std::string newDbgLog;

        bool good = true;
        bool peerGood;

        /* check all easily reachable peers' invariants */
#if 1
        for (size_t i=0; i < internal::Peer::LoopData::TOTAL_STATES; ++i) {
            internal::Peer::List * pL = internal::Peer::LoopData::getPeerList(uv_default_loop(),
                        internal::Peer::LoopData::PeerState(i));

            dbgLog << "\n";

            dbgLog << " pL #" << i << " @" << pL;

            dbgLog
                << "[" << static_cast<void*>(&*pL->begin())
                << "->" << static_cast<void*>(&*pL->end()) << "]("
                << pL->size() << ")";

            for(internal::Peer::List::iterator it = pL->begin(); it != pL->end(); ++it) {
                dbgLog << " peer " << &*it;
                peerGood = it->__checkInvariants();
                if (!peerGood) {
                    dbgLog << " BAD!";
                    good = false;
                }
            }
        }
#endif

        newDbgLog = dbgLog.str();

        if (oldDbgLog != newDbgLog) {
            oldDbgLog = newDbgLog;

            LOG(DEBUG) << newDbgLog;
        }

        assert(good);

        if (count_final_peers() < required_final_peers) {
            return;
        }

        if (required_post_conditions) {
            (*required_post_conditions)();
        }

        reinterpret_cast<CommsFixture*>(handle->data)->cleanup();

    }

    static void timeout_cb(uv_timer_t * handle) {

        LOG(DEBUG);

        if (!expect_timeout) {
            BOOST_CHECK(!"the test has timed out");
        }

        if (required_post_conditions) {
            (*required_post_conditions)();
        }

        reinterpret_cast<CommsFixture*>(handle->data)->cleanup();

    }

    void loop_until_final(size_t final_peers, pc post_conditions, bool timeout = false) {

        LOG(DEBUG);

        required_final_peers = final_peers;
        required_post_conditions = post_conditions;
        expect_timeout = timeout;

        internal::Peer::LoopData::getLoopData(loop_)->up();
        uv_idle_init(uv_default_loop(), &idler_);
        uv_idle_start(&idler_, check_peer_db_cb);

        uv_run(uv_default_loop(), UV_RUN_DEFAULT);

    }

};

size_t CommsFixture::required_final_peers;
CommsFixture::pc CommsFixture::required_post_conditions;
bool CommsFixture::expect_timeout;

BOOST_FIXTURE_TEST_CASE( test_initialization, CommsFixture ) {

    LOG(DEBUG);

    loop_until_final(0, NULL);

}

void pc_successful_connect(void) {

    LOG(DEBUG);

    /* empty */
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerList(uv_default_loop(),
                internal::Peer::LoopData::RETRY_TO_CONNECT)
            ->size(), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerList(uv_default_loop(),
                internal::Peer::LoopData::RETRY_TO_LISTEN)
            ->size(), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerList(uv_default_loop(),
                internal::Peer::LoopData::ATTEMPTING_TO_CONNECT)
            ->size(), 0);

    /* non-empty */
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerList(uv_default_loop(),
                internal::Peer::LoopData::ONLINE)
            ->size(), 2);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerList(uv_default_loop(),
                internal::Peer::LoopData::LISTENING)
            ->size(), 1);

}

void DoNothingOnConnect (
        ::yajr::Peer * p,
        void * data,
        ::yajr::StateChange::To stateChange,
        int error) {
    switch(stateChange) {
        case ::yajr::StateChange::CONNECT:
            LOG(INFO)
                << "chill out, we just had a connection on "
                << dynamic_cast< ::yajr::comms::internal::CommunicationPeer *>(p);
            break;
        case ::yajr::StateChange::DISCONNECT:
            break;
        case ::yajr::StateChange::FAILURE:
            break;
        default:
            assert(0);
    }
}

::yajr::Peer::StateChangeCb doNothingOnConnect = DoNothingOnConnect;

void StartPingingOnConnect(
        ::yajr::Peer * p,
        void * data,
        ::yajr::StateChange::To stateChange,
        int error) {
    switch(stateChange) {
        case ::yajr::StateChange::CONNECT:
            LOG(INFO)
                << "starting keep-alive, as we just had a connection on "
                << dynamic_cast< ::yajr::comms::internal::CommunicationPeer *>(p);
            p->startKeepAlive();
            break;
        case ::yajr::StateChange::DISCONNECT:
            break;
        case ::yajr::StateChange::FAILURE:
            break;
        default:
            assert(0);
    }
}

::yajr::Peer::StateChangeCb startPingingOnConnect = StartPingingOnConnect;

BOOST_FIXTURE_TEST_CASE( test_ipv4, CommsFixture ) {

    LOG(DEBUG);

    ::yajr::Listener * l = ::yajr::Listener::create("127.0.0.1", 65535, doNothingOnConnect);

    BOOST_CHECK_EQUAL(!l, 0);

    ::yajr::Peer * p = ::yajr::Peer::create("localhost", "65535", doNothingOnConnect);

    BOOST_CHECK_EQUAL(!p, 0);

    loop_until_final(3, pc_successful_connect);

}

BOOST_FIXTURE_TEST_CASE( test_ipv6, CommsFixture ) {

    LOG(DEBUG);

    ::yajr::Listener * l = ::yajr::Listener::create("::1", 65534, doNothingOnConnect);

    BOOST_CHECK_EQUAL(!l, 0);

    ::yajr::Peer * p = ::yajr::Peer::create("localhost", "65534", doNothingOnConnect);

    BOOST_CHECK_EQUAL(!p, 0);

    loop_until_final(3, pc_successful_connect);

}

static void pc_non_existent(void) {

    LOG(DEBUG);

    /* non-empty */
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerList(uv_default_loop(),
                internal::Peer::LoopData::RETRY_TO_CONNECT)
            ->size(), 1);

    /* empty */
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerList(uv_default_loop(),
                internal::Peer::LoopData::RETRY_TO_LISTEN)
            ->size(), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerList(uv_default_loop(),
                internal::Peer::LoopData::ATTEMPTING_TO_CONNECT)
            ->size(), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerList(uv_default_loop(),
                internal::Peer::LoopData::ONLINE)
            ->size(), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerList(uv_default_loop(),
                internal::Peer::LoopData::LISTENING)
            ->size(), 0);

}

BOOST_FIXTURE_TEST_CASE( test_non_existent_host, CommsFixture ) {

    LOG(DEBUG);

    ::yajr::Peer * p = ::yajr::Peer::create("non_existent_host.", "65533", doNothingOnConnect);

    BOOST_CHECK_EQUAL(!p, 0);

    loop_until_final(1, pc_non_existent);

}

BOOST_FIXTURE_TEST_CASE( test_non_existent_service, CommsFixture ) {

    LOG(DEBUG);

    ::yajr::Peer * p = ::yajr::Peer::create("127.0.0.1", "65533", doNothingOnConnect);

    BOOST_CHECK_EQUAL(!p, 0);

    loop_until_final(1, pc_non_existent);

}

BOOST_FIXTURE_TEST_CASE( test_keepalive, CommsFixture ) {

    LOG(DEBUG);

    ::yajr::Listener * l = ::yajr::Listener::create("::1", 65532, startPingingOnConnect);

    BOOST_CHECK_EQUAL(!l, 0);

    ::yajr::Peer * p = ::yajr::Peer::create("::1", "65532", startPingingOnConnect);

    BOOST_CHECK_EQUAL(!p, 0);

    loop_until_final(4, pc_successful_connect, true); // 4 is to cause a timeout

}

BOOST_AUTO_TEST_SUITE_END()
