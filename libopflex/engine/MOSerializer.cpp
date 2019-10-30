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

/* This must be included before anything else */
#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include <cstdio>
#include <sstream>

#include <boost/utility.hpp>
#include <boost/foreach.hpp>
#include <boost/next_prior.hpp>
#include <rapidjson/document.h>
#include <rapidjson/filewritestream.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/prettywriter.h>

#include "opflex/engine/internal/MOSerializer.h"
#include "opflex/modb/internal/ObjectStore.h"
#include "opflex/modb/internal/Region.h"

namespace opflex {
namespace engine {
namespace internal {

using modb::ObjectStore;
using modb::ClassInfo;
using modb::EnumInfo;
using modb::PropertyInfo;
using modb::URI;
using modb::MAC;
using modb::mointernal::ObjectInstance;
using modb::mointernal::StoreClient;
using modb::Region;
using rapidjson::Value;
using rapidjson::Document;
using rapidjson::SizeType;
using std::vector;
using std::string;
using gbp::PolicyUpdateOp;

MOSerializer::MOSerializer(ObjectStore* store_, Listener* listener_)
    : store(store_), listener(listener_) {

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
    } catch (const std::out_of_range& e) {
        // ignore unknown class
        LOG(DEBUG) << "Could not deserialize reference of unknown class "
                   << subject.GetString();
    }
}

void MOSerializer::deserialize_enum(modb::mointernal::StoreClient& client,
                                    const PropertyInfo& pinfo,
                                    const rapidjson::Value& pvalue,
                                    ObjectInstance& oi,
                                    bool scalar) {
    if (!pvalue.IsString()) return;
    const EnumInfo& ei = pinfo.getEnumInfo();
    try {
        uint64_t val = ei.getIdByName(pvalue.GetString());
        if (scalar) {
            oi.setUInt64(pinfo.getId(), val);
        } else {
            oi.addUInt64(pinfo.getId(), val);
        }

    } catch (const std::out_of_range& e) {
        LOG(WARNING) << "No value of type "
                     << ei.getName()
                     << " found for name "
                     << pvalue.GetString();
    }
}

void MOSerializer::deserialize(const rapidjson::Value& mo,
                               modb::mointernal::StoreClient& client,
                               bool replaceChildren,
                               /* out */ modb::mointernal::StoreClient::notif_t* notifs) {
    if (!mo.IsObject()
        || !mo.HasMember("uri")
        || !mo.HasMember("subject")) return;

    const Value& uriv = mo["uri"];
    if (!uriv.IsString()) return;
    const Value& classv = mo["subject"];
    if (!classv.IsString()) return;

    try {
        URI uri(uriv.GetString());
        const ClassInfo& ci = store->getClassInfo(classv.GetString());
        OF_SHARED_PTR<ObjectInstance> oi =
            OF_MAKE_SHARED<ObjectInstance>(ci.getId(), false);
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
                                deserialize_ref(client, pinfo, pvalue, *oi, true);
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
                            {
                                if (pinfo.getCardinality() == PropertyInfo::VECTOR) {
                                    if (!pvalue.IsArray()) continue;
                                    for (SizeType j = 0; j < pvalue.Size(); ++j) {
                                        const Value& v = pvalue[j];
                                        deserialize_enum(client, pinfo, v, *oi, false);
                                    }
                                } else {
                                    deserialize_enum(client, pinfo, pvalue, *oi, true);
                                }
                            }
                            break;
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
                        case PropertyInfo::MAC:
                            if (pinfo.getCardinality() == PropertyInfo::VECTOR) {
                                if (!pvalue.IsArray()) continue;
                                for (SizeType j = 0; j < pvalue.Size(); ++j) {
                                    const Value& v = pvalue[j];
                                    if (!v.IsString()) continue;
                                    oi->addMAC(pinfo.getId(), MAC(v.GetString()));
                                }
                            } else {
                                oi->setMAC(pinfo.getId(),
                                           MAC(pvalue.GetString()));
                            }
                            break;
                        case PropertyInfo::COMPOSITE:
                            // do nothing;
                            break;
                        }
                    } catch (const std::invalid_argument& e) {
                        LOG(DEBUG) << "Invalid property "
                                   << pname.GetString()
                                   << " in class "
                                   << ci.getName();
                    } catch (const std::out_of_range& e) {
                        LOG(DEBUG) << "Unknown property "
                                   << pname.GetString()
                                   << " in class "
                                   << ci.getName();
                        // ignore property
                    }
                }
            }
        }

        bool remoteUpdated = false;
        if (client.putIfModified(ci.getId(), uri, oi)) {
            remoteUpdated = true;
        }
        if (mo.HasMember("parent_uri") && mo.HasMember("parent_subject")) {
            const Value& pname = mo["parent_uri"];
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
                    if (client.isPresent(parent_class.getId(), parent_uri)) {
                        if (client.addChild(parent_class.getId(),
                                            parent_uri,
                                            parent_prop.getId(),
                                            ci.getId(),
                                            uri)) {
                            if (notifs)
                                client.queueNotification(parent_class.getId(),
                                                         parent_uri,
                                                         *notifs);
                        }
                    } else {
                        LOG(DEBUG2) << "No parent present for "
                                    << uri.toString();
                    }
                } catch (const std::out_of_range& e) {
                    // no parent class or property found
                    LOG(ERROR) << "Invalid parent or property for "
                               << uri.toString();
                }
            }
        }

        if (replaceChildren) {
            OF_UNORDERED_SET<string> children;
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

            const ClassInfo::property_map_t& props = ci.getProperties();
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
                                LOG(DEBUG) << "Removing missing child " << child
                                           << " from updated parent " << uri;
                                client.remove(it->second.getClassId(), child,
                                              true, notifs);
                                if (notifs) {
                                    (*notifs)[child] = it->second.getClassId();
                                    remoteUpdated = true;
                                }
                            } catch (const std::out_of_range& e) {
                                // most likely already removed by
                                // another thread
                            }
                        }
                    }
                }
            }
        }

        if (remoteUpdated) {
            LOG(DEBUG2) << "Updated object " << uri;
            if (notifs)
                client.queueNotification(ci.getId(), uri, *notifs);
            PolicyUpdateOp op = replaceChildren ? PolicyUpdateOp::REPLACE
                                                : PolicyUpdateOp::ADD;
            if (listener)
                listener->remoteObjectUpdated(ci.getId(), uri, op);
        }

    } catch (const std::invalid_argument& e) {
        // ignore invalid URIs
        LOG(DEBUG) << "Could not deserialize invalid object of class "
                   << classv.GetString();
    } catch (const std::out_of_range& e) {
        // ignore unknown class
        LOG(DEBUG) << "Could not deserialize object of unknown class "
                   << classv.GetString();
    }
}

