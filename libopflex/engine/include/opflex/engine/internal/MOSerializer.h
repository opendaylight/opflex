/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file MOSerializer.h
 * @brief Interface definition file for MOSerializer
 */
/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <vector>
#include <map>

#include <rapidjson/document.h>
#include <rapidjson/writer.h>

#include "opflex/modb/internal/ObjectStore.h"
#include "opflex/gbp/Policy.h"
#include "opflex/logging/internal/logging.hpp"

#ifndef OPFLEX_ENGINE_MOSERIALIZER_H
#define OPFLEX_ENGINE_MOSERIALIZER_H

namespace opflex {
namespace engine {
namespace internal {

/**
 * Serialize and deserialize a JSON managed object into an
 * ObjectInstance.
 *
 * A single instance can be used from multiple threads safely.
 */
class MOSerializer {
public:
    /**
     * A listener that will get called when a managed object is
     * updated from deserialize
     */
    class Listener {
    public:
        virtual ~Listener() {}

        /**
         * A managed object was just written to the store
         */
        virtual void remoteObjectUpdated(modb::class_id_t class_id,
                                         const modb::URI& uri,
                                         gbp::PolicyUpdateOp op) = 0;
    };

    /**
     * Allocate a new managed object serializer
     */
    MOSerializer(modb::ObjectStore* store, Listener* listener = NULL);
    ~MOSerializer();

