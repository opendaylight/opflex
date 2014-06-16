/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

/*
    rc = uv_timer_init(config->loop, &worker->restart_timer);
    rc = uv_stop(handle->loop);
*/

#include <noiro/policy-agent/comms-internal.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>
#include <noiro/debug_with_levels.h>

static void post_conditions(void) {
    extern peer_db_t peers;

    assert_true(d_intr_list_is_empty(&peers.retry));
    assert_true(d_intr_list_is_empty(&peers.retry_listening));

    assert_false(d_intr_list_is_empty(&peers.online));
    assert_false(d_intr_list_is_empty(&peers.listening));
}

static void timeout_cb(uv_timer_t * handle) {
    post_conditions();

    uv_stop(uv_default_loop());
}

static size_t count_peers() {
    extern peer_db_t peers;

    size_t n_peers = 0;

    if(!d_intr_list_is_empty(&peers.online)) {
        ++ n_peers;
    }
    if(!d_intr_list_is_empty(&peers.retry)) {
        ++n_peers;
    }
    if(!d_intr_list_is_empty(&peers.retry_listening)) {
        ++n_peers;
    }
    if(!d_intr_list_is_empty(&peers.listening)) {
        ++n_peers;
    }

    return n_peers;
}

static void check_peer_db_cb(uv_idle_t * handle) {

    (void) handle;

    if (count_peers() < 2) {
        return;
    }

    post_conditions();

    uv_idle_stop(handle);
    uv_stop(uv_default_loop());
}

static void loop_until_final(void) {
    uv_idle_t idler;
    uv_timer_t timer;

    uv_idle_init(uv_default_loop(), &idler);
    uv_idle_start(&idler, check_peer_db_cb);

    uv_timer_init(uv_default_loop(), &timer);
    uv_unref((uv_handle_t*) &timer);
    uv_timer_start(&timer, timeout_cb, 2000, 0);

    uv_run(uv_default_loop(), UV_RUN_DEFAULT);
}

static void empty_dlist(d_intr_head_t *head) {
    peer_t * peer;

    D_INTR_LIST_FOR_EACH(peer, head, peer_hook) {
        d_intr_list_unlink(&peer->peer_hook);
    }
}

static void test_initialization(void **state) {

    (void) state;

    extern peer_db_t peers;

    assert_true(d_intr_list_is_empty(&peers.online));
    assert_true(d_intr_list_is_empty(&peers.retry));
    assert_true(d_intr_list_is_empty(&peers.retry_listening));
    assert_true(d_intr_list_is_empty(&peers.listening));
}

static void test_ipv4(void **state) {

    (void) state;

    empty_dlist(&peers.online);
    empty_dlist(&peers.retry);
    empty_dlist(&peers.retry_listening);
    empty_dlist(&peers.listening);

    int rc;

    rc = comms_passive_listener("127.0.0.1", 65535);

    assert_int_equal(rc, 0);

    rc = comms_active_connection("localhost", "65535");

    assert_int_equal(rc, 0);

    loop_until_final();
}

static void test_ipv6(void **state) {

    (void) state;

    empty_dlist(&peers.online);
    empty_dlist(&peers.retry);
    empty_dlist(&peers.retry_listening);
    empty_dlist(&peers.listening);

    int rc;

    rc = comms_passive_listener("::1", 65534);

    assert_int_equal(rc, 0);

    rc = comms_active_connection("localhost", "65534");

    assert_int_equal(rc, 0);

    loop_until_final();
}

int main(int argc, char *argv[])
{
#ifndef _WIN32
      /* Ignore SIGPIPE */
      signal(SIGPIPE, SIG_IGN);
#endif  /* !_WIN32 */

    const UnitTest tests[] = {
        unit_test(test_initialization),
        unit_test(test_ipv4),
        unit_test(test_ipv6),
    };

    return run_tests(tests);
}

