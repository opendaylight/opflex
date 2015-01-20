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

#include <yajr/transport/ZeroCopyOpenSSL.hpp>
#include <yajr/internal/comms.hpp>

#include <opflex/logging/OFLogHandler.h>
#include <opflex/logging/StdOutLogHandler.h>

#include <opflex/logging/internal/logging.hpp>

#include <boost/test/unit_test_log.hpp>
#include <boost/test/unit_test.hpp>

#include <utility>

#define DEFAULT_COMMSTEST_TIMEOUT 7200

using namespace yajr::comms;
using namespace yajr::transport;

BOOST_AUTO_TEST_SUITE(asynchronous_sockets)

struct CommsTests {

    CommsTests() {
        LOG(INFO) << "global setup\n";

        boost::unit_test::unit_test_log_t::instance()
            .set_threshold_level(::boost::unit_test::log_successful_tests);

        opflex::logging::OFLogHandler::registerHandler(commsTestLogger_);
    }

    ~CommsTests() {
        LOG(INFO) << "global teardown\n";
    }

    /* potentially subject to static initialization order fiasco */
    static opflex::logging::StdOutLogHandler commsTestLogger_;

};

opflex::logging::StdOutLogHandler CommsTests::commsTestLogger_(TRACE);

BOOST_GLOBAL_FIXTURE( CommsTests );

/**
 * A fixture for communications tests
 */

typedef std::pair<size_t, size_t> range_t;

class CommsFixture {
  private:
    uv_prepare_t prepare_;
    uv_timer_t timer_;
    uv_loop_t * loop_;

  protected:
    CommsFixture() : loop_(uv_default_loop()) {

        LOG(INFO) << "\n\n\n\n\n\n\n\n";

        prepare_.data = timer_.data = this;

        int rc = ::yajr::initLoop(loop_);

        BOOST_CHECK(!rc);

        for (size_t i=0; i < internal::Peer::LoopData::TOTAL_STATES; ++i) {
            BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerList(uv_default_loop(),
                        internal::Peer::LoopData::PeerState(i))
                    ->size(), 0);
        }

        internal::Peer::LoopData::getLoopData(loop_)->up();

#ifdef YAJR_HAS_OPENSSL
        ZeroCopyOpenSSL::initOpenSSL(false); // false because we don't use multiple threads
#endif

        eventCounter = 0;
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
        uv_prepare_stop(&prepare_);

        uv_close((uv_handle_t *)&timer_, down_on_close);
        uv_close((uv_handle_t *)&prepare_, down_on_close);

#ifdef YAJR_HAS_OPENSSL
        ZeroCopyOpenSSL::finiOpenSSL();
#endif

