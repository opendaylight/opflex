/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <boost/foreach.hpp>
#include <internal/modb.h>
#include <internal/model.h>
#include <inventory.h>

using namespace std;
using namespace opflex::modb;
using namespace opflex::model;

namespace opflex {
namespace enforcer {

static boost::unordered_map<class_id_t, unsigned int> classSortOrder;

bool operator==(const ChangeInfo& lhs, const ChangeInfo& rhs) {
    return lhs.seqId == rhs.seqId &&
           lhs.changeType == rhs.changeType &&
           lhs.currObject == rhs.currObject &&
           lhs.oldObject == rhs.oldObject;
}

size_t hash_value(const ChangeInfo& chgInfo) {
    std::size_t seed = 0;
    boost::hash_combine(seed, chgInfo.seqId);
    boost::hash_combine(seed, chgInfo.changeType);
    boost::hash_combine(seed, chgInfo.GetUri());
    return seed;
}

bool operator<(const ChangeInfo& lhs, const ChangeInfo& rhs)
{
    if (lhs.seqId != rhs.seqId) {
        return lhs.seqId < rhs.seqId;
    }
    class_id_t lhsClass = lhs.GetObject()->GetClass();
    class_id_t rhsClass = rhs.GetObject()->GetClass();

    assert(classSortOrder.find(lhsClass) != classSortOrder.end());
    assert(classSortOrder.find(rhsClass) != classSortOrder.end());

    if (lhsClass != rhsClass) {
        return classSortOrder[lhsClass] < classSortOrder[rhsClass];
    }
    if (lhs.GetUri() != rhs.GetUri()) {
        return lhs.GetUri().toString() < rhs.GetUri().toString();
    }
    return lhs.changeType < rhs.changeType;
}

void Inventory::Start()
{
    classSortOrder[CLASS_ID_L3CTX] = 0;
    classSortOrder[CLASS_ID_BD] = 1;
    classSortOrder[CLASS_ID_FD] = 2;
    classSortOrder[CLASS_ID_SUBNET] = 3;
    classSortOrder[CLASS_ID_ENDPOINT_GROUP] = 4;
    classSortOrder[CLASS_ID_CONDITION_GROUP] = 5;
    classSortOrder[CLASS_ID_ENDPOINT] = 6;
    classSortOrder[CLASS_ID_POLICY] = 7;
}

void Inventory::Stop()
{
}

void
Inventory::Update(modb::class_id_t cid, const modb::URI& uri) {
    // lock queue
    updates.push_back(std::make_pair(cid, uri));
    // unlock queue

    // TODO should be on different thread
    HandleUpdate();
}

ChangeInfo::TYPE
Inventory::ResolveUpdate(const UpdateInfo& updInfo, ObjectMapItr& oldObj,
        modb::MoBasePtr& newObj)
{
    const URI& uri = updInfo.second;
    newObj = opflex::modb::Find(uri);
    assert(newObj == NULL || newObj->GetUri() == uri);

    MoBasePtr obj;
    oldObj = objects.find(uri);
    if (oldObj != objects.end()) {
        obj = oldObj->second;
    }

    if (obj != NULL && newObj != NULL)  return ChangeInfo::MOD;
    if (obj == NULL && newObj != NULL)  return ChangeInfo::ADD;
    if (obj != NULL && newObj == NULL)  return ChangeInfo::REMOVE;
    return ChangeInfo::NOOP;
}

void
Inventory::RemoveBackLink(BackLinkMap& map, const ObjectKey& key,
        const ObjectKey& valueKey) {
    BackLinkMap::iterator itr = map.find(key);
    if (itr == map.end()) {
        return;
    }
    itr->second.erase(valueKey);
    if (itr->second.empty()) {
        map.erase(itr);
    }
}

void
Inventory::HandleUpdate() {
    // lock queue
    list<UpdateInfo> upds;
    updates.swap(upds);
    // unlock queue

    ChangeSet affSet;

    BOOST_FOREACH(UpdateInfo& ui, upds) {
        ChangeInfo changeInfo(++changeInfoSeqCounter);
        UpdateRelationMaps(ui, changeInfo);
        if (changeInfo.changeType != ChangeInfo::NOOP) {
            affSet.insert(changeInfo);

            FindOtherAffectedObjects(changeInfo, affSet);
        }
    }

    // Convert the object set to a list based on topological sort and change-type
    vector<ChangeInfo> tmpCl(affSet.begin(), affSet.end());
    std::sort(tmpCl.begin(), tmpCl.end());
    changeInfoOut.insert(changeInfoOut.end(), tmpCl.begin(), tmpCl.end());
}

void
Inventory::UpdateRelationMaps(const UpdateInfo& updInfo, ChangeInfo& chgInfo)
{
    ObjectMapItr currObjItr;
    modb::MoBasePtr newObj;

    chgInfo.changeType = ResolveUpdate(updInfo, currObjItr, newObj);
    if (chgInfo.changeType == ChangeInfo::NOOP) {
        return;
    }

    /*
     * Make changes to object-map except for REMOVE - the entry is erased after
     * other relations of the object to be removed have been processed.
     */
    if (chgInfo.changeType == ChangeInfo::ADD) {
        objects[updInfo.second] = newObj;
    } else {
        chgInfo.oldObject = currObjItr->second;
        if (chgInfo.changeType != ChangeInfo::REMOVE) {
            currObjItr->second = newObj;
        }
    }
    chgInfo.currObject =
            chgInfo.changeType == ChangeInfo::REMOVE ? currObjItr->second : newObj;

    switch (updInfo.first) {
    case model::CLASS_ID_ENDPOINT:
        UpdateEndpointRelations(chgInfo);
        break;
    case model::CLASS_ID_ENDPOINT_GROUP:
        UpdateEndpointGroupRelations(chgInfo);
        break;
    case model::CLASS_ID_BD:
    case model::CLASS_ID_FD:
    case model::CLASS_ID_SUBNET:
        UpdateNetworkDomainRelations(chgInfo);
        break;
    case model::CLASS_ID_POLICY:
        UpdatePolicyRelations(chgInfo);
    }

    if (chgInfo.changeType == ChangeInfo::REMOVE) {
        objects.erase(currObjItr);
    }
}

void
Inventory::UpdateRelation(const ChangeInfo& chgInfo,
        const std::string& relPropName,
        BackLinkMap& backMap)
{
    assert(chgInfo.changeType != ChangeInfo::NOOP);
    assert(!relPropName.empty());

    const MoBasePtr& currObj = chgInfo.currObject;
    const MoBasePtr& oldObj = chgInfo.oldObject;

    const URI& uri = currObj->GetUri();
    URI relPropUri(currObj->Get(relPropName));

    if (chgInfo.changeType == ChangeInfo::ADD ||
        chgInfo.changeType == ChangeInfo::MOD) {
        backMap[relPropUri].insert(uri);
    } else {
        RemoveBackLink(backMap, relPropUri, uri);
    }

    if (chgInfo.changeType == ChangeInfo::MOD) {
        URI oldRelPropUri(oldObj->Get(relPropName));
        if (oldRelPropUri != relPropUri) {
            RemoveBackLink(backMap, oldRelPropUri, uri);
        }
    }
}

void
Inventory::UpdateEndpointRelations(const ChangeInfo& chgInfo)
{
    UpdateRelation(chgInfo, "epgUri", epgToEp);
}

void
Inventory::UpdateEndpointGroupRelations(const ChangeInfo& chgInfo)
{
    UpdateRelation(chgInfo, "networkDomainUri", networkDomainToEpg);
}

void
Inventory::UpdateNetworkDomainRelations(const ChangeInfo& chgInfo)
{
    UpdateRelation(chgInfo, "parentUri", networkDomainChildren);
}

void
Inventory::UpdatePolicyRelations(const ChangeInfo& chgInfo)
{
    UpdateRelation(chgInfo, "sEpgUri", epgCgToPolicy);
    UpdateRelation(chgInfo, "dEpgUri", epgCgToPolicy);
    UpdateRelation(chgInfo, "sCgUri", epgCgToPolicy);
    UpdateRelation(chgInfo, "dCgUri", epgCgToPolicy);
}

void
Inventory::FindAffectedObjectsInBackLinkMap(unsigned long ciSeqId,
        const URI& objUri,
        const BackLinkMap& backMap,
        bool recurse,
        ChangeSet& affSet)
{
    BackLinkMap::const_iterator itr = backMap.find(objUri);
    if (itr == backMap.end()) {
        return;
    }
    BOOST_FOREACH(const ObjectKey& obj, itr->second) {
        ObjectMapItr objItr = objects.find(obj);
        if (objItr != objects.end()) {
            ChangeInfo chg(ciSeqId, ChangeInfo::MOD,
                           objItr->second, objItr->second);
            affSet.insert(chg);
            if (recurse) {
                FindOtherAffectedObjects(chg, affSet);
            }
        }
    }
}

void
Inventory::FindOtherAffectedObjects(const ChangeInfo& obj,
        ChangeSet& affSet)
{
    if (obj.changeType != ChangeInfo::ADD &&
        obj.changeType != ChangeInfo::MOD) {
        return;
    }
    class_id_t cid = obj.currObject->GetClass();
    const URI& uri = obj.currObject->GetUri();
    unsigned long ciSeqId = obj.seqId;

    if (cid == model::CLASS_ID_ENDPOINT_GROUP) {
        FindAffectedObjectsInBackLinkMap(ciSeqId, uri, epgToEp, false, affSet);
    }
    if (cid == model::CLASS_ID_ENDPOINT_GROUP ||
        cid == model::CLASS_ID_CONDITION_GROUP) {
        FindAffectedObjectsInBackLinkMap(ciSeqId, uri, epgCgToPolicy,
                                         false, affSet);
    }
    if (cid == model::CLASS_ID_BD || cid == model::CLASS_ID_FD ||
        cid == model::CLASS_ID_L3CTX || cid == model::CLASS_ID_SUBNET) {
        FindAffectedObjectsInBackLinkMap(ciSeqId, uri, networkDomainChildren,
                                         true, affSet);
        FindAffectedObjectsInBackLinkMap(ciSeqId, uri, networkDomainToEpg,
                                         true, affSet);
    }
}

modb::MoBasePtr
Inventory::Find(const modb::URI& uri) {
    ObjectMapItr itr = objects.find(uri);
    return itr != objects.end() ? itr->second : MoBasePtr();
}

modb::MoBasePtr
Inventory::FindNetObj(const modb::URI& uri, modb::class_id_t targetClass) {
    MoBasePtr nullObj;
    URI toFind(uri);
    do {
        ObjectMapItr itr = objects.find(toFind);
        if (itr == objects.end()) {
            return nullObj;
        }
        if (itr->second->GetClass() == targetClass) {
            return itr->second;
        }
        switch (itr->second->GetClass()) {
        case model::CLASS_ID_SUBNET:
        case model::CLASS_ID_FD:
        case model::CLASS_ID_BD:
            toFind = URI(itr->second->Get("parentUri"));
            break;
        default:
            return nullObj;
        }
    } while (!toFind.toString().empty());
    return nullObj;
}

}   // namespace enforcer
}   // namespace opflex
