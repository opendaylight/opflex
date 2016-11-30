/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for URIBuilder class.
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


#include <sstream>
#include <string>
#include <cctype>
#include <iomanip>

#include "opflex/modb/URIBuilder.h"

namespace opflex {
namespace modb {

using std::stringstream;
using std::ostream;
using std::string;

class URIBuilder::URIBuilderImpl {
public:
    URIBuilderImpl() { 
        uri_stream.fill('0');
        uri_stream << '/';
    }

    URIBuilderImpl(const URI& uri) { 
        uri_stream.fill('0');
        uri_stream << uri.toString();
    }

    stringstream uri_stream;
};

URIBuilder::URIBuilder() : pimpl(new URIBuilderImpl()) {

}

URIBuilder::URIBuilder(const URI& uri) : pimpl(new URIBuilderImpl(uri)) {

}

URIBuilder::~URIBuilder() {
    delete pimpl;
}

static void writeStringEscape(ostream& stream, const string& str) {
    string::const_iterator i;
    stream << std::hex;
    for (i = str.begin(); i != str.end(); ++i) {
        string::value_type c = *i;
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            stream << c;
            continue;
        }

        stream << '%' << std::setw(2) << int((unsigned char) c);
    }
    stream << std::dec;
}

URIBuilder& URIBuilder::addElement(uint64_t elementValue) {
    pimpl->uri_stream << elementValue << '/';
    return *this;
}

URIBuilder& URIBuilder::addElement(int64_t elementValue) {
    pimpl->uri_stream << elementValue << '/';
    return *this;
}

URIBuilder& URIBuilder::addElement(uint32_t elementValue) {
    pimpl->uri_stream << elementValue << '/';
    return *this;
}

URIBuilder& URIBuilder::addElement(int32_t elementValue) {
    pimpl->uri_stream << elementValue << '/';
    return *this;
}

URIBuilder& URIBuilder::addElement(const string& elementValue) {
    writeStringEscape(pimpl->uri_stream, elementValue);
    pimpl->uri_stream << '/';
    return *this;
}

URIBuilder& URIBuilder::addElement(const MAC& elementValue) {
    return addElement(elementValue.toString());
}

URIBuilder& URIBuilder::addElement(const URI& elementValue) {
    return addElement(elementValue.toString());
}

modb::URI URIBuilder::build() {
    return modb::URI(pimpl->uri_stream.str());
}

} /* namespace modb */
} /* namespace opflex */