        ::yajr::finiLoop(uv_default_loop());

    }

    ~CommsFixture() {

        LOG(INFO) << "\n\n\n\n\n\n\n\n";

    }

    typedef void (*pc)(void);

    static range_t required_final_peers;
    static range_t required_transient_peers;
    static pc required_post_conditions;
    static bool expect_timeout;
    static size_t eventCounter;
    static size_t required_event_counter;

    static std::pair<size_t, size_t> count_peers() {

        static std::string oldDbgLog;
        std::stringstream dbgLog;
        std::string newDbgLog;

        size_t final_peers = 0, transient_peers = 0;

        size_t m;
        if((m=internal::Peer::LoopData::getPeerList(
                    uv_default_loop(),
                    internal::Peer::LoopData::ONLINE)->size())) {
            final_peers += m;
            dbgLog << " online: " << m;
        }
        if((m=internal::Peer::LoopData::getPeerList(
                    uv_default_loop(),
                    internal::Peer::LoopData::LISTENING)->size())) {
            final_peers += m;
            dbgLog << " listening: " << m;
        }

        if((m=internal::Peer::LoopData::getPeerList(
                    uv_default_loop(),
                    internal::Peer::LoopData::TO_RESOLVE)->size())) {
            transient_peers += m;
            dbgLog << " to_resolve: " << m;
        }
        if((m=internal::Peer::LoopData::getPeerList(
                    uv_default_loop(),
                    internal::Peer::LoopData::TO_LISTEN)->size())) {
            transient_peers += m;
            dbgLog << " to_listen: " << m;
        }

        if((m=internal::Peer::LoopData::getPeerList(
                    uv_default_loop(),
                    internal::Peer::LoopData::RETRY_TO_CONNECT)->size())) {
            final_peers += m;
            dbgLog << " retry-connecting: " << m;
        }
        if((m=internal::Peer::LoopData::getPeerList(
                    uv_default_loop(),
                    internal::Peer::LoopData::RETRY_TO_LISTEN)->size())) {
            final_peers += m;
            dbgLog << " retry-listening: " << m;
        }
        if((m=internal::Peer::LoopData::getPeerList(
                    uv_default_loop(),
                    internal::Peer::LoopData::ATTEMPTING_TO_CONNECT)->size())) {
            /* this is not a "final" state, from a test's perspective */
            transient_peers += m;
            dbgLog << " attempting: " << m;
        }
        if((m=internal::Peer::LoopData::getPeerList(
                    uv_default_loop(),
                    internal::Peer::LoopData::PENDING_DELETE)->size())) {
            /* this is not a "final" state, from a test's perspective */
            transient_peers += m;
            dbgLog << " pending_delete: " << m;
        }

        dbgLog
            << " TOTAL TRANSIENT: "
            << transient_peers
            << " / "
            << required_transient_peers.first
            << "-"
            << required_transient_peers.second
            << "\0"
        ;
        dbgLog
            << " TOTAL FINAL: "
            << final_peers
            << " / "
            << required_final_peers.first
            << "-"
            << required_final_peers.second
            << "\0"
        ;
        dbgLog
            << " EVENTS: "
            << eventCounter
        ;

        newDbgLog = dbgLog.str();

        if (oldDbgLog != newDbgLog) {
            oldDbgLog = newDbgLog;

            LOG(DEBUG) << newDbgLog;
        }

        return std::make_pair(final_peers, transient_peers);
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

    static void check_peer_db_cb(uv_prepare_t * handle) {

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

        /* at LEAST # required final peers must be in final state */
        if (
                (eventCounter < required_event_counter)
            ||
                (count_peers().first < required_final_peers.first)
            ||
                (count_peers().first > required_final_peers.second)
            ||
                (count_peers().second < required_transient_peers.first)
            ||
                (count_peers().second > required_transient_peers.second)
            ) {
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

    static void destroy_listener_cb(uv_timer_t * handle) {

        LOG(DEBUG);

        reinterpret_cast< ::yajr::Listener * >(handle->data)->destroy();

        uv_timer_stop(handle);
        if (!uv_is_closing((uv_handle_t*)handle)) {
            uv_close((uv_handle_t *)handle, NULL);
        }

    }

    static void destroy_peer_cb(uv_timer_t * handle) {

        LOG(DEBUG);

        reinterpret_cast< ::yajr::Peer* >(handle->data)->destroy();

        uv_timer_stop(handle);
        if (!uv_is_closing((uv_handle_t*)handle)) {
            uv_close((uv_handle_t *)handle, NULL);
        }

    }

    void loop_until_final(
            range_t final_peers,
            pc post_conditions,
            range_t transient_peers = range_t(0, 0),
            bool should_timeout = false,
            unsigned int timeout = DEFAULT_COMMSTEST_TIMEOUT,
            size_t num_events = 0
            ) {

        LOG(DEBUG);

        required_final_peers = final_peers;
        required_transient_peers = transient_peers;
        required_post_conditions = post_conditions;
        required_event_counter = num_events;
        expect_timeout = should_timeout;

        internal::Peer::LoopData::getLoopData(loop_)->up();
        uv_prepare_init(uv_default_loop(), &prepare_);
        uv_prepare_start(&prepare_, check_peer_db_cb);

        uv_timer_init(uv_default_loop(), &timer_);
        uv_timer_start(&timer_, timeout_cb, timeout, 0);
        uv_unref((uv_handle_t*) &timer_);

        uv_run(uv_default_loop(), UV_RUN_DEFAULT);

    }

    static void     disconnect_cb(uv_timer_t * handle);
    static void disconnect_now_cb(uv_timer_t * handle);
    static void        destroy_cb(uv_timer_t * handle);
    static void    destroy_now_cb(uv_timer_t * handle);
};

range_t CommsFixture::required_final_peers;
range_t CommsFixture::required_transient_peers;
size_t CommsFixture::required_event_counter;
CommsFixture::pc CommsFixture::required_post_conditions;
bool CommsFixture::expect_timeout;
size_t CommsFixture::eventCounter;

BOOST_FIXTURE_TEST_CASE( STABLE_test_initialization, CommsFixture ) {

    LOG(DEBUG);

    loop_until_final(range_t(0,0), NULL);

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

    /* no-transient guys */
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerList(uv_default_loop(),
                internal::Peer::LoopData::TO_RESOLVE)
            ->size(), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerList(uv_default_loop(),
                internal::Peer::LoopData::TO_LISTEN)
            ->size(), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerList(uv_default_loop(),
                internal::Peer::LoopData::PENDING_DELETE)
            ->size(), 0);

}

void DoNothingOnConnect (
        ::yajr::Peer * p,
        void * data,
        ::yajr::StateChange::To stateChange,
        int error) {
    switch(stateChange) {
        case ::yajr::StateChange::CONNECT:
            LOG(DEBUG)
                << "got a CONNECT notification on "
                << dynamic_cast< ::yajr::comms::internal::CommunicationPeer *>(p);
            LOG(INFO)
                << "chill out, we just had a connection on "
                << dynamic_cast< ::yajr::comms::internal::CommunicationPeer *>(p);
            break;
        case ::yajr::StateChange::DISCONNECT:
            LOG(DEBUG)
                << "got a DISCONNECT notification on "
                << dynamic_cast< ::yajr::comms::internal::CommunicationPeer *>(p);
            break;
        case ::yajr::StateChange::FAILURE:
            LOG(DEBUG)
                << "got a FAILURE notification on "
                << dynamic_cast< ::yajr::comms::internal::CommunicationPeer *>(p);
            break;
        case ::yajr::StateChange::DELETE:
            LOG(DEBUG)
                << "got a DELETE notification on "
                << dynamic_cast< ::yajr::comms::internal::CommunicationPeer *>(p);
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
            LOG(DEBUG)
                << "got a CONNECT notification on "
                << dynamic_cast< ::yajr::comms::internal::CommunicationPeer *>(p);
            LOG(INFO)
                << "starting keep-alive, as we just had a connection on "
                << dynamic_cast< ::yajr::comms::internal::CommunicationPeer *>(p);
            p->startKeepAlive();
            break;
        case ::yajr::StateChange::DISCONNECT:
            LOG(DEBUG)
                << "got a DISCONNECT notification on "
                << dynamic_cast< ::yajr::comms::internal::CommunicationPeer *>(p);
            break;
        case ::yajr::StateChange::FAILURE:
            LOG(DEBUG)
                << "got a FAILURE notification on "
                << dynamic_cast< ::yajr::comms::internal::CommunicationPeer *>(p);
            break;
        case ::yajr::StateChange::DELETE:
            LOG(DEBUG)
                << "got a DELETE notification on "
                << dynamic_cast< ::yajr::comms::internal::CommunicationPeer *>(p);
            break;
        default:
            assert(0);
    }
}

::yajr::Peer::StateChangeCb startPingingOnConnect = StartPingingOnConnect;

BOOST_FIXTURE_TEST_CASE( STABLE_test_ipv4, CommsFixture ) {

    LOG(DEBUG);

    ::yajr::Listener * l = ::yajr::Listener::create("127.0.0.1", 65535, doNothingOnConnect);

    BOOST_CHECK_EQUAL(!l, 0);

    ::yajr::Peer * p = ::yajr::Peer::create("localhost", "65535", doNothingOnConnect);

    BOOST_CHECK_EQUAL(!p, 0);

    loop_until_final(range_t(3,3), pc_successful_connect);

}

BOOST_FIXTURE_TEST_CASE( STABLE_test_ipv6, CommsFixture ) {

    LOG(DEBUG);

    ::yajr::Listener * l = ::yajr::Listener::create("::1", 65534, doNothingOnConnect);

    BOOST_CHECK_EQUAL(!l, 0);

    ::yajr::Peer * p = ::yajr::Peer::create("localhost", "65534", doNothingOnConnect);

    BOOST_CHECK_EQUAL(!p, 0);

    loop_until_final(range_t(3,3), pc_successful_connect);

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

    /* no-transient guys */
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerList(uv_default_loop(),
                internal::Peer::LoopData::TO_RESOLVE)
            ->size(), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerList(uv_default_loop(),
                internal::Peer::LoopData::TO_LISTEN)
            ->size(), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerList(uv_default_loop(),
                internal::Peer::LoopData::PENDING_DELETE)
            ->size(), 0);

}

BOOST_FIXTURE_TEST_CASE( STABLE_test_non_existent_host, CommsFixture ) {

    LOG(DEBUG);

    ::yajr::Peer * p = ::yajr::Peer::create("non_existent_host.", "65533", doNothingOnConnect);

    BOOST_CHECK_EQUAL(!p, 0);

    loop_until_final(range_t(1,1), pc_non_existent);

}