static void getRoots(ObjectStore* store, Region::obj_set_t& roots) {
    OF_UNORDERED_SET<string> owners;
    store->getOwners(owners);
    BOOST_FOREACH(const string& owner, owners) {
        try {
            Region* r = store->getRegion(owner);
            r->getRoots(roots);
        } catch (const std::out_of_range& e) { }
    }
}

void MOSerializer::dumpMODB(FILE* pfile) {
    Region::obj_set_t roots;
    getRoots(store, roots);

    char buffer[1024];
    rapidjson::FileWriteStream ws(pfile, buffer, sizeof(buffer));
    rapidjson::PrettyWriter<rapidjson::FileWriteStream> writer(ws);
    writer.StartArray();
    StoreClient& client = store->getReadOnlyStoreClient();
    BOOST_FOREACH(Region::obj_set_t::value_type r, roots) {
        try {
            serialize(r.first, r.second, client, writer, true);
        } catch (const std::out_of_range& e) { }
    }
    writer.EndArray();
    fwrite("\n", 1, 1, pfile);
}

void MOSerializer::dumpMODB(const std::string& file) {
    FILE* pfile = fopen(file.c_str(), "w");
    if (pfile == NULL) {
        LOG(ERROR) << "Could not open MODB file "
                   << file << " for writing";
        return;
    }
    dumpMODB(pfile);
    fclose(pfile);
    LOG(INFO) << "Wrote MODB to " << file;
}

