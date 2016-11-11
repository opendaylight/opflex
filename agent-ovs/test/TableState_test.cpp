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

#include <cstring>
#include <algorithm>

#include "TableState.h"
#include "FlowBuilder.h"
#include "logging.h"

using namespace ovsagent;

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
          f3_1(FlowBuilder().priority(10).inPort(5).cookie(0x1)
               .action().output(6)
               .parent().build()) {}

    TableState state;
    FlowEntryList el;
    FlowEdit diffs;

    FlowEntryPtr f1_1;
    FlowEntryPtr f1_2;
    FlowEntryPtr f2_1;
    FlowEntryPtr f3_1;
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
