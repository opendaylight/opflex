/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for LearningBridgeManager class.
 *
 * Copyright (c) 2018 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <opflexagent/LearningBridgeManager.h>
#include <opflexagent/logging.h>

#include <algorithm>

namespace opflexagent {

using std::unique_lock;
using std::mutex;

LearningBridgeManager::LearningBridgeManager() {

}

void LearningBridgeManager::registerListener(LearningBridgeListener* listener) {
    unique_lock<mutex> guard(listener_mutex);
    lbListeners.push_back(listener);
}

void LearningBridgeManager::
unregisterListener(LearningBridgeListener* listener) {
    unique_lock<mutex> guard(listener_mutex);
    lbListeners.remove(listener);
}

void LearningBridgeManager::notifyListeners(const std::string& uuid) {
    unique_lock<mutex> guard(listener_mutex);
    for (LearningBridgeListener* listener : lbListeners) {
        listener->lbIfaceUpdated(uuid);
    }
}

void LearningBridgeManager::notifyListeners(const range_set_t& notify) {
    unique_lock<mutex> guard(listener_mutex);
    for (LearningBridgeListener* listener : lbListeners) {
        for (auto& r : notify) {
            listener->lbVlanUpdated(r);
        }
    }
}

std::shared_ptr<const LearningBridgeIface>
LearningBridgeManager::getLBIface(const std::string& uuid) {
    unique_lock<mutex> guard(iface_mutex);
    auto it = lbi_map.find(uuid);
    if (it != lbi_map.end())
        return it->second.iface;
    return std::shared_ptr<const LearningBridgeIface>();
}

void LearningBridgeManager::removeIfaces(const LearningBridgeIface& iface) {
    if (iface.getInterfaceName()) {
        auto it = iface_lbi_map.find(iface.getInterfaceName().get());
        if (it != iface_lbi_map.end()) {
            std::unordered_set<std::string>& ifaces = it->second;
            ifaces.erase(iface.getUUID());

            if (ifaces.size() == 0) {
                iface_lbi_map.erase(it);
            }
        }
    }
}

void LearningBridgeManager::removeVlans(const LearningBridgeIface& iface,
                                        range_set_t& notify) {
    const std::string& uuid = iface.getUUID();

    for (auto r : iface.getTrunkVlans()) {
        auto it = range_lbi_map.find(r);

        while (it != range_lbi_map.end()) {
            if (r.second < it->first.second)
                break;

            notify.insert(it->first);
            it->second.erase(uuid);
            if (it->second.empty()) {
                it = range_lbi_map.erase(it);
            } else {
                it++;
            }
        }
    }
}

void LearningBridgeManager::addVlans(const LearningBridgeIface& iface,
                                     range_set_t& notify) {
    const std::string& uuid = iface.getUUID();

    for (auto r : iface.getTrunkVlans()) {
        // find all the ranges superceded by this update and replace
        // as needed
        auto lit = range_lbi_map.lower_bound(r);
        bool range_valid = true;

        // go back one element if possible to check if preceding range
        // overlaps.
        if (lit != range_lbi_map.begin())
            lit--;

        std::unordered_set<std::string> single {uuid};
        while (lit != range_lbi_map.end()) {
            if (r.first <= lit->first.first) {
                if (r.second < lit->first.first) {
                    // r ----
                    // l     ----

                    // [r.first, r.second]
                    range_lbi_map.emplace(r, single);
                    notify.insert(r);

                    // done
                    range_valid = false;
                    break;
                } else {
                    // [r first, lit first) (if nonzero size)
                    if (r.first < lit->first.first) {
                        vlan_range_t nr1(r.first, lit->first.first - 1);
                        range_lbi_map.insert(lit, make_pair(nr1, single));
                        notify.insert(nr1);
                    }

                    if (lit->first.second <= r.second) {
                        // r ------
                        // l  ----
                        // [r first, lit first) updated above

                        // [lit first, lit second]
                        lit->second.insert(uuid);
                        notify.insert(lit->first);

                        // continue with:
                        // (lit second, r second] (if nonzero size)
                        if (r.second > lit->first.second) {
                            r.first = lit->first.second + 1;
                            lit++;
                        } else {
                            range_valid = false;
                            break;
                        }
                    } else {
                        // r ----
                        // l   ----
                        // [r first, lit first) updated above

                        auto uuids = lit->second;
                        auto uuidsplus = lit->second;
                        uuidsplus.insert(uuid);

                        vlan_range_t l = lit->first;
                        lit = range_lbi_map.erase(lit);
                        notify.insert(l);

                        // [lit first, r second]
                        {
                            vlan_range_t nr2(l.first, r.second);
                            range_lbi_map.insert(lit,
                                                 make_pair(nr2, uuidsplus));
                            notify.insert(nr2);
                        }

                        // (r second, lit second] (if nonzero size)
                        if (r.second < l.second) {
                            vlan_range_t nr3(r.second + 1, l.second);
                            range_lbi_map.insert(lit, make_pair(nr3, uuids));
                            notify.insert(nr3);
                        }

                        // done
                        range_valid = false;
                        break;
                    }
                }
            } else {
                if (lit->first.second < r.first) {
                    // r     ----
                    // l ----
                    //
                    // no updates
                    // continue with:
                    // [r.first, r.second]
                    lit++;
                } else {
                    auto uuids = lit->second;
                    auto uuidsplus = lit->second;
                    uuidsplus.insert(uuid);

                    vlan_range_t l = lit->first;

                    if (lit->first.second <= r.second) {
                        // r   ----
                        // l ----

                        // [lit first, r first) (if nonzero size)
                        if (l.first < r.first) {
                            vlan_range_t nr1(l.first, r.first - 1);
                            lit = range_lbi_map.erase(lit);
                            notify.insert(l);
                            range_lbi_map.emplace(make_pair(nr1, uuids));
                            notify.insert(nr1);
                        }

                        // [r first, lit second]
                        {
                            vlan_range_t nr2(r.first, l.second);
                            range_lbi_map.emplace(make_pair(nr2, uuidsplus));
                            notify.insert(nr2);
                        }

                        // continue with:
                        // (lit second, r second] (if nonzero size)
                        if (r.second > l.second) {
                            r.first = l.second + 1;
                        } else {
                            range_valid = false;
                            break;
                        }
                    } else {
                        // r  ----
                        // l ------

                        // [lit first, r first) (if nonzero size)
                        if (l.first < r.first) {
                            vlan_range_t nr1(l.first, r.first - 1);
                            // overwrites [lit first, lit second]
                            notify.insert(lit->first);
                            range_lbi_map.erase(lit);
                            range_lbi_map.emplace(make_pair(nr1, uuids));
                            notify.insert(nr1);
                        }

                        // [r first, r second]
                        range_lbi_map.emplace(make_pair(r, uuidsplus));
                        notify.insert(r);

                        // (r second, lit second] (if nonzero size)
                        if (r.second < l.second) {
                            vlan_range_t nr3(r.second + 1, l.second);
                            range_lbi_map.emplace(make_pair(nr3, uuids));
                            notify.insert(nr3);
                        }

                        // done
                        range_valid = false;
                        break;
                    }
                }
            }
        }
        // add any residual range value
        if (range_valid) {
            range_lbi_map.emplace(r, single);
            notify.insert(r);
        }
    }
}

void LearningBridgeManager::updateLBIface(const LearningBridgeIface& iface) {
    unique_lock<mutex> guard(iface_mutex);
    const std::string& uuid = iface.getUUID();
    LBIfaceState& lbi = lbi_map[uuid];
    range_set_t range_notify;

    // update interface name to iface mapping
    if (lbi.iface &&
        lbi.iface->getInterfaceName() != iface.getInterfaceName()) {
        removeIfaces(*lbi.iface);
    }
    if (iface.getInterfaceName()) {
        iface_lbi_map[iface.getInterfaceName().get()].insert(uuid);
    }

    // update VLAN to iface mapping
    if (lbi.iface &&
        lbi.iface->getTrunkVlans() != iface.getTrunkVlans()) {
        removeVlans(*lbi.iface, range_notify);
    }
    addVlans(iface, range_notify);

    lbi.iface = std::make_shared<const LearningBridgeIface>(iface);

    guard.unlock();
    notifyListeners(uuid);
    notifyListeners(range_notify);
}

void LearningBridgeManager::removeLBIface(const std::string& uuid) {
    unique_lock<mutex> guard(iface_mutex);
    range_set_t range_notify;

    auto it = lbi_map.find(uuid);
    if (it != lbi_map.end()) {
        // update interface name to iface mapping
        removeIfaces(*it->second.iface);
        // update VLAN to iface mapping
        removeVlans(*it->second.iface, range_notify);
        lbi_map.erase(it);
    }

    guard.unlock();
    notifyListeners(uuid);
    notifyListeners(range_notify);
}

void LearningBridgeManager::
getLBIfaceByIface(const std::string& ifaceName,
                  /* out */ std::unordered_set<std::string>& lbi) {
    unique_lock<mutex> guard(iface_mutex);

    auto it = iface_lbi_map.find(ifaceName);
    if (it != iface_lbi_map.end()) {
        lbi.insert(it->second.begin(), it->second.end());
    }
}

void LearningBridgeManager::
getIfacesByVlanRange(vlan_range_t range,
                     /* out */ std::unordered_set<std::string>& lbi) {
    unique_lock<mutex> guard(iface_mutex);

    auto it = range_lbi_map.find(range);
    if (it != range_lbi_map.end() && it->first.second == range.second) {
        lbi.insert(it->second.begin(), it->second.end());
    }
}

void LearningBridgeManager::
getVlanRangesByIface(const std::string& uuid,
                     /* out */ std::set<vlan_range_t> ranges) {
    unique_lock<mutex> guard(iface_mutex);
    auto it = lbi_map.find(uuid);
    if (it == lbi_map.end()) return;

    for (auto r : it->second.iface->getTrunkVlans()) {
        auto it = range_lbi_map.find(r);

        while (it != range_lbi_map.end()) {
            if (r.second < it->first.second)
                break;

            ranges.insert(it->first);
        }
    }
}

void LearningBridgeManager::forEachVlanRange(const vlanCb& func) {
    unique_lock<mutex> guard(iface_mutex);
    for (auto& r : range_lbi_map) {
        func(r.first, r.second);
    }
}

} /* namespace opflexagent */