size_t MOSerializer::readMOs(FILE* pfile, StoreClient& client) {
    char buffer[1024];
    rapidjson::FileReadStream f(pfile, buffer, sizeof(buffer));
    rapidjson::Document d;
    d.ParseStream<0, rapidjson::UTF8<>, rapidjson::FileReadStream>(f);
    if (!d.IsArray()) {
        LOG(ERROR) << "Malformed policy file: not an array";
        return 0;
    }
    rapidjson::Value::ConstValueIterator moit;
    size_t i = 0;
    for (moit = d.Begin(); moit != d.End(); ++ moit) {
        const rapidjson::Value& mo = *moit;
        deserialize(mo, client, true, NULL);
        i += 1;
    }
    return i;
}

size_t MOSerializer::updateMOs(rapidjson::Document& d, StoreClient& client,
                               PolicyUpdateOp op) {
    rapidjson::Value::ConstValueIterator moit;
    size_t i = 0;
    bool replaceChildren = (op == PolicyUpdateOp::REPLACE);
    bool deleteRec = (op == PolicyUpdateOp::DELETE_RECURSIVE);
    if (replaceChildren || op == PolicyUpdateOp::ADD) {
        for (moit = d.Begin(); moit != d.End(); ++ moit) {
            const rapidjson::Value& mo = *moit;
            deserialize(mo, client, replaceChildren, NULL);
            i += 1;
        }
    } else if (deleteRec || op == PolicyUpdateOp::DELETE) {
        for (moit = d.Begin(); moit != d.End(); ++ moit) {
            const rapidjson::Value& mo = *moit;

            if (!mo.IsObject()
                || !mo.HasMember("uri")
                || !mo.HasMember("subject")) continue;

            const Value& uriv = mo["uri"];
            if (!uriv.IsString()) continue;
            const Value& classv = mo["subject"];
            if (!classv.IsString()) continue;

            try {
                URI uri(uriv.GetString());
                const ClassInfo& ci = store->getClassInfo(classv.GetString());
                if (client.remove(ci.getId(), uri, deleteRec, NULL) && listener)
                    listener->remoteObjectUpdated(ci.getId(), uri, op);
            } catch (const std::invalid_argument& e) {
                // ignore invalid URIs
                LOG(DEBUG) << "Could not deserialize invalid object of class "
                           << classv.GetString();
            } catch (const std::out_of_range& e) {
                // ignore unknown class
                LOG(DEBUG) << "Could not deserialize object of unknown class "
                           << classv.GetString();
            }
            i += 1;
        }
    }
    return i;
}

