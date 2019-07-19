/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for URI class.
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


#include <cctype>
#include <cstdlib>

#include <boost/algorithm/string/split.hpp>
#if __cplusplus <= 199711L
#include <boost/make_shared.hpp>
#endif

#include "opflex/modb/URI.h"

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

const URI URI::ROOT("/");

URI::URI(const OF_SHARED_PTR<const std::string>& uri_)
    : uri(uri_) {
    hashv = 0;
    boost::hash_combine(hashv, *uri);
}

URI::URI(const std::string& uri_) {
    uri = OF_MAKE_SHARED<const std::string>(uri_);

    hashv = 0;
    boost::hash_combine(hashv, uri_);
}

URI::URI(const URI& uri_)
    : uri(uri_.uri) {
    hashv = uri_.hashv;
}

URI::~URI() {
}

std::ostream & operator<<(std::ostream &os, const URI& uri) {
    os << uri.toString();
    return os;
}

const std::string& URI::toString() const {
    return *uri;
}

typedef split_iterator<string::const_iterator> string_split_iter;

enum UState {
    BEGIN,
    P1,
    P2
};

void URI::getElements(/* out */ vector<string>& elements) const {
    char p[3];
    p[2] = '\0';

    for(string_split_iter it =
        make_split_iterator(*uri, first_finder("/", is_iequal()));
        it != string_split_iter();
        ++it) {
        UState state = BEGIN;
        bool found = false;
        stringstream estream;

        for (string::const_iterator segment = it->begin(); 
             segment != it->end(); ++segment) {
            char c = *segment;
            switch (state) {
            case BEGIN:
                if (c == '%') {
                    state = P1;
                } else {
                    estream << c;
                    found = true;
                }
                break;
            case P1:
                p[0] = c;
                state = P2;
                break;
            case P2:
                p[1] = c;
                state = BEGIN;
                if (isxdigit(p[0]) && isxdigit(p[1])) {
                    estream << (char)strtol(p, NULL, 16);
                    found = true;
                }
                break;
            }
        }
        if (found)
            elements.push_back(estream.str());
    }
}

URI& URI::operator=(const URI& rhs) {
    uri = rhs.uri;
    hashv = rhs.hashv;
    return *this;
}

bool operator==(const URI& lhs, const URI& rhs) {
    return *lhs.uri == *rhs.uri;
}
bool operator!=(const URI& lhs, const URI& rhs) {
    return !operator==(lhs,rhs);
}

bool operator<(const URI& lhs, const URI& rhs) {
    return *lhs.uri < *rhs.uri;
}

size_t hash_value(URI const& uri) {
    return uri.hashv;
}

} /* namespace modb */
} /* namespace opflex */

#if __cplusplus > 199711L

namespace std {
std::size_t
hash<opflex::modb::URI>::operator()(const opflex::modb::URI& u) const {
    return opflex::modb::hash_value(u);
}
} /* namespace std */

#endif