BOOST_FIXTURE_TEST_CASE( STABLE_test_non_existent_service, CommsFixture ) {

    LOG(DEBUG);

    ::yajr::Peer * p = ::yajr::Peer::create("127.0.0.1", "65533", doNothingOnConnect);

    BOOST_CHECK_EQUAL(!p, 0);

    loop_until_final(range_t(1,1), pc_non_existent);

}

BOOST_FIXTURE_TEST_CASE( STABLE_test_keepalive, CommsFixture ) {

    LOG(DEBUG);

    ::yajr::Listener * l = ::yajr::Listener::create("127.0.0.1", 65532, startPingingOnConnect);

    BOOST_CHECK_EQUAL(!l, 0);

    ::yajr::Peer * p = ::yajr::Peer::create("127.0.0.1", "65532", startPingingOnConnect);

    BOOST_CHECK_EQUAL(!p, 0);

    loop_until_final(range_t(4,4), pc_successful_connect, range_t(0,0), true, DEFAULT_COMMSTEST_TIMEOUT); // 4 is to cause a timeout

}

void pc_no_peers(void) {

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
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerList(uv_default_loop(),
                internal::Peer::LoopData::ONLINE)
            ->size(), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerList(uv_default_loop(),
                internal::Peer::LoopData::LISTENING)
            ->size(), 0);

    /* no-transient guys */
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerList(uv_default_loop(),
                internal::Peer::LoopData::TO_RESOLVE)
            ->size(), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerList(uv_default_loop(),
                internal::Peer::LoopData::TO_LISTEN)
            ->size(), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerList(uv_default_loop(),
                internal::Peer::LoopData::PENDING_DELETE)
            ->size(), 0);

}

BOOST_FIXTURE_TEST_CASE( STABLE_test_destroy_listener_early, CommsFixture ) {

    LOG(DEBUG);

    ::yajr::Listener * l = ::yajr::Listener::create("127.0.0.1", 65531, doNothingOnConnect);

    BOOST_CHECK_EQUAL(!l, 0);

    l->destroy();

    loop_until_final(range_t(0,0), pc_no_peers);

}

BOOST_FIXTURE_TEST_CASE( STABLE_test_destroy_listener_late, CommsFixture ) {

    LOG(DEBUG);

    ::yajr::Listener * l = ::yajr::Listener::create("127.0.0.1", 65531, doNothingOnConnect);

    BOOST_CHECK_EQUAL(!l, 0);

    uv_timer_t destroy_timer;

    destroy_timer.data = l;

    uv_timer_init(uv_default_loop(), &destroy_timer);
    uv_timer_start(&destroy_timer, destroy_listener_cb, 200, 0);
    uv_unref((uv_handle_t*) &destroy_timer);

    loop_until_final(range_t(0,0), pc_no_peers);

}

BOOST_FIXTURE_TEST_CASE( STABLE_test_destroy_client_early, CommsFixture ) {

    LOG(DEBUG);

    ::yajr::Peer * p = ::yajr::Peer::create("localhost", "65530", doNothingOnConnect);

    BOOST_CHECK_EQUAL(!p, 0);

    p->destroy();

    loop_until_final(range_t(0,0), pc_no_peers);

}

void pc_listening_peer(void) {

    LOG(DEBUG);

    /* one listener */
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerList(uv_default_loop(),
                internal::Peer::LoopData::LISTENING)
            ->size(), 1);

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
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerList(uv_default_loop(),
                internal::Peer::LoopData::ONLINE)
            ->size(), 0);

    /* no-transient guys */
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerList(uv_default_loop(),
                internal::Peer::LoopData::TO_RESOLVE)
            ->size(), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerList(uv_default_loop(),
                internal::Peer::LoopData::TO_LISTEN)
            ->size(), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerList(uv_default_loop(),
                internal::Peer::LoopData::PENDING_DELETE)
            ->size(), 0);

}

BOOST_FIXTURE_TEST_CASE( STABLE_test_destroy_client_late, CommsFixture ) {

    LOG(DEBUG);

    ::yajr::Listener * l = ::yajr::Listener::create("127.0.0.1", 65529, doNothingOnConnect);

    BOOST_CHECK_EQUAL(!l, 0);

    ::yajr::Peer * p = ::yajr::Peer::create("localhost", "65529", doNothingOnConnect);

    BOOST_CHECK_EQUAL(!p, 0);

    uv_timer_t destroy_timer;

    destroy_timer.data = p;

    uv_timer_init(uv_default_loop(), &destroy_timer);
    uv_timer_start(&destroy_timer, destroy_peer_cb, 200, 0);
    uv_unref((uv_handle_t*) &destroy_timer);

    loop_until_final(range_t(1,1), pc_listening_peer);

}

void DisconnectOnCallback (
        ::yajr::Peer * p,
        void * data,
        ::yajr::StateChange::To stateChange,
        int error) {
    switch(stateChange) {
        case ::yajr::StateChange::CONNECT:
            LOG(DEBUG)
                << "got a CONNECT notification on "
                << dynamic_cast< ::yajr::comms::internal::CommunicationPeer *>(p);
            LOG(INFO)
                << "we just had a connection on "
                << dynamic_cast< ::yajr::comms::internal::CommunicationPeer *>(p)
                << " and we are gonna Disconnect it"
            ;
            p->disconnect();
            break;
        case ::yajr::StateChange::DISCONNECT:
            LOG(DEBUG)
                << "got a DISCONNECT notification on "
                << dynamic_cast< ::yajr::comms::internal::CommunicationPeer *>(p);
            LOG(INFO)
                << "we had a disconnection on "
                << dynamic_cast< ::yajr::comms::internal::CommunicationPeer *>(p)
                << " and we are gonna Disconnect it nevertheless to verify that it does no harm"
            ;
            p->disconnect();
            break;
        case ::yajr::StateChange::FAILURE:
            LOG(DEBUG)
                << "got a FAILURE notification on "
                << dynamic_cast< ::yajr::comms::internal::CommunicationPeer *>(p);
            LOG(INFO)
                << "we failed to have a connection on "
                << dynamic_cast< ::yajr::comms::internal::CommunicationPeer *>(p)
                << " and we are gonna Disconnect it nevertheless to verify that it does no harm"
            ;
            p->disconnect();
            break;
        case ::yajr::StateChange::DELETE:
            LOG(DEBUG)
                << "got a DELETE notification on "
                << dynamic_cast< ::yajr::comms::internal::CommunicationPeer *>(p);
            break;
        default:
            assert(0);
    }
}

::yajr::Peer::StateChangeCb disconnectOnCallback = DisconnectOnCallback;

void DestroyOnCallback (
        ::yajr::Peer * p,
        void * data,
        ::yajr::StateChange::To stateChange,
        int error) {
    switch(stateChange) {
        case ::yajr::StateChange::CONNECT:
            LOG(DEBUG)
                << "got a CONNECT notification on "
                << dynamic_cast< ::yajr::comms::internal::CommunicationPeer *>(p);
            LOG(INFO)
                << "we just had a connection on "
                << dynamic_cast< ::yajr::comms::internal::CommunicationPeer *>(p)
                << " and we are gonna Destroy it"
            ;
            p->destroy();
            break;
        case ::yajr::StateChange::DISCONNECT:
            LOG(DEBUG)
                << "got a DISCONNECT notification on "
                << dynamic_cast< ::yajr::comms::internal::CommunicationPeer *>(p);
            break;
        case ::yajr::StateChange::FAILURE:
            LOG(DEBUG)
                << "got a FAILURE notification on "
                << dynamic_cast< ::yajr::comms::internal::CommunicationPeer *>(p);
            LOG(INFO)
                << "we failed to have a connection on "
                << dynamic_cast< ::yajr::comms::internal::CommunicationPeer *>(p)
                << " and we are gonna Destroy it nevertheless to verify that it does no harm"
            ;
            p->destroy();
            break;
        case ::yajr::StateChange::DELETE:
            LOG(DEBUG)
                << "got a DELETE notification on "
                << dynamic_cast< ::yajr::comms::internal::CommunicationPeer *>(p);
            break;
        default:
            assert(0);
    }
}

::yajr::Peer::StateChangeCb destroyOnCallback = DestroyOnCallback;

void DestroyOnDisconnect (
        ::yajr::Peer * p,
        void * data,
        ::yajr::StateChange::To stateChange,
        int error) {
    switch(stateChange) {
        case ::yajr::StateChange::CONNECT:
            LOG(DEBUG)
                << "got a CONNECT notification on "
                << dynamic_cast< ::yajr::comms::internal::CommunicationPeer *>(p);
            break;
        case ::yajr::StateChange::DISCONNECT:
            LOG(DEBUG)
                << "got a DISCONNECT notification on "
                << dynamic_cast< ::yajr::comms::internal::CommunicationPeer *>(p);
            LOG(INFO)
                << "we had a disconnection on "
                << dynamic_cast< ::yajr::comms::internal::CommunicationPeer *>(p)
                << " and we are gonna Disconnect it nevertheless to verify that it does no harm"
            ;
            p->destroy();
            break;
        case ::yajr::StateChange::FAILURE:
            LOG(DEBUG)
                << "got a FAILURE notification on "
                << dynamic_cast< ::yajr::comms::internal::CommunicationPeer *>(p);
            LOG(INFO)
                << "we failed to have a connection on "
                << dynamic_cast< ::yajr::comms::internal::CommunicationPeer *>(p)
                << " and we are not gonna do anything with it"
            ;
            break;
        case ::yajr::StateChange::DELETE:
            LOG(DEBUG)
                << "got a DELETE notification on "
                << dynamic_cast< ::yajr::comms::internal::CommunicationPeer *>(p);
            break;
        default:
            assert(0);
    }
}

::yajr::Peer::StateChangeCb destroyOnDisconnect = DestroyOnDisconnect;

BOOST_FIXTURE_TEST_CASE( STABLE_test_destroy_client_before_connect, CommsFixture ) {

    LOG(DEBUG);

    ::yajr::Peer * p = ::yajr::Peer::create("localhost", "65528", destroyOnCallback);

    BOOST_CHECK_EQUAL(!p, 0);

    loop_until_final(range_t(0,0), pc_no_peers);

}

void pc_retrying_client(void) {

    LOG(DEBUG);

    /* one listener */
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

    /* no-transient guys */
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerList(uv_default_loop(),
                internal::Peer::LoopData::TO_RESOLVE)
            ->size(), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerList(uv_default_loop(),
                internal::Peer::LoopData::TO_LISTEN)
            ->size(), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerList(uv_default_loop(),
                internal::Peer::LoopData::PENDING_DELETE)
            ->size(), 0);

}

BOOST_FIXTURE_TEST_CASE( STABLE_test_disconnect_client_before_connect, CommsFixture ) {

    LOG(DEBUG);

    ::yajr::Peer * p = ::yajr::Peer::create("localhost", "65527", disconnectOnCallback);

    BOOST_CHECK_EQUAL(!p, 0);

    loop_until_final(range_t(1,1), pc_retrying_client);

}

BOOST_FIXTURE_TEST_CASE( STABLE_test_destroy_client_after_connect, CommsFixture ) {

    LOG(DEBUG);

    ::yajr::Listener * l = ::yajr::Listener::create("127.0.0.1", 65526, doNothingOnConnect);

    BOOST_CHECK_EQUAL(!l, 0);

    ::yajr::Peer * p = ::yajr::Peer::create("localhost", "65526", destroyOnCallback);

    BOOST_CHECK_EQUAL(!p, 0);

    loop_until_final(range_t(1,1), pc_listening_peer);

}

void pc_retrying_peers(void) {

    LOG(DEBUG);

    /* one listener */
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerList(uv_default_loop(),
                internal::Peer::LoopData::RETRY_TO_CONNECT)
            ->size(), 1);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerList(uv_default_loop(),
                internal::Peer::LoopData::LISTENING)
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

    /* no-transient guys */
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerList(uv_default_loop(),
                internal::Peer::LoopData::TO_RESOLVE)
            ->size(), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerList(uv_default_loop(),
                internal::Peer::LoopData::TO_LISTEN)
            ->size(), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerList(uv_default_loop(),
                internal::Peer::LoopData::PENDING_DELETE)
            ->size(), 0);

}

BOOST_FIXTURE_TEST_CASE( STABLE_test_disconnect_client_after_connect, CommsFixture ) {

    LOG(DEBUG);

    ::yajr::Listener * l = ::yajr::Listener::create("127.0.0.1", 65525, doNothingOnConnect);

    BOOST_CHECK_EQUAL(!l, 0);

    ::yajr::Peer * p = ::yajr::Peer::create("localhost", "65525", disconnectOnCallback);

    BOOST_CHECK_EQUAL(!p, 0);

    loop_until_final(range_t(2,2), pc_retrying_peers);

}

BOOST_FIXTURE_TEST_CASE( STABLE_test_destroy_server_after_connect, CommsFixture ) {

    LOG(DEBUG);

    ::yajr::Listener * l = ::yajr::Listener::create("127.0.0.1", 65524, destroyOnCallback);
    BOOST_CHECK_EQUAL(!l, 0);

    ::yajr::Peer * p = ::yajr::Peer::create("localhost", "65524", destroyOnDisconnect);

    BOOST_CHECK_EQUAL(!p, 0);

    loop_until_final(range_t(1,1), pc_listening_peer);

}

BOOST_FIXTURE_TEST_CASE( STABLE_test_disconnect_server_after_connect, CommsFixture ) {

    LOG(DEBUG);

    ::yajr::Listener * l = ::yajr::Listener::create("127.0.0.1", 65523, disconnectOnCallback);
    BOOST_CHECK_EQUAL(!l, 0);

    ::yajr::Peer * p = ::yajr::Peer::create("localhost", "65523", destroyOnDisconnect);

    BOOST_CHECK_EQUAL(!p, 0);

    loop_until_final(range_t(1,1), pc_listening_peer);

}

::yajr::Listener * flakyListener;

void DisconnectAndStopListeningOnCallback (
        ::yajr::Peer * p,
        void * data,
        ::yajr::StateChange::To stateChange,
        int error) {
    switch(stateChange) {
        case ::yajr::StateChange::CONNECT:
            LOG(DEBUG)
                << "got a CONNECT notification on "
                << dynamic_cast< ::yajr::comms::internal::CommunicationPeer *>(p);
            LOG(INFO)
                << "we just had a connection on "
                << dynamic_cast< ::yajr::comms::internal::CommunicationPeer *>(p)
                << " and we are gonna Disconnect it"
            ;
            p->disconnect();
            LOG(INFO)
                << "also, we are gonna stop listening!"
            ;
            flakyListener->destroy();
            break;
        case ::yajr::StateChange::DISCONNECT:
            LOG(DEBUG)
                << "got a DISCONNECT notification on "
                << dynamic_cast< ::yajr::comms::internal::CommunicationPeer *>(p);
            LOG(INFO)
                << "we had a disconnection on "
                << dynamic_cast< ::yajr::comms::internal::CommunicationPeer *>(p)
                << " and we are gonna Disconnect it nevertheless to verify that it does no harm"
            ;
            p->disconnect();
            break;
        case ::yajr::StateChange::FAILURE:
            LOG(DEBUG)
                << "got a FAILURE notification on "
                << dynamic_cast< ::yajr::comms::internal::CommunicationPeer *>(p);
            LOG(INFO)
                << "we failed to have a connection on "
                << dynamic_cast< ::yajr::comms::internal::CommunicationPeer *>(p)
                << " and we are gonna Disconnect it nevertheless to verify that it does no harm"
            ;
            p->disconnect();
            break;
        case ::yajr::StateChange::DELETE:
            LOG(DEBUG)
                << "got a DELETE notification on "
                << dynamic_cast< ::yajr::comms::internal::CommunicationPeer *>(p);
            break;
        default:
            assert(0);
    }
}

::yajr::Peer::StateChangeCb disconnectAndStopListeningOnCallback = DisconnectAndStopListeningOnCallback;

void CountdownAttemptsOnCallback (
        ::yajr::Peer * p,
        void * data,
        ::yajr::StateChange::To stateChange,
        int error) {

    static size_t attempts = 4;

    switch(stateChange) {
        case ::yajr::StateChange::CONNECT:
            LOG(DEBUG)
                << "got a CONNECT notification on "
                << dynamic_cast< ::yajr::comms::internal::CommunicationPeer *>(p);
            break;
        case ::yajr::StateChange::DISCONNECT:
            LOG(DEBUG)
                << "got a DISCONNECT notification on "
                << dynamic_cast< ::yajr::comms::internal::CommunicationPeer *>(p);
            break;
        case ::yajr::StateChange::FAILURE:
            LOG(DEBUG)
                << "got a FAILURE notification on "
                << dynamic_cast< ::yajr::comms::internal::CommunicationPeer *>(p);
            if (!--attempts) {
                p->destroy();
            }
            break;
        case ::yajr::StateChange::DELETE:
            LOG(DEBUG)
                << "got a DELETE notification on "
                << dynamic_cast< ::yajr::comms::internal::CommunicationPeer *>(p);
            break;
        default:
            assert(0);
    }
}

::yajr::Peer::StateChangeCb countdownAttemptsOnCallback = CountdownAttemptsOnCallback;

void pc_no_server_and_client_gives_up(void) {

    LOG(DEBUG);

    /* no listener */
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerList(uv_default_loop(),
                internal::Peer::LoopData::LISTENING)
            ->size(), 0);

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
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerList(uv_default_loop(),
                internal::Peer::LoopData::ONLINE)
            ->size(), 0);

    /* no-transient guys */
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerList(uv_default_loop(),
                internal::Peer::LoopData::TO_RESOLVE)
            ->size(), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerList(uv_default_loop(),
                internal::Peer::LoopData::TO_LISTEN)
            ->size(), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerList(uv_default_loop(),
                internal::Peer::LoopData::PENDING_DELETE)
            ->size(), 0);

}

BOOST_FIXTURE_TEST_CASE( STABLE_test_client_retry_more_than_once, CommsFixture ) {

    LOG(DEBUG);

    flakyListener = ::yajr::Listener::create("127.0.0.1", 65522, disconnectAndStopListeningOnCallback);
    BOOST_CHECK_EQUAL(!flakyListener, 0);

    ::yajr::Peer * p = ::yajr::Peer::create("localhost", "65522", countdownAttemptsOnCallback);

    BOOST_CHECK_EQUAL(!p, 0);

    loop_until_final(range_t(0,0), pc_no_server_and_client_gives_up, range_t(0,0));

}

BOOST_FIXTURE_TEST_CASE( STABLE_test_destroy_client_for_non_existent_host, CommsFixture ) {

    LOG(DEBUG);

    ::yajr::Peer * p = ::yajr::Peer::create("non_existent_host.", "65521", destroyOnCallback);

    BOOST_CHECK_EQUAL(!p, 0);

    loop_until_final(range_t(1,1), pc_non_existent);

}

BOOST_FIXTURE_TEST_CASE( STABLE_test_destroy_client_for_non_existent_service, CommsFixture ) {

    LOG(DEBUG);

    ::yajr::Peer * p = ::yajr::Peer::create("127.0.0.1", "65520", destroyOnCallback);

    BOOST_CHECK_EQUAL(!p, 0);

    loop_until_final(range_t(0,0), pc_no_peers);

}

BOOST_FIXTURE_TEST_CASE( STABLE_test_disconnect_client_for_non_existent_host, CommsFixture ) {

    LOG(DEBUG);

    ::yajr::Peer * p = ::yajr::Peer::create("localhost", "65519", disconnectOnCallback);

    BOOST_CHECK_EQUAL(!p, 0);

    loop_until_final(range_t(1,1), pc_retrying_client);

}

BOOST_FIXTURE_TEST_CASE( STABLE_test_disconnect_client_for_non_existent_service, CommsFixture ) {

    LOG(DEBUG);

    ::yajr::Peer * p = ::yajr::Peer::create("localhost", "65518", disconnectOnCallback);

    BOOST_CHECK_EQUAL(!p, 0);

    loop_until_final(range_t(1,1), pc_retrying_client);

}

BOOST_FIXTURE_TEST_CASE( SLOW_test_non_routable_host, CommsFixture ) {

    LOG(DEBUG);

    ::yajr::Peer * p = ::yajr::Peer::create("255.255.255.254", "65517", doNothingOnConnect);

    BOOST_CHECK_EQUAL(!p, 0);

    loop_until_final(range_t(1,1), pc_non_existent, range_t(0,0), false, 90000);

}

BOOST_FIXTURE_TEST_CASE( SLOW_test_destroy_client_for_non_routable_host_upon_connection_failure, CommsFixture ) {

    LOG(DEBUG);

    ::yajr::Peer * p = ::yajr::Peer::create("255.255.255.254", "65516", destroyOnCallback);

    BOOST_CHECK_EQUAL(!p, 0);

    loop_until_final(range_t(0,0), pc_no_peers, range_t(0,0), false, 90000);

}

BOOST_FIXTURE_TEST_CASE( SLOW_test_disconnect_client_for_non_routable_host_upon_connection_failure, CommsFixture ) {

    LOG(DEBUG);

    ::yajr::Peer * p = ::yajr::Peer::create("255.255.255.254", "65515", disconnectOnCallback);

    BOOST_CHECK_EQUAL(!p, 0);

    loop_until_final(range_t(1,1), pc_non_existent, range_t(0,0), false, 90000);

}

void nothing_on_close(uv_handle_t * h) {
}

void CommsFixture::destroy_now_cb(uv_timer_t * handle) {

    ++eventCounter;

    ::yajr::comms::internal::CommunicationPeer * peer =
        static_cast< ::yajr::comms::internal::CommunicationPeer * >(handle->data)
    ;
    LOG(DEBUG)
        << "{"
        << static_cast<void *>(peer)
        << "} timer handle "
        << handle
    ;

    peer->destroy(true);

    uv_timer_stop(handle);
    uv_close((uv_handle_t *)handle, nothing_on_close);

}

BOOST_FIXTURE_TEST_CASE( STABLE_test_destroy_client_for_non_routable_host, CommsFixture ) {

    LOG(DEBUG);

    ::yajr::Peer * p = ::yajr::Peer::create("255.255.255.254", "65514", destroyOnCallback);

    BOOST_CHECK_EQUAL(!p, 0);

    uv_timer_t destroyTimer;

    LOG(DEBUG)
        << "destroyTimer @"
        << &destroyTimer
    ;

    uv_timer_init(uv_default_loop(), &destroyTimer);
    destroyTimer.data =
        dynamic_cast< ::yajr::comms::internal::CommunicationPeer * >(p);
    uv_timer_start(&destroyTimer, destroy_now_cb, 250, 0);
    uv_unref((uv_handle_t*) &destroyTimer);

    loop_until_final(range_t(0,0), pc_no_peers, range_t(0,0), false, 350, 1);

}

void CommsFixture::disconnect_now_cb(uv_timer_t * handle) {

    ++eventCounter;

    ::yajr::comms::internal::CommunicationPeer * peer =
        static_cast< ::yajr::comms::internal::CommunicationPeer * >(handle->data)
    ;
    LOG(DEBUG)
        << peer
        << " timer handle "
        << handle
    ;
    peer->disconnect(true);

    uv_timer_stop(handle);
    uv_close((uv_handle_t *)handle, nothing_on_close);

}

BOOST_FIXTURE_TEST_CASE( STABLE_test_disconnect_client_for_non_routable_host, CommsFixture ) {

    LOG(DEBUG);

    ::yajr::Peer * p = ::yajr::Peer::create("255.255.255.254", "65513", disconnectOnCallback);

    BOOST_CHECK_EQUAL(!p, 0);

    uv_timer_t disconnectTimer;

    LOG(DEBUG)
        << "disconnectTimer @"
        << &disconnectTimer
    ;

    uv_timer_init(uv_default_loop(), &disconnectTimer);
    disconnectTimer.data =
        dynamic_cast< ::yajr::comms::internal::CommunicationPeer * >(p);
    uv_timer_start(&disconnectTimer, disconnect_now_cb, 250, 0);
    uv_unref((uv_handle_t*) &disconnectTimer);

    eventCounter = 0;
    loop_until_final(range_t(1,1), pc_non_existent, range_t(0,0), false, 550, 1);

}

void CommsFixture::destroy_cb(uv_timer_t * handle) {

    ++eventCounter;

    ::yajr::comms::internal::CommunicationPeer * peer =
        static_cast< ::yajr::comms::internal::CommunicationPeer * >(handle->data)
    ;
    LOG(DEBUG)
        << peer
        << " timer handle "
        << handle
    ;

    peer->destroy();

    uv_timer_stop(handle);
    uv_close((uv_handle_t *)handle, nothing_on_close);

}

BOOST_FIXTURE_TEST_CASE( STABLE_test_destroy_client_semigracefully_for_non_routable_host, CommsFixture ) {

    LOG(DEBUG);

    ::yajr::Peer * p = ::yajr::Peer::create("255.255.255.254", "65512", destroyOnCallback);

    BOOST_CHECK_EQUAL(!p, 0);

    uv_timer_t destroyTimer;

    LOG(DEBUG)
        << "destroyTimer @"
        << &destroyTimer
    ;

    uv_timer_init(uv_default_loop(), &destroyTimer);
    destroyTimer.data =
        dynamic_cast< ::yajr::comms::internal::CommunicationPeer * >(p);
    uv_timer_start(&destroyTimer, destroy_cb, 250, 0);
    uv_unref((uv_handle_t*) &destroyTimer);

    uv_timer_t destroyNowTimer;

    LOG(DEBUG)
        << "destroyNowTimer @"
        << &destroyNowTimer
    ;

    uv_timer_init(uv_default_loop(), &destroyNowTimer);
    destroyNowTimer.data =
        dynamic_cast< ::yajr::comms::internal::CommunicationPeer * >(p);
    uv_timer_start(&destroyNowTimer, destroy_now_cb, 251, 0);
    uv_unref((uv_handle_t*) &destroyNowTimer);

    loop_until_final(range_t(0,0), pc_no_peers, range_t(0,0), false, 400, 2);

}

void CommsFixture::disconnect_cb(uv_timer_t * handle) {

    ++eventCounter;

    ::yajr::comms::internal::CommunicationPeer * peer =
        static_cast< ::yajr::comms::internal::CommunicationPeer * >(handle->data)
    ;
    LOG(DEBUG)
        << peer
        << " timer handle "
        << handle
    ;
    peer->disconnect();

    uv_timer_stop(handle);
    uv_close((uv_handle_t *)handle, nothing_on_close);

}

BOOST_FIXTURE_TEST_CASE( STABLE_test_disconnect_client_semigracefully_for_non_routable_host, CommsFixture ) {

    LOG(DEBUG);

    ::yajr::Peer * p = ::yajr::Peer::create("255.255.255.254", "65511", disconnectOnCallback);

    BOOST_CHECK_EQUAL(!p, 0);

    uv_timer_t disconnectTimer;

    LOG(DEBUG)
        << "disconnectTimer @"
        << &disconnectTimer
    ;

    uv_timer_init(uv_default_loop(), &disconnectTimer);
    disconnectTimer.data =
        dynamic_cast< ::yajr::comms::internal::CommunicationPeer * >(p);
    uv_timer_start(&disconnectTimer, disconnect_cb, 250, 0);
    uv_unref((uv_handle_t*) &disconnectTimer);

    uv_timer_t disconnectNowTimer;

    LOG(DEBUG)
        << "disconnectNowTimer @"
        << &disconnectNowTimer
    ;

    uv_timer_init(uv_default_loop(), &disconnectNowTimer);
    disconnectNowTimer.data =
        dynamic_cast< ::yajr::comms::internal::CommunicationPeer * >(p);
    uv_timer_start(&disconnectNowTimer, disconnect_now_cb, 251, 0);
    uv_unref((uv_handle_t*) &disconnectNowTimer);

    loop_until_final(range_t(1,1), pc_non_existent, range_t(0,0), false, 400, 2);

}

BOOST_FIXTURE_TEST_CASE( STABLE_test_destroy_then_disconnect_client_semigracefully_for_non_routable_host, CommsFixture ) {

    LOG(DEBUG);

    ::yajr::Peer * p = ::yajr::Peer::create("255.255.255.254", "65512", destroyOnCallback);

    BOOST_CHECK_EQUAL(!p, 0);

    uv_timer_t destroyTimer;

    LOG(DEBUG)
        << "destroyTimer @"
        << &destroyTimer
    ;

    uv_timer_init(uv_default_loop(), &destroyTimer);
    destroyTimer.data =
        dynamic_cast< ::yajr::comms::internal::CommunicationPeer * >(p);
    uv_timer_start(&destroyTimer, destroy_cb, 250, 0);
    uv_unref((uv_handle_t*) &destroyTimer);

    uv_timer_t disconnectNowTimer;

    LOG(DEBUG)
        << "disconnectNowTimer @"
        << &disconnectNowTimer
    ;

    uv_timer_init(uv_default_loop(), &disconnectNowTimer);
    disconnectNowTimer.data =
        dynamic_cast< ::yajr::comms::internal::CommunicationPeer * >(p);
    uv_timer_start(&disconnectNowTimer, disconnect_now_cb, 251, 0);
    uv_unref((uv_handle_t*) &disconnectNowTimer);

    loop_until_final(range_t(0,0), pc_no_peers, range_t(0,0), false, 400, 2);

}

BOOST_FIXTURE_TEST_CASE( STABLE_test_disconnect_then_destroy_client_semigracefully_for_non_routable_host, CommsFixture ) {

    LOG(DEBUG);

    ::yajr::Peer * p = ::yajr::Peer::create("255.255.255.254", "65512", destroyOnCallback);

    BOOST_CHECK_EQUAL(!p, 0);

    uv_timer_t disconnectTimer;

    LOG(DEBUG)
        << "disconnectTimer @"
        << &disconnectTimer
    ;

    uv_timer_init(uv_default_loop(), &disconnectTimer);
    disconnectTimer.data =
        dynamic_cast< ::yajr::comms::internal::CommunicationPeer * >(p);
    uv_timer_start(&disconnectTimer, disconnect_cb, 250, 0);
    uv_unref((uv_handle_t*) &disconnectTimer);

    uv_timer_t destroyNowTimer;

    LOG(DEBUG)
        << "destroyNowTimer @"
        << &destroyNowTimer
    ;

    uv_timer_init(uv_default_loop(), &destroyNowTimer);
    destroyNowTimer.data =
        dynamic_cast< ::yajr::comms::internal::CommunicationPeer * >(p);
    uv_timer_start(&destroyNowTimer, destroy_now_cb, 251, 0);
    uv_unref((uv_handle_t*) &destroyNowTimer);

    loop_until_final(range_t(0,0), pc_no_peers, range_t(0,0), false, 400, 2);

}


#ifdef YAJR_HAS_OPENSSL

void AttachPassiveSslTransportOnConnect(
        ::yajr::Peer * p,
        void * data,
        ::yajr::StateChange::To stateChange,
        int error) {
    switch(stateChange) {
        case ::yajr::StateChange::CONNECT:
            LOG(DEBUG)
                << "got a CONNECT notification on "
                << dynamic_cast< ::yajr::comms::internal::CommunicationPeer *>(p);
            LOG(INFO)
                << "attaching passive SSL transport "
                << data
                << " to "
                << dynamic_cast< ::yajr::comms::internal::CommunicationPeer *>(p);

            BOOST_CHECK_EQUAL(

            ZeroCopyOpenSSL::attachTransport(
                    p,
                    static_cast<ZeroCopyOpenSSL::Ctx *>(data))

            , true);

            break;
        case ::yajr::StateChange::DISCONNECT:
            LOG(DEBUG)
                << "got a DISCONNECT notification on "
                << dynamic_cast< ::yajr::comms::internal::CommunicationPeer *>(p);
            break;
        case ::yajr::StateChange::FAILURE:
            LOG(DEBUG)
                << "got a FAILURE notification on "
                << dynamic_cast< ::yajr::comms::internal::CommunicationPeer *>(p);
            break;
        case ::yajr::StateChange::DELETE:
            LOG(DEBUG)
                << "got a DELETE notification on "
                << dynamic_cast< ::yajr::comms::internal::CommunicationPeer *>(p);
            break;
        default:
            assert(0);
    }
}

::yajr::Peer::StateChangeCb attachPassiveSslTransportOnConnect = AttachPassiveSslTransportOnConnect;

void * passthroughAccept(yajr::Listener *, void * data, int) {
    return data;
}

BOOST_FIXTURE_TEST_CASE( STABLE_test_no_message_on_SSL, CommsFixture ) {

    LOG(DEBUG);

    ::yajr::transport::ZeroCopyOpenSSL::Ctx * serverCtx =
        ::yajr::transport::ZeroCopyOpenSSL::Ctx::createCtx(
            NULL,
            SRCDIR"/test/server.pem",
            "password123"
        );

    BOOST_CHECK_EQUAL(!serverCtx, 0);

    if (!serverCtx) {
        return;
    }

    ::yajr::Listener * l = ::yajr::Listener::create(
            "127.0.0.1",
            65514,
            attachPassiveSslTransportOnConnect,
            passthroughAccept,
            serverCtx
    );

    BOOST_CHECK_EQUAL(!l, 0);

    ::yajr::Peer * p = ::yajr::Peer::create(
            "127.0.0.1",
            "65514",
            doNothingOnConnect
    );

    BOOST_CHECK_EQUAL(!p, 0);

    ::yajr::transport::ZeroCopyOpenSSL::Ctx * clientCtx =
        ::yajr::transport::ZeroCopyOpenSSL::Ctx::createCtx(
            SRCDIR"/test/ca.pem",
            NULL
        );

    BOOST_CHECK_EQUAL(!clientCtx, 0);

    if (!clientCtx) {
        return;
    }

    bool ok = ZeroCopyOpenSSL::attachTransport(p, clientCtx);

    BOOST_CHECK_EQUAL(ok, true);

    if (!ok) {
        return;
    }

    loop_until_final(range_t(4,4), pc_successful_connect, range_t(0,0), true, 800); // 4 is to cause a timeout

}

void SinglePingOnConnect(
        ::yajr::Peer * p,
        void * data,
        ::yajr::StateChange::To stateChange,
        int error) {
    switch(stateChange) {
        case ::yajr::StateChange::CONNECT:
            LOG(DEBUG)
                << "got a CONNECT notification on "
                << dynamic_cast< ::yajr::comms::internal::CommunicationPeer *>(p);
            LOG(INFO)
                << "sending a single Ping, as we just had a connection on "
                << dynamic_cast< ::yajr::comms::internal::CommunicationPeer *>(p);

            dynamic_cast< ::yajr::comms::internal::CommunicationPeer *>(p)
                ->sendEchoReq(); /* just one! */
            break;
        case ::yajr::StateChange::DISCONNECT:
            LOG(DEBUG)
                << "got a DISCONNECT notification on "
                << dynamic_cast< ::yajr::comms::internal::CommunicationPeer *>(p);
            break;
        case ::yajr::StateChange::FAILURE:
            LOG(DEBUG)
                << "got a FAILURE notification on "
                << dynamic_cast< ::yajr::comms::internal::CommunicationPeer *>(p);
            break;
        case ::yajr::StateChange::DELETE:
            LOG(DEBUG)
                << "got a DELETE notification on "
                << dynamic_cast< ::yajr::comms::internal::CommunicationPeer *>(p);
            break;
        default:
            assert(0);
    }
}

::yajr::Peer::StateChangeCb singlePingOnConnect = SinglePingOnConnect;

BOOST_FIXTURE_TEST_CASE( STABLE_test_single_message_on_SSL, CommsFixture ) {

    LOG(DEBUG);

    ::yajr::transport::ZeroCopyOpenSSL::Ctx * serverCtx =
        ::yajr::transport::ZeroCopyOpenSSL::Ctx::createCtx(
            NULL,
            SRCDIR"/test/server.pem",
            "password123"
        );

    BOOST_CHECK_EQUAL(!serverCtx, 0);

    if (!serverCtx) {
        return;
    }

    ::yajr::Listener * l = ::yajr::Listener::create(
            "127.0.0.1",
            65514,
            attachPassiveSslTransportOnConnect,
            passthroughAccept,
            serverCtx
    );

    BOOST_CHECK_EQUAL(!l, 0);

    ::yajr::Peer * p = ::yajr::Peer::create(
            "127.0.0.1",
            "65514",
            singlePingOnConnect
    );

    BOOST_CHECK_EQUAL(!p, 0);

    ::yajr::transport::ZeroCopyOpenSSL::Ctx * clientCtx =
        ::yajr::transport::ZeroCopyOpenSSL::Ctx::createCtx(
            SRCDIR"/test/ca.pem",
            NULL
        );

    BOOST_CHECK_EQUAL(!clientCtx, 0);

    if (!clientCtx) {
        return;
    }

    bool ok = ZeroCopyOpenSSL::attachTransport(p, clientCtx);

    BOOST_CHECK_EQUAL(ok, true);

    if (!ok) {
        return;
    }

    loop_until_final(range_t(4,4), pc_successful_connect, range_t(0,0), true, 800); // 4 is to cause a timeout

}

BOOST_FIXTURE_TEST_CASE( STABLE_test_keepalive_on_SSL, CommsFixture ) {

    LOG(DEBUG);

    ::yajr::transport::ZeroCopyOpenSSL::Ctx * serverCtx =
        ::yajr::transport::ZeroCopyOpenSSL::Ctx::createCtx(
            NULL,
            "test/server.pem",
            "password123"
        );

    BOOST_CHECK_EQUAL(!serverCtx, 0);

    if (!serverCtx) {
        return;
    }

    ::yajr::Listener * l = ::yajr::Listener::create(
            "127.0.0.1",
            65513,
            attachPassiveSslTransportOnConnect,
            passthroughAccept,
            serverCtx
    );

    BOOST_CHECK_EQUAL(!l, 0);

    ::yajr::Peer * p = ::yajr::Peer::create(
            "127.0.0.1",
            "65513",
            startPingingOnConnect
    );

    BOOST_CHECK_EQUAL(!p, 0);

    ::yajr::transport::ZeroCopyOpenSSL::Ctx * clientCtx =
        ::yajr::transport::ZeroCopyOpenSSL::Ctx::createCtx(
            "test/ca.pem",
            NULL
        );

    BOOST_CHECK_EQUAL(!clientCtx, 0);

    if (!clientCtx) {
        return;
    }

    bool ok = ZeroCopyOpenSSL::attachTransport(p, clientCtx);

    BOOST_CHECK_EQUAL(ok, true);

    if (!ok) {
        return;
    }

    loop_until_final(range_t(4,4), pc_successful_connect, range_t(0,0), true, DEFAULT_COMMSTEST_TIMEOUT); // 4 is to cause a timeout

}

#endif


BOOST_AUTO_TEST_SUITE_END()