#define FORMAT_PROP(gfunc, type, prefixTrunc, output)                   \
    {                                                                   \
        std::ostringstream str;                                         \
        if (pit->second.getCardinality() == modb::PropertyInfo::SCALAR) { \
            type pvalue(oi->get##gfunc(pit->first));                    \
            str << output;                                              \
        } else {                                                        \
            size_t len = oi->get##gfunc##Size(pit->first);              \
            bool first = true;                                          \
            str << '[';                                                 \
            for (size_t i = 0; i < len; ++i) {                          \
                type pvalue(oi->get##gfunc(pit->first, i));             \
                if (!first) {                                           \
                    str << ", ";                                        \
                    first = false;                                      \
                }                                                       \
                str << output;                                          \
            }                                                           \
            str << ']';                                                 \
        }                                                               \
        dispProps[pit->second.getName()] =                              \
            std::make_pair<bool, std::string>(prefixTrunc, str.str());  \
    }

static std::string getRefSubj(const modb::ObjectStore& store,
                              const modb::reference_t& ref) {
    try {
        const modb::ClassInfo& ref_class =
            store.getClassInfo(ref.first);
        return ref_class.getName();
    } catch (const std::out_of_range& e) {
        return "UNKNOWN:" + ref.first;
    }
}

static std::string getEnumVal(const modb::PropertyInfo& pinfo, uint64_t v) {
    const modb::EnumInfo& ei = pinfo.getEnumInfo();
    try {
        return ei.getNameById(v);
    } catch (const std::out_of_range& e) {
        return "UNKNOWN: " + v;
    }
}

static const string BULLET("\xe2\xa6\x81");
static const string HORIZ("\xe2\x94\x80");
static const string VERT("\xe2\x94\x82");
static const string DOWN_HORIZ("\xe2\x94\xac");
static const string ARC_UP_RIGHT("\xe2\x95\xb0");
static const string VERT_RIGHT("\xe2\x94\x9c");
static const string ELLIPSIS("\xe2\x80\xa6");

void MOSerializer::displayObject(std::ostream& ostream,
                                 modb::class_id_t class_id,
                                 const modb::URI& uri,
                                 bool tree, bool root,
                                 bool includeProps,
                                 bool last, const std::string& prefix,
                                 size_t prefixCharCount,
                                 bool utf8, size_t truncate) {
    StoreClient& client = store->getReadOnlyStoreClient();
    const modb::ClassInfo& ci = store->getClassInfo(class_id);
    const OF_SHARED_PTR<const modb::mointernal::ObjectInstance>
        oi(client.get(class_id, uri));
    std::map<modb::class_id_t, std::vector<modb::URI> > children;

    typedef std::map<std::string, std::pair<bool, std::string> > dmap;
    dmap dispProps;
    size_t maxPropName = 0;

    const modb::ClassInfo::property_map_t& pmap = ci.getProperties();
    modb::ClassInfo::property_map_t::const_iterator pit;
    for (pit = pmap.begin(); pit != pmap.end(); ++pit) {
        if (pit->second.getType() != modb::PropertyInfo::COMPOSITE &&
            !oi->isSet(pit->first, pit->second.getType(),
                       pit->second.getCardinality()))
            continue;

        if (pit->second.getType() != modb::PropertyInfo::COMPOSITE &&
            pit->second.getName().size() > maxPropName)
            maxPropName = pit->second.getName().size();

        switch (pit->second.getType()) {
        case modb::PropertyInfo::STRING:
            FORMAT_PROP(String, std::string, false, pvalue)
            break;
        case modb::PropertyInfo::S64:
            FORMAT_PROP(Int64, int64_t, false, pvalue)
            break;
        case modb::PropertyInfo::U64:
            FORMAT_PROP(UInt64, uint64_t, false, pvalue)
            break;
        case modb::PropertyInfo::MAC:
            FORMAT_PROP(MAC, modb::MAC, false, pvalue)
            break;
        case modb::PropertyInfo::ENUM8:
        case modb::PropertyInfo::ENUM16:
        case modb::PropertyInfo::ENUM32:
        case modb::PropertyInfo::ENUM64:
            FORMAT_PROP(UInt64, uint64_t, false,
                        pvalue
                        << " (" << getEnumVal(pit->second, pvalue) << ")");
            break;
        case modb::PropertyInfo::REFERENCE:
            FORMAT_PROP(Reference, modb::reference_t, true,
                        getRefSubj(*store, pvalue) << "," << pvalue.second);
            break;
        case modb::PropertyInfo::COMPOSITE:
            client.getChildren(class_id, uri, pit->first,
                               pit->second.getClassId(),
                               children[pit->second.getClassId()]);
            break;
        }
    }

    bool hasChildren = false;
    std::map<modb::class_id_t,
             std::vector<modb::URI> >::iterator clsit;
    std::vector<modb::URI>::const_iterator cit;
    for (clsit = children.begin(); clsit != children.end(); ) {
        if (clsit->second.size() == 0)
            children.erase(clsit++);
        else {
            hasChildren = true;
            ++clsit;
        }
    }

    size_t lineLength = 0;
    ostream << prefix;
    lineLength += prefixCharCount;

    if (tree) {
        if (root) {
            ostream << (utf8 ? HORIZ : "-");
            if (last)
                ostream << (utf8 ? HORIZ : "-");
            else
                ostream << (utf8 ? DOWN_HORIZ : "-");

        } else {
            if (last)
                ostream << (utf8 ? ARC_UP_RIGHT : "`");
            else
                ostream << (utf8 ? VERT_RIGHT : "|");
            ostream << (utf8 ? HORIZ : "-");
        }
        if (hasChildren)
            ostream << (utf8 ? DOWN_HORIZ : "-")
                    << (utf8 ? BULLET : "*") << " ";
        else
            ostream << (utf8 ? HORIZ : "-")
                    << (utf8 ? BULLET : "*") << " ";

        lineLength += 5;
    }

    ostream << ci.getName() << ",";
    lineLength += ci.getName().size() + 1;

    if (truncate == 0) {
        ostream << uri;
    } else {
        const string& uriStr = uri.toString();
        size_t remaining = 0;
        if (lineLength < truncate)
            remaining = truncate - lineLength;

        if (remaining > 0) {
            if (uriStr.size() > remaining) {
                if (utf8) ostream << ELLIPSIS;
                else ostream << "_";

                ostream << uriStr.substr(uriStr.size() - remaining + 1,
                                         uriStr.size());
            } else {
                ostream << uriStr;
            }
        }
    }
    ostream << " " << std::endl;

    if (includeProps && dispProps.size() > 0) {
        lineLength = 0;
        string pprefix = prefix;
        lineLength += prefixCharCount;
        if (tree) {
            if (hasChildren) {
                if (last)
                    pprefix = pprefix + "  " +
                        (utf8 ? VERT : "|") + "   ";
                else
                    pprefix = pprefix +
                        (utf8 ? VERT : "|") + " " +
                        (utf8 ? VERT : "|") + "   ";
            } else {
                if (last)
                    pprefix = pprefix + "      ";
                else
                    pprefix = pprefix + (utf8 ? VERT : "|") + "     ";

            }
            lineLength += 6;
        }
        ostream << pprefix << "{" << std::endl;
        BOOST_FOREACH(dmap::value_type v, dispProps) {
            ostream << pprefix << "  ";
            ostream.width(maxPropName);
            ostream << std::left << v.first << " : ";
            size_t plineLength = lineLength + maxPropName + 5;
            size_t remaining = 0;
            if (plineLength < truncate)
                remaining = truncate - plineLength;

            string& propv = v.second.second;
            if (truncate == 0 || remaining > 0) {
                if (truncate != 0 && propv.size() > remaining) {
                    if (v.second.first) {
                        if (utf8) ostream << ELLIPSIS;
                        else ostream << "_";
                        ostream << propv.substr(propv.size() - remaining + 1,
                                                propv.size());
                    } else {
                        ostream << propv.substr(0, remaining - 1);
                        if (utf8) ostream << ELLIPSIS;
                        else ostream << "_";
                    }
                } else {
                    ostream << propv;
                }
            }
            ostream << std::endl;

        }
        ostream << pprefix << "}" << std::endl;
    }

    for (clsit = children.begin(); clsit != children.end(); ++clsit) {
        bool lclass = boost::next(clsit) == children.end();
        for (cit = clsit->second.begin();
             cit != clsit->second.end(); ++cit) {

            bool islast = lclass && (boost::next(cit) == clsit->second.end());
            string nprefix = prefix;
            size_t nPrefixCharCount = prefixCharCount;
            if (tree) {
                nprefix = prefix + (last ? "  " : (utf8 ? VERT : "|") + " ");
                nPrefixCharCount += 2;
            }
            displayObject(ostream, clsit->first, *cit,
                          tree, false, includeProps, islast, nprefix,
                          nPrefixCharCount, utf8, truncate);
        }
    }
}

void MOSerializer::displayMODB(std::ostream& ostream,
                               bool tree, bool includeProps, bool utf8,
                               size_t truncate) {
    Region::obj_set_t roots;
    getRoots(store, roots);

    BOOST_FOREACH(Region::obj_set_t::value_type r, roots) {
        try {
            displayObject(ostream, r.first, r.second,
                          tree, true, includeProps,
                          true, "", 0, utf8, truncate);
        } catch (const std::out_of_range& e) { }
    }
}


} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */
