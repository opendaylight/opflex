/*
 * Implementation of IdGenerator class
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <fstream>

#include <boost/scoped_ptr.hpp>
#include <boost/foreach.hpp>
#include <boost/thread/lock_guard.hpp>

#include "IdGenerator.h"
#include "logging.h"

namespace ovsagent {

using std::string;
using boost::lock_guard;
using boost::mutex;
typedef uint16_t UriLenType;
const uint32_t MAX_ID_VALUE = (1 << 31);

IdGenerator::IdGenerator() : cleanupInterval(duration(5*60*1000)) {

}

IdGenerator::IdGenerator(duration cleanupInterval_)
    : cleanupInterval(cleanupInterval_) {

}

uint32_t IdGenerator::getId(const string& nmspc, const string& str) {
    lock_guard<mutex> guard(id_mutex);
    NamespaceMap::iterator nitr = namespaces.find(nmspc);
    if (nitr == namespaces.end()) {
        LOG(ERROR) << "ID requested for unknown namespace: " << nmspc;
        return 0;
    }

    IdMap& idmap = nitr->second;
    IdMap::Str2IdMap::const_iterator it = idmap.ids.find(str);
    if (it == idmap.ids.end()) {
        if (++idmap.lastUsedId == MAX_ID_VALUE) {
            LOG(ERROR) << "ID overflow in namespace: " << nmspc;
            return -1;
        }
        idmap.ids[str] = idmap.lastUsedId;
        LOG(DEBUG) << "Assigned " << nmspc << ":" << idmap.lastUsedId
            << " to id: " << str;
        persist(nmspc, idmap);
        return idmap.lastUsedId;
    }

    IdMap::Str2EIdMap::iterator eit = idmap.erasedIds.find(str);
    if (eit != idmap.erasedIds.end())
        idmap.erasedIds.erase(eit);

    return it->second;
}

void IdGenerator::erase(const string& nmspc, const string& str) {
    lock_guard<mutex> guard(id_mutex);
    NamespaceMap::iterator nitr = namespaces.find(nmspc);
    if (nitr == namespaces.end()) {
        return;
    }
    // XXX Need a way to reclaim IDs not in use
    IdMap& idmap = nitr->second;
    IdMap::Str2EIdMap::const_iterator it = idmap.erasedIds.find(str);
    if (it == idmap.erasedIds.end()) {
        idmap.erasedIds[str] = boost::chrono::steady_clock::now();
    }
}

void IdGenerator::cleanup() {
    lock_guard<mutex> guard(id_mutex);
    time_point now = boost::chrono::steady_clock::now();
    BOOST_FOREACH(NamespaceMap::value_type& nmv, namespaces) {
        bool changed = false;
        IdMap& map = nmv.second;
        IdMap::Str2EIdMap::iterator it = map.erasedIds.begin();
        while (it != map.erasedIds.end()) {
            if ((now - it->second) > cleanupInterval) {
                if (map.ids.erase(it->first) > 0) {
                    changed = true;

                    LOG(DEBUG) << "Cleaned up ID " << it->first
                               << " in namespace " << nmv.first;
                }
                it = map.erasedIds.erase(it);
                continue;
            }
            it++;
        }
        if (changed)
            persist(nmv.first, nmv.second);
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
    std::ofstream file(fname.c_str(), std::ios_base::binary);
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
    BOOST_FOREACH (const IdMap::Str2IdMap::value_type& kv, idmap.ids) {
        const uint32_t& id = kv.second;
        const string& str = kv.first;
        if (str.size() > UINT16_MAX) {
            LOG(ERROR) << "ID string length exceeds maximum";
            continue;
        }
        uint16_t len = str.size();

        if (file.write((const char *)&id, sizeof(id)).fail() ||
            file.write((const char *)&len, sizeof(len)).fail() ||
            file.write(str.c_str(), len).fail()) {
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
    std::ifstream file(fname.c_str(), std::ios_base::binary);
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
        uint16_t len;
        if (file.read((char *)&id, sizeof(id)).eof() ||
            file.read((char *)&len, sizeof(len)).eof()) {
            LOG(DEBUG) << "Got EOF, loaded " << idmap.ids.size()
                << " entries from " << fname << "; last used id="
                << idmap.lastUsedId;
            break;
        }
        boost::scoped_ptr<string> str(new string((size_t)len, '\0'));
        if (file.read((char *)str->data(), len).eof()) {
            LOG(DEBUG) << "Unexpected EOF while reading string";
            break;
        }
        idmap.ids[*str] = id;
        idmap.lastUsedId = std::max(idmap.lastUsedId, id);
        LOG(DEBUG) << "Loaded str: " << str << ", "
                   << nmspc << ":" << id;
    }
    file.close();
}

void IdGenerator::collectGarbage(const std::string& ns,
                                 garbage_cb_t cb) {
    NamespaceMap::iterator nitr = namespaces.find(ns);
    if (nitr == namespaces.end()) {
        return;
    }

    IdMap& map = nitr->second;
    uint32_t max_id = 0;

    for (IdMap::Str2IdMap::iterator uit = map.ids.begin();
         uit != map.ids.end(); uit++) {
        max_id = std::max(max_id, uit->second);
        if (cb(ns, uit->first))
            continue;

        IdMap::Str2EIdMap::const_iterator it = map.erasedIds.find(uit->first);
        if (it == map.erasedIds.end()) {
            map.erasedIds[uit->first] = boost::chrono::steady_clock::now();
            LOG(DEBUG) << "Found garbage " << uit->first << " in " << ns;
        }
    }

    // lastUsedId could still creep up indefinitely, which after an
    // extremely long time could become a problem.  To be really
    // bulletproof we'd need to maintain a free list to allow reuse of
    // IDs.
    LOG(DEBUG) << "Maximum ID for namespace " << ns << ": " << max_id;
    map.lastUsedId = max_id;
}

} // namespace ovsagent

