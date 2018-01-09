/*
 * Test suite for class RangeMask.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <sstream>
#include <boost/test/unit_test.hpp>
#include <boost/assign/list_of.hpp>

#include "RangeMask.h"
#include <opflexagent/logging.h>

using namespace opflexagent;
using namespace std;
using namespace boost;
using namespace boost::assign;

void expect(const optional<uint16_t>& s,
            const optional<uint16_t>& e,
            const MaskList& exp) {
    MaskList ml;
    RangeMask::getMasks(s, e, ml);
    stringstream expStr, gotStr;
    expStr << exp;
    gotStr << ml;
    BOOST_CHECK_MESSAGE(ml == exp,
        "[" << (s ? s.get() : -1) << "," << (e ? e.get() : -1) << "]"
        << "\nexp: " << expStr.str() << "\ngot: " << gotStr.str());
}


BOOST_AUTO_TEST_SUITE(RangeMask_test)

BOOST_AUTO_TEST_CASE(range) {

    expect(optional<uint16_t>(), optional<uint16_t>(), MaskList());
    expect(optional<uint16_t>(), 5, list_of<Mask>(0x0005, 0xffff));
    expect(10, optional<uint16_t>(), list_of<Mask>(0x000a, 0xffff));

    expect(uint16_t(0), 1, list_of<Mask>(0x0000, 0xfffe));

    expect(uint16_t(0), 3, list_of<Mask>(0x0000, 0xfffc));

    expect(32, 32, list_of<Mask>(0x0020, 0xffff));

    expect(13, 2,
        list_of<Mask>(0x0002, 0xfffe)(0x0004, 0xfffc)
            (0x0008, 0xfffc)(0x000c, 0xfffe));

    expect(6, 13,
        list_of<Mask>(0x0006, 0xfffe)(0x0008, 0xfffc)
            (0x000c, 0xfffe));

    expect(6, 11,
        list_of<Mask>(0x0006, 0xfffe)(0x0008, 0xfffc));

    expect(6, 15,
        list_of<Mask>(0x0006, 0xfffe)(0x0008, 0xfff8));

    expect(0x001a, 0x00e9,
        list_of<Mask>(0x001a, 0xfffe)(0x001c, 0xfffc)
            (0x0020, 0xffe0)(0x0040, 0xffc0) (0x0080, 0xffc0)(0x00c0, 0xffe0)
            (0x00e0, 0xfff8)(0x00e8, 0xfffe));

    expect(1000, 1999,
        list_of<Mask>(0x03e8, 0xfff8)(0x03f0, 0xfff0)
            (0x0400, 0xfe00)(0x0600, 0xff00)(0x0700, 0xff80)(0x0780, 0xffc0)
            (0x07c0, 0xfff0));
}

BOOST_AUTO_TEST_SUITE_END()
