/*
 * Implementation of IdGenerator class
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <fstream>
#include <algorithm>

#include <opflexagent/IdGenerator.h>
#include <opflexagent/logging.h>

namespace opflexagent {

using std::string;
using std::lock_guard;
using std::mutex;
typedef uint16_t UriLenType;
const uint32_t MAX_ID_VALUE = (1 << 31);

IdGenerator::IdGenerator() : cleanupInterval(duration(5*60*1000)) {

}

IdGenerator::IdGenerator(duration cleanupInterval_)
    : cleanupInterval(cleanupInterval_) {

}

void IdGenerator::setAllocHook(const std::string& nmspc,
                               alloc_hook_t& allocHook) {
    lock_guard<mutex> guard(id_mutex);
    NamespaceMap::iterator nitr = namespaces.find(nmspc);
    if (nitr == namespaces.end()) {
        LOG(ERROR) << "Cannot set hook for uninitialized namespace: " << nmspc;
    }

    nitr->second.allocHook = allocHook;
}

uint32_t IdGenerator::getId(const string& nmspc, const string& str) {
    lock_guard<mutex> guard(id_mutex);
    NamespaceMap::iterator nitr = namespaces.find(nmspc);
    if (nitr == namespaces.end()) {
        LOG(ERROR) << "ID requested for unknown namespace: " << nmspc;
        return -1;
    }

    IdMap& idmap = nitr->second;
    IdMap::Str2EIdMap::iterator eit = idmap.erasedIds.find(str);
    if (eit != idmap.erasedIds.end())
        idmap.erasedIds.erase(eit);

    IdMap::Str2IdMap::const_iterator it = idmap.ids.find(str);
    if (it == idmap.ids.end()) {
        if (idmap.freeIds.empty()) {
            LOG(ERROR) << "No free IDS in namespace: " << nmspc;
            return -1;
        }
        id_range start = *idmap.freeIds.begin();
        uint32_t newId = idmap.ids[str] = start.start;
        if (idmap.allocHook) {
            if (!idmap.allocHook.get()(str, newId)) {
                LOG(ERROR) << "ID allocation canceled by allocation hook";
                return -1;
            }
        }
        idmap.freeIds.erase(start);
        if (start.start < start.end)
            idmap.freeIds.insert(id_range(start.start + 1, start.end));

        idmap.reverseMap[newId] = str;

        LOG(DEBUG) << "Assigned " << nmspc << ":" << newId
            << " to id: " << str;
        persist(nmspc, idmap);

        return newId;
    }

    return it->second;
}

boost::optional<std::string>
IdGenerator::getStringForId(const std::string& nmspc, uint32_t id) {

    lock_guard<mutex> guard(id_mutex);
    NamespaceMap::iterator nitr = namespaces.find(nmspc);
    if (nitr == namespaces.end()) {
        LOG(ERROR) << "ID requested for unknown namespace: " << nmspc;
        return boost::none;
    }

    IdMap& idmap = nitr->second;

    IdMap::Id2StrMap::iterator itr = idmap.reverseMap.find(id);

    if (itr != idmap.reverseMap.end()) {
        return itr->second;
    }

    LOG(DEBUG) << "Unable to map to string for id:"
               << id << " in namespace " << nmspc;
    return boost::none;
}

void IdGenerator::erase(const string& nmspc, const string& str) {
    lock_guard<mutex> guard(id_mutex);
    NamespaceMap::iterator nitr = namespaces.find(nmspc);
    if (nitr == namespaces.end()) {
        return;
    }

    IdMap& idmap = nitr->second;
    IdMap::Str2EIdMap::const_iterator it = idmap.erasedIds.find(str);
    if (it == idmap.erasedIds.end()) {
        idmap.erasedIds[str] = std::chrono::steady_clock::now();
    }
}

uint32_t IdGenerator::getRemainingIds(const std::string& nmspc) {
    lock_guard<mutex> guard(id_mutex);
    return getRemainingIdsLocked(nmspc);
}

uint32_t IdGenerator::getFreeRangeCount(const std::string& nmspc) {
    lock_guard<mutex> guard(id_mutex);
    NamespaceMap::iterator nitr = namespaces.find(nmspc);
    if (nitr == namespaces.end()) {
        return 0;
    }

    IdMap& idmap = nitr->second;
    return idmap.freeIds.size();
}

uint32_t IdGenerator::getRemainingIdsLocked(const std::string& nmspc) {
    NamespaceMap::iterator nitr = namespaces.find(nmspc);
    if (nitr == namespaces.end()) {
        return 0;
    }

    IdMap& idmap = nitr->second;
    uint32_t remaining = 0;
    for (const id_range& r : idmap.freeIds) {
        remaining += r.end - r.start + 1;
    }
    return remaining;
}

void IdGenerator::cleanup() {
    lock_guard<mutex> guard(id_mutex);
    time_point now = std::chrono::steady_clock::now();
    for (NamespaceMap::value_type& nmv : namespaces) {
        bool changed = false;
        IdMap& idmap = nmv.second;
        IdMap::Str2EIdMap::iterator it = idmap.erasedIds.begin();
        while (it != idmap.erasedIds.end()) {
            if ((now - it->second) > cleanupInterval) {
                IdMap::Str2IdMap::iterator iit = idmap.ids.find(it->first);
                if (iit != idmap.ids.end()) {
                    uint32_t erasedId = iit->second;

                    // return erasedId to free set
                    std::set<id_range>::iterator ub =
                        std::upper_bound(idmap.freeIds.begin(),
                                         idmap.freeIds.end(),
                                         id_range(erasedId, erasedId));
                    std::set<id_range>::iterator prev;
                    if (ub != idmap.freeIds.end()) {
                        prev = ub;
                        prev--;
                    } else {
                        prev = idmap.freeIds.begin();
                    }

                    if (prev != idmap.freeIds.end() &&
                        ub != idmap.freeIds.end() &&
                        prev->end + 1 == erasedId &&
                        erasedId + 1 == ub->start) {
                        // merge prev and upper bound
                        id_range newr(prev->start, ub->end);
                        idmap.freeIds.erase(*prev);
                        idmap.freeIds.erase(*ub);
                        idmap.freeIds.insert(newr);
                    } else if (prev != idmap.freeIds.end() &&
                               prev->end + 1 == erasedId) {
                        // extend prev bound range to include erased id
                        id_range newprev(prev->start, erasedId);
                        idmap.freeIds.erase(*prev);
                        idmap.freeIds.insert(newprev);
                    } else if (ub != idmap.freeIds.end() &&
                               erasedId + 1 == ub->start) {
                        // extend ub range to include erased Id
                        id_range newub(erasedId, ub->end);
                        idmap.freeIds.erase(*ub);
                        idmap.freeIds.insert(newub);
                    } else {
                        // add new range for just this value
                        idmap.freeIds.insert(id_range(erasedId, erasedId));
                    }
                    changed = true;

                    IdMap::Id2StrMap::iterator irmt =
                        idmap.reverseMap.find(iit->second);
                    if (irmt != idmap.reverseMap.end()) {
                        idmap.reverseMap.erase(irmt);
                    }

                    idmap.ids.erase(iit);

                    LOG(DEBUG) << "Cleaned up ID " << it->first
                               << " in namespace " << nmv.first;
                }
                it = idmap.erasedIds.erase(it);
                continue;
            }
            it++;
        }
        if (changed)
            persist(nmv.first, nmv.second);

        LOG(DEBUG) << "Remaining IDs for namespace "
                   << nmv.first << ": "
                   << getRemainingIdsLocked(nmv.first)
                   << " in " << idmap.freeIds.size() << " range(s)";
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
    for (const IdMap::Str2IdMap::value_type& kv : idmap.ids) {
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

void IdGenerator::initNamespace(const std::string& nmspc,
                                uint32_t minId, uint32_t maxId) {
    lock_guard<mutex> guard(id_mutex);
    IdMap& idmap = namespaces[nmspc];
    idmap.ids.clear();
    idmap.freeIds.insert(id_range(minId, maxId));

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

    std::set<uint32_t> usedIds;

    while (!file.fail()) {
        uint32_t id;
        uint16_t len;
        if (file.read((char *)&id, sizeof(id)).eof() ||
            file.read((char *)&len, sizeof(len)).eof()) {
            break;
        }
        std::unique_ptr<string> str(new string((size_t)len, '\0'));
        if (file.read((char *)str->data(), len).eof()) {
            LOG(DEBUG) << "Unexpected EOF while reading string";
            break;
        }
        if (usedIds.find(id) != usedIds.end()) {
            LOG(WARNING) << "ID file corrupt: " << id << " seen more than once";
        } else if (id > maxId) {
            LOG(WARNING) << "ID file corrupt: " << id << " above maximum";
        } else if (id < minId) {
            LOG(WARNING) << "ID file corrupt: " << id << " below minimum";
        } else {
            idmap.ids[*str] = id;
            idmap.reverseMap[id] = *str;
        }
        usedIds.insert(id);
        LOG(DEBUG) << "Loaded str: " << *str << ", "
                   << nmspc << ":" << id;
    }
    file.close();

    uint32_t cur = minId;
    idmap.freeIds.clear();
    for (auto id : usedIds) {
        if (id > cur)
            idmap.freeIds.insert(id_range(cur, id - 1));
        cur = id + 1;
    }
    if (cur < maxId)
        idmap.freeIds.insert(id_range(cur, maxId));

    LOG(DEBUG) << "Loaded " << idmap.ids.size()
               << " entries from " << fname << " with "
               << idmap.freeIds.size() << " free range(s)";

}

void IdGenerator::collectGarbage(const std::string& ns,
                                 garbage_cb_t cb) {
    NamespaceMap::iterator nitr = namespaces.find(ns);
    if (nitr == namespaces.end()) {
        return;
    }

    IdMap& map = nitr->second;

    for (IdMap::Str2IdMap::iterator uit = map.ids.begin();
         uit != map.ids.end(); uit++) {
        if (cb(ns, uit->first))
            continue;

        IdMap::Str2EIdMap::const_iterator it = map.erasedIds.find(uit->first);
        if (it == map.erasedIds.end()) {
            map.erasedIds[uit->first] = std::chrono::steady_clock::now();
            LOG(DEBUG) << "Found garbage " << uit->first << " in " << ns;
        }
    }
}

bool operator<(const IdGenerator::id_range& lhs,
               const IdGenerator::id_range& rhs) {
    return lhs.start < rhs.start;
}
bool operator==(const IdGenerator::id_range& lhs,
                const IdGenerator::id_range& rhs) {
    return lhs.start == rhs.start && lhs.end == rhs.end;
}
bool operator!=(const IdGenerator::id_range& lhs,
                const IdGenerator::id_range& rhs) {
    return !(lhs == rhs);
}

} // namespace opflexagent
