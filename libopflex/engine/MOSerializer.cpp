/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for MOSerializer class.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/foreach.hpp>

#include "opflex/engine/internal/MOSerializer.h"

namespace opflex {
namespace engine {
namespace internal {

using modb::ObjectStore;
using modb::ClassInfo;
using modb::PropertyInfo;
using modb::URI;
using modb::mointernal::ObjectInstance;
using rapidjson::Value;
using rapidjson::Document;
using rapidjson::SizeType;
using boost::shared_ptr;
using boost::make_shared;
using std::vector;
using std::string;
using boost::unordered_set;

MOSerializer::MOSerializer(ObjectStore* store_) : store(store_) {

}

MOSerializer::~MOSerializer() {

}

void MOSerializer::deserialize_ref(modb::mointernal::StoreClient& client,
                                   const PropertyInfo& pinfo,
                                   const rapidjson::Value& v,
                                   ObjectInstance& oi,
                                   bool scalar) {
    if (!v.IsObject() ||
        !v.HasMember("subject") ||
        !v.HasMember("reference_uri")) return;

    const Value& subject = v["subject"];
    const Value& refuri = v["reference_uri"];

    try {
        const ClassInfo& ci = store->getClassInfo(subject.GetString());
        if (scalar) {
            oi.setReference(pinfo.getId(), ci.getId(), URI(refuri.GetString()));
        } else {
            oi.addReference(pinfo.getId(), ci.getId(), URI(refuri.GetString()));
        }
    } catch (std::out_of_range e) {
        // ignore unknown class
        LOG(DEBUG) << "Could not deserialize reference of unknown class " 
                   << subject.GetString();
    }
}

void MOSerializer::deserialize(const rapidjson::Value& mo,
                               modb::mointernal::StoreClient& client,
                               bool replaceChildren,
                               /* out */ modb::mointernal::StoreClient::notif_t* notifs) {
    if (!mo.IsObject()
        || !mo.HasMember("name")
        || !mo.HasMember("subject")) return;

    const Value& uriv = mo["name"];
    if (!uriv.IsString()) return;
    const Value& classv = mo["subject"];
    if (!classv.IsString()) return;

    try {
        URI uri(uriv.GetString());
        const ClassInfo& ci = store->getClassInfo(classv.GetString());
        shared_ptr<ObjectInstance> oi = 
            make_shared<ObjectInstance>(ci.getId());
        
        if (mo.HasMember("properties")) {
            const Value& properties = mo["properties"];
            if (properties.IsArray()) {
                for (SizeType i = 0; i < properties.Size(); ++i) {
                    const Value& prop = properties[i];
                    if (!prop.IsObject() ||
                        !prop.HasMember("name") ||
                        !prop.HasMember("data"))
                        continue;

                    const Value& pname = prop["name"];
                    if (!pname.IsString())
                        continue;

                    const Value& pvalue = prop["data"];

                    try {
                        const PropertyInfo& pinfo = 
                            ci.getProperty(pname.GetString());
                        switch (pinfo.getType()) {
                        case PropertyInfo::STRING:
                            if (pinfo.getCardinality() == PropertyInfo::VECTOR) {
                                if (!pvalue.IsArray()) continue;
                                for (SizeType j = 0; j < pvalue.Size(); ++j) {
                                    const Value& v = pvalue[j];
                                    if (!v.IsString()) continue;
                                    oi->addString(pinfo.getId(), v.GetString());
                                }
                            } else {
                                if (!pvalue.IsString()) continue;
                                oi->setString(pinfo.getId(),
                                              pvalue.GetString());
                            }
                            break;
                        case PropertyInfo::REFERENCE:
                            if (pinfo.getCardinality() == PropertyInfo::VECTOR) {
                                if (!pvalue.IsArray()) continue;
                                for (SizeType j = 0; j < pvalue.Size(); ++j) {
                                    const Value& v = pvalue[j];
                                    deserialize_ref(client, pinfo, v, *oi, false);
                                }
                            } else {
                                deserialize_ref(client, pinfo, pvalue, *oi, false);
                            }
                            break;
                        case PropertyInfo::S64:
                            if (pinfo.getCardinality() == PropertyInfo::VECTOR) {
                                if (!pvalue.IsArray()) continue;
                                for (SizeType j = 0; j < pvalue.Size(); ++j) {
                                    const Value& v = pvalue[j];
                                    if (!v.IsInt64()) continue;
                                    oi->addInt64(pinfo.getId(), v.GetInt64());
                                }
                            } else {
                                if (!pvalue.IsInt64()) continue;
                                oi->setInt64(pinfo.getId(),
                                             pvalue.GetInt64());
                            }
                            break;
                        case PropertyInfo::ENUM8:
                        case PropertyInfo::ENUM16:
                        case PropertyInfo::ENUM32:
                        case PropertyInfo::ENUM64:
                        case PropertyInfo::U64:
                            if (pinfo.getCardinality() == PropertyInfo::VECTOR) {
                                if (!pvalue.IsArray()) continue;
                                for (SizeType j = 0; j < pvalue.Size(); ++j) {
                                    const Value& v = pvalue[j];
                                    if (!v.IsUint64()) continue;
                                    oi->addUInt64(pinfo.getId(), v.GetUint64());
                                }
                            } else {
                                if (!pvalue.IsUint64()) continue;
                                oi->setUInt64(pinfo.getId(),
                                              pvalue.GetUint64());
                            }
                            break;
                        case PropertyInfo::COMPOSITE:
                            // do nothing;
                            break;
                        }
                    } catch (std::out_of_range e) {
                        LOG(DEBUG) << "Unknown property " 
                                   << pname.GetString() 
                                   << " in class " 
                                   << ci.getName();
                        // ignore property
                    }
                }
            }
        }
        
        client.put(ci.getId(), uri, oi);
        if (notifs)
            client.queueNotification(ci.getId(), uri, *notifs);
        if (mo.HasMember("parent_name") && mo.HasMember("parent_subject")) {
            const Value& pname = mo["parent_name"];
            const Value& psubj = mo["parent_subject"];
            const Value* prel = &classv;
            if (mo.HasMember("parent_relation"))
                prel = &mo["parent_relation"];

            if (pname.IsString() && psubj.IsString() && prel->IsString()) {
                try {
                    const ClassInfo& parent_class = 
                        store->getClassInfo(psubj.GetString());
                    const PropertyInfo& parent_prop = 
                        parent_class.getProperty(prel->GetString());
                    URI parent_uri(pname.GetString());
                    boost::shared_ptr<const ObjectInstance> parentoi =
                        client.get(parent_class.getId(), parent_uri);

                    client.addChild(parent_class.getId(),
                                    parent_uri,
                                    parent_prop.getId(),
                                    ci.getId(),
                                    uri);
                    if (notifs)
                        client.queueNotification(parent_class.getId(),
                                                 parent_uri,
                                                 *notifs);
                } catch (std::out_of_range e) {
                    // no parent class, property, or object instance
                    // found
                    LOG(DEBUG) << "Could not resolve parent for " 
                               << uri.toString();
                }
            }
        }

        if (replaceChildren) {
            unordered_set<string> children;
            if (mo.HasMember("children")) {
                const Value& cvs = mo["children"];
                if (cvs.IsArray()) {
                    for (SizeType i = 0; i < cvs.Size(); ++i) {
                        const Value& cv = cvs[i];
                        if (cv.IsString())
                            children.insert(cv.GetString());
                    }
                }
            }

            const ClassInfo::property_map_t props = ci.getProperties();
            ClassInfo::property_map_t::const_iterator it;
            for (it = props.begin(); it != props.end(); ++it) {
                if (it->second.getType() == PropertyInfo::COMPOSITE) {
                    std::vector<URI> curChildren;
                    client.getChildren(ci.getId(),
                                       uri,
                                       it->second.getId(),
                                       it->second.getClassId(),
                                       curChildren);

                    BOOST_FOREACH(URI& child, curChildren) {
                        if (children.find(child.toString()) == children.end()) {
                            // this child isn't in the list of children
                            // set in the update
                            try {
                                client.remove(it->second.getClassId(), child,
                                              true, notifs);
                                if (notifs)
                                    (*notifs)[child] = it->second.getClassId();
                            } catch (std::out_of_range e) {
                                // most likely already removed by
                                // another thread
                            }
                        }
                    }
                }
            }
        }
    } catch (std::out_of_range e) {
        // ignore unknown class
        LOG(DEBUG) << "Could not deserialize object of unknown class " 
                   << classv.GetString();
    }
}

} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */
