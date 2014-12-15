/*
 * Implementation of IdGenerator class
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <fstream>
#include <boost/shared_ptr.hpp>
#include <boost/foreach.hpp>
#include "IdGenerator.h"
#include "logging.h"

using namespace std;
using namespace boost;
using namespace opflex::modb;

namespace ovsagent {

typedef uint16_t UriLenType;
const uint32_t MAX_ID_VALUE = (1 << 31);

uint32_t IdGenerator::getId(const string& nmspc, const URI& uri) {
    NamespaceMap::iterator nitr = namespaces.find(nmspc);
    if (nitr == namespaces.end()) {
        LOG(ERROR) << "ID requested for unknown namespace: " << nmspc;
        return 0;
    }

    IdMap& idmap = nitr->second;
    IdMap::Uri2IdMap::const_iterator it = idmap.ids.find(uri);
    if (it == idmap.ids.end()) {
        if (++idmap.lastUsedId == MAX_ID_VALUE) {
            LOG(ERROR) << "ID overflow in namespace: " << nmspc;
            return -1;
        }
        idmap.ids[uri] = idmap.lastUsedId;
        LOG(DEBUG) << "Assigned " << nmspc << ":" << idmap.lastUsedId
            << " to URI: " << uri;
        persist(nmspc, idmap);
        return idmap.lastUsedId;
    }
    return it->second;
}

void IdGenerator::erase(const string& nmspc, const URI& uri) {
    NamespaceMap::iterator nitr = namespaces.find(nmspc);
    if (nitr == namespaces.end()) {
        return;
    }
    // XXX Need a way to reclaim IDs not in use
    IdMap& idmap = nitr->second;
    if (idmap.ids.erase(uri) > 0) {
        persist(nmspc, idmap);
    }
}

string IdGenerator::getNamespaceFile(const string& nmspc) {
    assert(!persistDir.empty());
    return persistDir + "/" + nmspc + ".id";
}

void IdGenerator::persist(const std::string& nmspc, IdMap& idmap) {
    if (persistDir.empty()) {
        return;
    }

    string fname = getNamespaceFile(nmspc);
    ofstream file(fname.c_str(), ios_base::binary);
    if (!file.is_open()) {
        LOG(ERROR) << "Unable to open file " << fname << " for writing";
        return;
    }
    uint32_t formatVersion = 0x1;
    if (file.write("opflexid", 8).fail() ||
        file.write((char*)&formatVersion, sizeof(formatVersion)).fail()) {
        LOG(ERROR) << "Failed to write to file: " << fname;
        return;
    }
    BOOST_FOREACH (const IdMap::Uri2IdMap::value_type& kv, idmap.ids) {
        const uint32_t& id = kv.second;
        const URI& uri = kv.first;
        UriLenType len = uri.toString().size();
        if (file.write((const char *)&id, sizeof(id)).fail() ||
            file.write((const char *)&len, sizeof(len)).fail() ||
            file.write(uri.toString().c_str(), len).fail()) {
            LOG(ERROR) << "Failed to write to file: " << fname;
            break;
        }
    }
    file.close();
    LOG(DEBUG) << "Wrote " << idmap.ids.size() << " entries to file " << fname;
}

void IdGenerator::initNamespace(const std::string& nmspc) {
    IdMap& idmap = namespaces[nmspc];
    idmap.ids.clear();

    if (persistDir.empty()) {
        return;
    }
    string fname = getNamespaceFile(nmspc);
    LOG(DEBUG) << "Loading IDs from file " << fname;
    ifstream file(fname.c_str(), ios_base::binary);
    if (!file.is_open()) {
        LOG(DEBUG) << "Unable to open file " << fname << " for reading";
        return;
    }
    idmap.lastUsedId = 0;

    char magic[8];
    uint32_t formatVersion;
    if (file.read(magic, sizeof(magic)).eof() ||
        file.read((char*)&formatVersion, sizeof(formatVersion)).eof()) {
        LOG(ERROR) << fname << " exists, but could not be read";
        return;
    }
    if (0 != strncmp(magic, "opflexid", sizeof(magic))) {
        LOG(ERROR) << fname << " is not an ID file";
        return;
    }
    if (formatVersion != 1) {
        LOG(ERROR) << fname << ": Unsupported ID file format version: "
                   << formatVersion;
        return;
    }
    while (!file.fail()) {
        uint32_t id;
        UriLenType len;
        if (file.read((char *)&id, sizeof(id)).eof() ||
            file.read((char *)&len, sizeof(len)).eof()) {
            LOG(INFO) << "Got EOF, loaded " << idmap.ids.size()
                << " entries from " << fname << "; last used id="
                << idmap.lastUsedId;
            break;
        }
        shared_ptr<string> uriStr(new string((size_t)len, '\0'));
        if (file.read((char *)uriStr->data(), len).eof()) {
            LOG(DEBUG) << "Unexpected EOF while reading URI";
            break;
        }
        idmap.ids[URI(uriStr)] = id;
        idmap.lastUsedId = max(idmap.lastUsedId, id);
        LOG(DEBUG) << "Loaded URI: " << uriStr->c_str() << ", "
            << nmspc << ":" << id;
    }
    file.close();
}

} // namespace ovsagent