    /**
     * Serialize the whole object subtree rooted at the given URI.
     *
     * @param class_id the class ID of the object to serialize
     * @param uri the URI of the object instance
     * @param client the store client to use to look up the data
     * @param writer the writer to write to
     * @param recursive serialize the children as well
     * @throws std::out_of_range if there is no such managed object
     */
    template <typename T>
    void serialize(modb::class_id_t class_id,
                   const modb::URI& uri,
                   modb::mointernal::StoreClient& client,
                   T& writer,
                   bool recursive = true) {
        const modb::ClassInfo& ci = store->getClassInfo(class_id);
        const OF_SHARED_PTR<const modb::mointernal::ObjectInstance>
            oi(client.get(class_id, uri));
        std::map<modb::class_id_t, std::vector<modb::URI> > children;

        writer.StartObject();

        writer.String("subject");
        writer.String(ci.getName().c_str());

        writer.String("uri");
        writer.String(uri.toString().c_str());

        writer.String("properties");
        writer.StartArray();
        const modb::ClassInfo::property_map_t& pmap = ci.getProperties();
        modb::ClassInfo::property_map_t::const_iterator pit;
        for (pit = pmap.begin(); pit != pmap.end(); ++pit) {
            if (pit->second.getType() != modb::PropertyInfo::COMPOSITE &&
                !oi->isSet(pit->first, pit->second.getType(),
                           pit->second.getCardinality()))
                continue;

            switch (pit->second.getType()) {
            case modb::PropertyInfo::STRING:
                writer.StartObject();
                writer.String("name");
                writer.String(pit->second.getName().c_str());
                writer.String("data");
                if (pit->second.getCardinality() == modb::PropertyInfo::SCALAR) {
                    writer.String(oi->getString(pit->first).c_str());
                } else {
                    writer.StartArray();
                    size_t len = oi->getStringSize(pit->first);
                    for (size_t i = 0; i < len; ++i) {
                        writer.String(oi->getString(pit->first, i).c_str());
                    }
                    writer.EndArray();
                }
                writer.EndObject();
                break;
            case modb::PropertyInfo::S64:
                writer.StartObject();
                writer.String("name");
                writer.String(pit->second.getName().c_str());
                writer.String("data");
                if (pit->second.getCardinality() == modb::PropertyInfo::SCALAR) {
                    writer.Int64(oi->getInt64(pit->first));
                } else {
                    writer.StartArray();
                    size_t len = oi->getInt64Size(pit->first);
                    for (size_t i = 0; i < len; ++i) {
                        writer.Int64(oi->getInt64(pit->first, i));
                    }
                    writer.EndArray();
                }
                writer.EndObject();
                break;
            case modb::PropertyInfo::ENUM8:
            case modb::PropertyInfo::ENUM16:
            case modb::PropertyInfo::ENUM32:
            case modb::PropertyInfo::ENUM64:
                writer.StartObject();
                writer.String("name");
                writer.String(pit->second.getName().c_str());
                writer.String("data");
                if (pit->second.getCardinality() == modb::PropertyInfo::SCALAR) {
                    uint64_t v = oi->getUInt64(pit->first);
                    serialize_enum(client, pit->second, writer, v);
                } else {
                    writer.StartArray();
                    size_t len = oi->getReferenceSize(pit->first);
                    for (size_t i = 0; i < len; ++i) {
                        uint64_t v = oi->getUInt64(pit->first, i);
                        serialize_enum(client, pit->second, writer, v);
                    }
                    writer.EndArray();
                }
                writer.EndObject();
                break;
            case modb::PropertyInfo::U64:
                writer.StartObject();
                writer.String("name");
                writer.String(pit->second.getName().c_str());
                writer.String("data");
                if (pit->second.getCardinality() == modb::PropertyInfo::SCALAR) {
                    writer.Uint64(oi->getUInt64(pit->first));
                } else {
                    writer.StartArray();
                    size_t len = oi->getUInt64Size(pit->first);
                    for (size_t i = 0; i < len; ++i) {
                        writer.Uint64(oi->getUInt64(pit->first, i));
                    }
                    writer.EndArray();
                }
                writer.EndObject();
                break;
            case modb::PropertyInfo::MAC:
                writer.StartObject();
                writer.String("name");
                writer.String(pit->second.getName().c_str());
                writer.String("data");
                if (pit->second.getCardinality() == modb::PropertyInfo::SCALAR) {
                    writer.String(oi->getMAC(pit->first).toString().c_str());
                } else {
                    writer.StartArray();
                    size_t len = oi->getMACSize(pit->first);
                    for (size_t i = 0; i < len; ++i) {
                        writer.String(oi->getMAC(pit->first, i)
                                      .toString().c_str());
                    }
                    writer.EndArray();
                }
                writer.EndObject();
                break;
            case modb::PropertyInfo::REFERENCE:
                writer.StartObject();
                writer.String("name");
                writer.String(pit->second.getName().c_str());
                writer.String("data");
                if (pit->second.getCardinality() == modb::PropertyInfo::SCALAR) {
                    modb::reference_t r = oi->getReference(pit->first);
                    serialize_ref(client, writer, r);
                } else {
                    writer.StartArray();
                    size_t len = oi->getReferenceSize(pit->first);
                    for (size_t i = 0; i < len; ++i) {
                        modb::reference_t r = oi->getReference(pit->first, i);
                        serialize_ref(client, writer, r);
                    }
                    writer.EndArray();
                }
                writer.EndObject();
                break;
            case modb::PropertyInfo::COMPOSITE:
                client.getChildren(class_id, uri, pit->first,
                                   pit->second.getClassId(),
                                   children[pit->second.getClassId()]);
                break;
            }
        }
        writer.EndArray();

        writer.String("children");
        writer.StartArray();
        std::map<modb::class_id_t,
                 std::vector<modb::URI> >::const_iterator clsit;
        std::vector<modb::URI>::const_iterator cit;
        for (clsit = children.begin(); clsit != children.end(); ++clsit) {
            for (cit = clsit->second.begin(); cit != clsit->second.end(); ++cit) {
                writer.String(cit->toString().c_str());
            }
        }
        writer.EndArray();

        try {
            std::pair<modb::URI, modb::prop_id_t> parent(modb::URI::ROOT, 0);
            if (client.getParent(class_id, uri, parent)) {
                const modb::ClassInfo& parent_class =
                    store->getPropClassInfo(parent.second);
                const modb::PropertyInfo& parent_prop =
                    parent_class.getProperty(parent.second);

                writer.String("parent_subject");
                writer.String(parent_class.getName().c_str());
                writer.String("parent_uri");
                writer.String(parent.first.toString().c_str());
                writer.String("parent_relation");
                writer.String(parent_prop.getName().c_str());
            }
        } catch (const std::out_of_range& e) {
            // some parent info not found
        }

        writer.EndObject();
        if (recursive) {
            for (clsit = children.begin(); clsit != children.end(); ++clsit) {
                for (cit = clsit->second.begin();
                     cit != clsit->second.end(); ++cit) {
                    serialize(clsit->first, *cit, client, writer);
                }
            }
        }
    }

