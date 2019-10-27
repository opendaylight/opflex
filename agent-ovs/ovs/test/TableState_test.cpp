/*
 * Test suite for class TableState.
 *
 * Copyright (c) 2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <boost/test/unit_test.hpp>
#include <boost/functional/hash.hpp>

#include <cstring>
#include <algorithm>
#include <unordered_set>

#include "TableState.h"
#include "FlowBuilder.h"
#include <opflexagent/logging.h>

#include "ovs-shim.h"
#include "ovs-ofputil.h"

using namespace opflexagent;

struct cookieMatch {
    uint64_t cookie;
    uint64_t prio;
    struct match match;
};

namespace std {

template<> struct hash<cookieMatch> {
    size_t operator()(const cookieMatch& key) const noexcept {
        size_t hashv = match_hash(&key.match, 0);
        boost::hash_combine(hashv, key.prio);
        boost::hash_combine(hashv, key.cookie);
        return hashv;
    }
};

} /* namespace std */

bool operator==(const cookieMatch& lhs, const cookieMatch& rhs) {
    return lhs.prio == rhs.prio && lhs.cookie == rhs.cookie &&
        match_equal(&lhs.match, &rhs.match);
}
bool operator!=(const cookieMatch& lhs, const cookieMatch& rhs) {
    return !(lhs == rhs);
}

typedef std::unordered_set<cookieMatch> cookieMatchSet;

BOOST_AUTO_TEST_SUITE(TableState_test)

class TableStateFixture {
public:
    TableStateFixture()
        : f1_1(FlowBuilder().priority(1).inPort(5)
               .action().output(4)
               .parent().build()),
          f1_2(FlowBuilder().priority(1).inPort(5)
               .action().decTtl().output(4)
               .parent().build()),
          f2_1(FlowBuilder().priority(1).inPort(4).action().output(5)
               .parent().build()),
          f2_2(FlowBuilder().priority(1).inPort(4).cookie(ovs_htonll(0x2))
               .action().output(5).parent().build()),
          f2_3(FlowBuilder().priority(1).inPort(4).cookie(ovs_htonll(0x3))
               .action().output(5).parent().build()),
          f3_1(FlowBuilder().priority(10).inPort(5).cookie(ovs_htonll(0x1))
               .action().output(6)
               .parent().build()),
          f3_2(FlowBuilder().priority(10).inPort(5).cookie(ovs_htonll(0x2))
               .action().output(6)
               .parent().build()) {}

    TableState state;
    FlowEntryList el;
    FlowEdit diffs;

    FlowEntryPtr f1_1;
    FlowEntryPtr f1_2;
    FlowEntryPtr f2_1;
    FlowEntryPtr f2_2;
    FlowEntryPtr f2_3;
    FlowEntryPtr f3_1;
    FlowEntryPtr f3_2;
};

BOOST_FIXTURE_TEST_CASE(apply, TableStateFixture) {
    el.push_back(f1_1);
    el.push_back(f2_1);

    state.apply("test", el, diffs);
    std::sort(diffs.edits.begin(), diffs.edits.end());

    BOOST_REQUIRE(2 == diffs.edits.size());
    BOOST_CHECK_EQUAL(FlowEdit::ADD, diffs.edits[0].first);
    BOOST_CHECK(diffs.edits[0].second->matchEq(f2_1.get()));
    BOOST_CHECK(diffs.edits[0].second->actionEq(f2_1.get()));
    BOOST_CHECK_EQUAL(FlowEdit::ADD, diffs.edits[1].first);
    BOOST_CHECK(diffs.edits[1].second->matchEq(f1_1.get()));
    BOOST_CHECK(diffs.edits[1].second->actionEq(f1_1.get()));

    el.clear();
    el.push_back(f1_1);
    el.push_back(f2_1);
    state.apply("test", el, diffs);
    BOOST_REQUIRE(0 == diffs.edits.size());

    el.clear();
    el.push_back(f1_2);
    state.apply("test", el, diffs);

    BOOST_REQUIRE(2 == diffs.edits.size());
    BOOST_CHECK_EQUAL(FlowEdit::MOD, diffs.edits[0].first);
    BOOST_CHECK(diffs.edits[0].second->matchEq(f1_2.get()));
    BOOST_CHECK(diffs.edits[0].second->actionEq(f1_2.get()));
    BOOST_CHECK_EQUAL(FlowEdit::DEL, diffs.edits[1].first);
    BOOST_CHECK(diffs.edits[1].second->matchEq(f2_1.get()));

    el.clear();
    el.push_back(f1_1);
    state.apply("conflict", el, diffs);

    BOOST_REQUIRE(0 == diffs.edits.size());

    el.clear();
    state.apply("test", el, diffs);
    BOOST_REQUIRE(1 == diffs.edits.size());
    BOOST_CHECK_EQUAL(FlowEdit::MOD, diffs.edits[0].first);
    BOOST_CHECK(diffs.edits[0].second->matchEq(f1_1.get()));
    BOOST_CHECK(diffs.edits[0].second->actionEq(f1_1.get()));

    el.clear();
    el.push_back(f1_2);
    state.apply("conflict2", el, diffs);
    BOOST_REQUIRE(0 == diffs.edits.size());

    el.clear();
    state.apply("conflict2", el, diffs);
    BOOST_REQUIRE(0 == diffs.edits.size());

    el.clear();
    el.push_back(f3_1);
    state.apply("conflict2", el, diffs);
    BOOST_REQUIRE(1 == diffs.edits.size());
    BOOST_CHECK_EQUAL(FlowEdit::ADD, diffs.edits[0].first);
    BOOST_CHECK(diffs.edits[0].second->matchEq(f3_1.get()));
    BOOST_CHECK(diffs.edits[0].second->actionEq(f3_1.get()));

    cookieMatchSet expCSet1 {
        {0x1, 10, f3_1->entry->match}
    };
    cookieMatchSet actual;
    TableState::cookie_callback_t cb =
        [&actual](uint64_t c,
                  uint16_t p,
                  const struct match& m) {
        actual.insert({c, p, m});
    };

    state.forEachCookieMatch(cb);
    BOOST_CHECK(expCSet1 == actual);

    el.clear();
    el.push_back(f3_2);
    state.apply("conflict2", el, diffs);

    cookieMatchSet expCSet2 {
        {0x2, 10, f3_2->entry->match}
    };
    actual.clear();
    state.forEachCookieMatch(cb);
    BOOST_CHECK(expCSet2 == actual);

    el.clear();
    el.push_back(f2_2);
    state.apply("test", el, diffs);

    cookieMatchSet expCSet3 {
        {0x2, 10, f3_2->entry->match},
        {0x2, 1,  f2_2->entry->match},
    };
    actual.clear();
    state.forEachCookieMatch(cb);
    BOOST_CHECK(expCSet3 == actual);

    el.clear();
    state.apply("test", el, diffs);

    actual.clear();
    state.forEachCookieMatch(cb);
    BOOST_CHECK(expCSet2 == actual);

    el.clear();
    el.push_back(f2_2);
    state.apply("test", el, diffs);
    el.clear();
    el.push_back(f2_3);
    state.apply("cookieconflict", el, diffs);

    actual.clear();
    state.forEachCookieMatch(cb);
    BOOST_CHECK(expCSet3 == actual);

    el.clear();
    state.apply("test", el, diffs);
    cookieMatchSet expCSet4 {
        {0x2, 10, f3_2->entry->match},
        {0x3, 1,  f2_2->entry->match},
    };

    actual.clear();
    state.forEachCookieMatch(cb);
    BOOST_CHECK(expCSet4 == actual);
}

BOOST_FIXTURE_TEST_CASE(diff, TableStateFixture) {
    el.push_back(f1_1);
    el.push_back(f2_1);
    state.apply("test", el, diffs);

    el.clear();
    el.push_back(f1_2);
    el.push_back(f3_1);
    state.diffSnapshot(el, diffs);
    std::sort(diffs.edits.begin(), diffs.edits.end());
    for (const FlowEdit::Entry& e : diffs.edits) {
        LOG(DEBUG) << e;
    }

    BOOST_REQUIRE(3 == diffs.edits.size());
    BOOST_CHECK_EQUAL(FlowEdit::ADD, diffs.edits[0].first);
    BOOST_CHECK(diffs.edits[0].second->matchEq(f2_1.get()));
    BOOST_CHECK(diffs.edits[0].second->actionEq(f2_1.get()));
    BOOST_CHECK_EQUAL(FlowEdit::MOD, diffs.edits[1].first);
    BOOST_CHECK(diffs.edits[1].second->matchEq(f1_1.get()));
    BOOST_CHECK(diffs.edits[1].second->actionEq(f1_1.get()));
    BOOST_CHECK_EQUAL(FlowEdit::DEL, diffs.edits[2].first);
    BOOST_CHECK(diffs.edits[2].second->matchEq(f3_1.get()));
}

BOOST_AUTO_TEST_SUITE_END()
