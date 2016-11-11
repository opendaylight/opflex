/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for MAC class.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

/* This must be included before anything else */
#if HAVE_CONFIG_H
#  include <config.h>
#endif


#include <arpa/inet.h>
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <iomanip>
#include <iostream>

#include <boost/make_shared.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/functional/hash.hpp>

#include "opflex/modb/MAC.h"

namespace opflex {
namespace modb {

using std::string;
using std::vector;
using std::stringstream;
using boost::algorithm::is_iequal;
using boost::algorithm::split_iterator;
using boost::algorithm::make_split_iterator;
using boost::algorithm::first_finder;
using boost::iterator_range;
using boost::copy_range;

static uint64_t parseMac(const string& macstr) {
    uint64_t result = 0;
    uint8_t* r8 = (uint8_t*)&result;
    int r[6];
    if (std::sscanf(macstr.c_str(),
                   "%02x:%02x:%02x:%02x:%02x:%02x",
                    r, r+1, r+2, r+3, r+4, r+5) != 6) {
        throw std::invalid_argument(macstr + " is not a valid MAC address");
    }

    for (int i = 0; i < 6; ++i) {
        r8[i] = (uint8_t)r[i];
    }
    return result;
}

MAC::MAC() : mac(0) { }

MAC::MAC(const std::string& mac_) 
    : mac(parseMac(mac_)) {
}

MAC::MAC(uint64_t mac_) {
    if (htonl(1) == 1) {
        mac = (mac_ << 16);
    } else {
        uint32_t* mac32_ = (uint32_t*)&mac_;
        uint32_t* mac32 = (uint32_t*)&mac;
        mac32[0] = htonl(mac32_[1]);
        mac32[1] = htonl(mac32_[0]);
        mac = (mac >> 16);
    }
}

MAC::MAC(uint8_t mac_[6]) : mac(0) {
    uint8_t* m8 = (uint8_t*)&mac;
    for(int i = 0; i < 6; ++i) {
        m8[i] = mac_[i];
    }
}

MAC::~MAC() {
}

std::ostream & operator<<(std::ostream &os, const MAC& mac) {
    return os << mac.toString();
}

std::string MAC::toString() const {
    std::stringstream os;
    os.fill('0');
    os << std::hex;
    bool first = true;
    uint8_t* m8 = (uint8_t*)&mac;
    for (int i = 0; i < 6; ++i) {
        if (first) first = false;
        else os << ":";
        os << std::setw(2) << (int)m8[i];
    }
    return os.str();
}

void MAC::toUIntArray(/* out */uint8_t mac_[6]) const {
    uint8_t* m8 = (uint8_t*)&mac;
    for (int i = 0; i < 6; ++i) {
        mac_[i] = m8[i];
    }
}

bool operator==(const MAC& lhs, const MAC& rhs) {
    return lhs.mac == rhs.mac;
}
bool operator!=(const MAC& lhs, const MAC& rhs) {
    return !operator==(lhs,rhs);
}
size_t hash_value(MAC const& mac) {
    size_t hashv = 0;
    boost::hash_combine(hashv, mac.mac);
    return hashv;
}

} /* namespace modb */
} /* namespace opflex */