    /**
     * Deserialize the parameters from the JSON value into the object
     * instance.
     *
     * @param mo the JSON value to deserialize
     * @param client the store client where we should write the output
     * @param replaceChildren if true, delete any children not present
     * in the list of child URIs.
     * @param notifs an optional map that will hold update
     * notifications that should be dispatched as a result of this
     * change.
     */
    void deserialize(const rapidjson::Value& mo,
                     modb::mointernal::StoreClient& client,
                     bool replaceChildren,
                     /* out */
                     modb::mointernal::StoreClient::notif_t* notifs = NULL);

    /**
     * Dump the managed object database to the file specified as a
     * JSON blob.
     *
     * @param file the file to write to.
     */
    void dumpMODB(const std::string& file);

    /**
     * Dump the managed object database to the file specified as a
     * JSON blob.
     *
     * @param file the file to write to.
     */
    void dumpMODB(FILE* file);

    /**
     * Read managed objects from the given file into the MODB
     *
     * @param file the file containing the managed objects
     * @param client the store client to use
     * @param return the number of managed objects read
     */
    size_t readMOs(FILE* file,
                   modb::mointernal::StoreClient& client);

    /**
     * Update managed objects from RapidJson document into the MODB
     *
     * @param d the RapidJson document representing the update
     * @param client the store client to use
     * @param op the Update opcode
     * @param return the number of managed objects updated
     */
    size_t updateMOs(rapidjson::Document& d,
                     modb::mointernal::StoreClient& client,
                     gbp::PolicyUpdateOp op);

    /**
     * Display the managed object database in a human-readable format
     *
     * @param ostream the output stream to write to
     * @param tree display in a nested tree format
     * @param includeProps include the properties of the objects
     * @param utf8 use UTF-8 characters when drawing trees
     * @param truncate truncate URIs to the specified number of bytes.
     * 0 means do not truncate.
     */
    void displayMODB(std::ostream& ostream,
                     bool tree = true, bool includeProps = false,
                     bool utf8 = true, size_t truncate = 0);

private:
    modb::ObjectStore* store;
    Listener* listener;

    /**
     * Serialize a reference
     * @param client the store client to use to look up the data
     * @param writer the writer to write to
     * @param ref the reference
     */
    template <typename T>
    void serialize_ref(modb::mointernal::StoreClient& client,
                       T& writer,
                       modb::reference_t& ref) {
        try {
            const modb::ClassInfo& ref_class =
                store->getClassInfo(ref.first);
            writer.StartObject();
            writer.String("subject");
            writer.String(ref_class.getName().c_str());
            writer.String("reference_uri");
            writer.String(ref.second.toString().c_str());
            writer.EndObject();
        } catch (const std::out_of_range& e) {
            writer.Null();
            LOG(ERROR) << "Could not find class for " << ref.first;
        }
    }

    /**
     * Serialize an enum
     * @param client the store client to use to look up the data
     * @param writer the writer to write to
     * @param ref the reference
     */
    template <typename T>
    void serialize_enum(modb::mointernal::StoreClient& client,
                        const modb::PropertyInfo& pinfo,
                        rapidjson::Writer<T>& writer,
                        uint64_t v) {
        const modb::EnumInfo& ei = pinfo.getEnumInfo();
        try {
            const std::string& name = ei.getNameById(v);
            writer.String(name.c_str());
        } catch (const std::out_of_range& e) {
            writer.Null();
            LOG(WARNING) << "No name of type "
                         << ei.getName()
                         << " found for value "
                         << v;
        }
    }

    /**
     * Deserialize a reference
     *
     * @param client the store client
     * @param pinfo the property info for the reference
     * @param v the value containing the reference
     * @param oi the object instance where we'll store the result
     * @param scalar true if this is a scalar-valued reference
     */
    void deserialize_ref(modb::mointernal::StoreClient& client,
                         const modb::PropertyInfo& pinfo,
                         const rapidjson::Value& v,
                         modb::mointernal::ObjectInstance& oi,
                         bool scalar);

    /**
     * Deserialize an enum
     */
    void deserialize_enum(modb::mointernal::StoreClient& client,
                          const modb::PropertyInfo& pinfo,
                          const rapidjson::Value& v,
                          modb::mointernal::ObjectInstance& oi,
                          bool scalar);

    /**
     * Display a particular object
     */
    void displayObject(std::ostream& ostream,
                       modb::class_id_t class_id,
                       const modb::URI& uri,
                       bool tree, bool root, bool includeProps,
                       bool last, const std::string& prefix,
                       size_t prefixCharCount,
                       bool utf8, size_t truncate = 0);

};

} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */

#endif /* OPFLEX_ENGINE_MOSERIALIZER_H */
