/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for ObjectInstance class.
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


#include <utility>

#include <boost/foreach.hpp>

#include "opflex/modb/mo-internal/ObjectInstance.h"

namespace opflex {
namespace modb {

size_t hash_value(prop_key_t const& key) {
    std::size_t seed = 0;
    boost::hash_combine(seed, boost::get<0>(key));
    boost::hash_combine(seed, boost::get<1>(key));
    boost::hash_combine(seed, boost::get<2>(key));
    return seed;
}

size_t hash_value(reference_t const& p) {
    std::size_t seed = 0;
    boost::hash_combine(seed, p.first);
    boost::hash_combine(seed, p.second);
    return seed;
}

namespace mointernal {

using std::string;
using std::vector;
using std::pair;
using std::make_pair;
using boost::make_tuple;
using boost::get;

static PropertyInfo::property_type_t
normalize(PropertyInfo::property_type_t type) {
    switch (type) {
    case PropertyInfo::ENUM8:
    case PropertyInfo::ENUM16:
    case PropertyInfo::ENUM32:
    case PropertyInfo::ENUM64:
        return PropertyInfo::U64;
        break;
    default:
        return type;
        break;
    }
}

ObjectInstance::Value::Value(const Value& val)
    : type(val.type), cardinality(val.cardinality) {
    if (cardinality == PropertyInfo::SCALAR) {
        value = val.value;
    } else if (cardinality == PropertyInfo::VECTOR &&
               val.value.which() != 0) {
        if (type == PropertyInfo::U64)
            value = new vector<uint64_t>(*get<vector<uint64_t>*>(val.value));
        else if (type == PropertyInfo::S64)
            value = new vector<int64_t>(*get<vector<int64_t>*>(val.value));
        else if (type == PropertyInfo::REFERENCE)
            value = new vector<reference_t>(*get<vector<reference_t>*>(val.value));
        else if (type == PropertyInfo::STRING)
            value = new vector<string>(*get<vector<string>*>(val.value));
    }
}

ObjectInstance::Value::~Value() {
    clear();
}

void ObjectInstance::Value::clear() {
    if (cardinality == PropertyInfo::VECTOR &&
        value.which() != 0) {
        if (type == PropertyInfo::U64)
            delete get<vector<uint64_t>*>(value);
        else if (type == PropertyInfo::S64)
            delete get<vector<int64_t>*>(value);
        else if (type == PropertyInfo::REFERENCE)
            delete get<vector<reference_t>*>(value);
        else if (type == PropertyInfo::STRING)
            delete get<vector<string>*>(value);
    }
}

ObjectInstance::Value& ObjectInstance::Value::operator=(const Value& val) {
    clear();

    type = val.type;
    cardinality = val.cardinality;

    if (cardinality == PropertyInfo::SCALAR) {
        value = val.value;
    } else if (cardinality == PropertyInfo::VECTOR &&
               val.value.which() != 0) {
        if (type == PropertyInfo::U64)
            value = new vector<uint64_t>(*get<vector<uint64_t>*>(val.value));
        else if (type == PropertyInfo::S64)
            value = new vector<int64_t>(*get<vector<int64_t>*>(val.value));
        else if (type == PropertyInfo::REFERENCE)
            value = new vector<reference_t>(*get<vector<reference_t>*>(val.value));
        else if (type == PropertyInfo::STRING)
            value = new vector<string>(*get<vector<string>*>(val.value));
    }
    return *this;
}

bool ObjectInstance::isSet(prop_id_t prop_id,
                           PropertyInfo::property_type_t type,
                           PropertyInfo::cardinality_t cardinality) const {
    type = normalize(type);
    return prop_map.find(make_tuple(type,
                                    cardinality,
                                    prop_id)) != prop_map.end();
}

bool ObjectInstance::unset(prop_id_t prop_id,
                           PropertyInfo::property_type_t type,
                           PropertyInfo::cardinality_t cardinality) {
    type = normalize(type);
    prop_map_t::iterator it =
        prop_map.find(make_tuple(type, cardinality, prop_id));
    if (it == prop_map.end()) return false;

    prop_map.erase(it);
    return true;
}

uint64_t ObjectInstance::getUInt64(prop_id_t prop_id) const {
    const Value& v = prop_map.at(make_tuple(PropertyInfo::U64,
                                            PropertyInfo::SCALAR,
                                            prop_id));
    return get<uint64_t>(v.value);
}

uint64_t ObjectInstance::getUInt64(prop_id_t prop_id,
                                   size_t index) const {
    const Value& v = prop_map.at(make_tuple(PropertyInfo::U64,
                                            PropertyInfo::VECTOR,
                                            prop_id));
    return get<vector<uint64_t>*>(v.value)->at(index);
}

size_t ObjectInstance::getUInt64Size(prop_id_t prop_id) const {
    prop_map_t::const_iterator it =
        prop_map.find(make_tuple(PropertyInfo::U64,
                                 PropertyInfo::VECTOR,
                                 prop_id));
    if (it == prop_map.end()) return 0;
    return get<vector<uint64_t>*>(it->second.value)->size();
}

const MAC& ObjectInstance::getMAC(prop_id_t prop_id) const {
    const Value& v = prop_map.at(make_tuple(PropertyInfo::MAC,
                                            PropertyInfo::SCALAR,
                                            prop_id));
    return get<MAC>(v.value);
}

const MAC& ObjectInstance::getMAC(prop_id_t prop_id,
                                   size_t index) const {
    const Value& v = prop_map.at(make_tuple(PropertyInfo::MAC,
                                            PropertyInfo::VECTOR,
                                            prop_id));
    return get<vector<MAC>*>(v.value)->at(index);
}

size_t ObjectInstance::getMACSize(prop_id_t prop_id) const {
    prop_map_t::const_iterator it =
        prop_map.find(make_tuple(PropertyInfo::MAC,
                                 PropertyInfo::VECTOR,
                                 prop_id));
    if (it == prop_map.end()) return 0;
    return get<vector<MAC>*>(it->second.value)->size();
}

int64_t ObjectInstance::getInt64(prop_id_t prop_id) const {
    const Value& v = prop_map.at(make_tuple(PropertyInfo::S64,
                                            PropertyInfo::SCALAR,
                                            prop_id));
    return get<int64_t>(v.value);
}

int64_t ObjectInstance::getInt64(prop_id_t prop_id,
                                 size_t index) const {
    const Value& v = prop_map.at(make_tuple(PropertyInfo::S64,
                                            PropertyInfo::VECTOR,
                                            prop_id));
    return get<vector<int64_t>*>(v.value)->at(index);
}

size_t ObjectInstance::getInt64Size(prop_id_t prop_id) const {
    prop_map_t::const_iterator it =
        prop_map.find(make_tuple(PropertyInfo::S64,
                                 PropertyInfo::VECTOR,
                                 prop_id));
    if (it == prop_map.end()) return 0;
    return get<vector<int64_t>*>(it->second.value)->size();
}

const string& ObjectInstance::getString(prop_id_t prop_id) const {
    const Value& v = prop_map.at(make_tuple(PropertyInfo::STRING,
                                            PropertyInfo::SCALAR,
                                            prop_id));
    return get<string>(v.value);
}

const string& ObjectInstance::getString(prop_id_t prop_id,
                                        size_t index) const {
    const Value& v = prop_map.at(make_tuple(PropertyInfo::STRING,
                                            PropertyInfo::VECTOR,
                                            prop_id));
    return get<vector<string>*>(v.value)->at(index);
}

size_t ObjectInstance::getStringSize(prop_id_t prop_id) const {
    prop_map_t::const_iterator it =
        prop_map.find(make_tuple(PropertyInfo::STRING,
                                 PropertyInfo::VECTOR,
                                 prop_id));
    if (it == prop_map.end()) return 0;
    return get<vector<string>*>(it->second.value)->size();
}

reference_t ObjectInstance::getReference(prop_id_t prop_id) const {
    const Value& v = prop_map.at(make_tuple(PropertyInfo::REFERENCE,
                                            PropertyInfo::SCALAR,
                                            prop_id));
    return get<reference_t>(v.value);
}

reference_t ObjectInstance::getReference(prop_id_t prop_id,
                                         size_t index) const {
    const Value& v = prop_map.at(make_tuple(PropertyInfo::REFERENCE,
                                            PropertyInfo::VECTOR,
                                            prop_id));
    return get<vector<reference_t>*>(v.value)->at(index);
}

size_t ObjectInstance::getReferenceSize(prop_id_t prop_id) const {
    prop_map_t::const_iterator it =
        prop_map.find(make_tuple(PropertyInfo::REFERENCE,
                                 PropertyInfo::VECTOR,
                                 prop_id));
    if (it == prop_map.end()) return 0;
    return get<vector<reference_t>*>(it->second.value)->size();
}

void ObjectInstance::setUInt64(prop_id_t prop_id, uint64_t value) {
    Value& v = prop_map[make_tuple(PropertyInfo::U64,
                                   PropertyInfo::SCALAR,
                                   prop_id)];
    v.type = PropertyInfo::U64;
    v.cardinality = PropertyInfo::SCALAR;
    v.value = value;
}

void ObjectInstance::setUInt64(prop_id_t prop_id,
                               const vector<uint64_t>& value) {
    Value& v = prop_map[make_tuple(PropertyInfo::U64,
                                   PropertyInfo::VECTOR,
                                   prop_id)];
    v.type = PropertyInfo::U64;
    v.cardinality = PropertyInfo::VECTOR;
    if (v.value.which() != 0)
        delete get<vector<uint64_t>*>(v.value);
    v.value = new vector<uint64_t>(value);
}

void ObjectInstance::setMAC(prop_id_t prop_id, const MAC& value) {
    Value& v = prop_map[make_tuple(PropertyInfo::MAC,
                                   PropertyInfo::SCALAR,
                                   prop_id)];
    v.type = PropertyInfo::MAC;
    v.cardinality = PropertyInfo::SCALAR;
    v.value = value;
}

void ObjectInstance::setMAC(prop_id_t prop_id,
                               const vector<MAC>& value) {
    Value& v = prop_map[make_tuple(PropertyInfo::MAC,
                                   PropertyInfo::VECTOR,
                                   prop_id)];
    v.type = PropertyInfo::MAC;
    v.cardinality = PropertyInfo::VECTOR;
    if (v.value.which() != 0)
        delete get<vector<MAC>*>(v.value);
    v.value = new vector<MAC>(value);
}

void ObjectInstance::setInt64(prop_id_t prop_id, int64_t value) {
    Value& v = prop_map[make_tuple(PropertyInfo::S64,
                                   PropertyInfo::SCALAR,
                                   prop_id)];
    v.type = PropertyInfo::S64;
    v.cardinality = PropertyInfo::SCALAR;
    v.value = value;
}

void ObjectInstance::setInt64(prop_id_t prop_id,
                              const vector<int64_t>& value) {
    Value& v = prop_map[make_tuple(PropertyInfo::S64,
                                   PropertyInfo::VECTOR,
                                   prop_id)];
    v.type = PropertyInfo::S64;
    v.cardinality = PropertyInfo::VECTOR;
    if (v.value.which() != 0)
        delete get<vector<int64_t>*>(v.value);
    v.value = new vector<int64_t>(value);
}

void ObjectInstance::setString(prop_id_t prop_id, const string& value) {
    Value& v = prop_map[make_tuple(PropertyInfo::STRING,
                                   PropertyInfo::SCALAR,
                                   prop_id)];
    v.type = PropertyInfo::STRING;
    v.cardinality = PropertyInfo::SCALAR;
    v.value = value;
}

void ObjectInstance::setString(prop_id_t prop_id,
                               const vector<string>& value) {
    Value& v = prop_map[make_tuple(PropertyInfo::STRING,
                                   PropertyInfo::VECTOR,
                                   prop_id)];
    v.type = PropertyInfo::STRING;
    v.cardinality = PropertyInfo::VECTOR;
    if (v.value.which() != 0)
        delete get<vector<string>*>(v.value);
    v.value = new vector<string>(value);
}


void ObjectInstance::setReference(prop_id_t prop_id,
                                  class_id_t class_id, const URI& uri) {
    Value& v = prop_map[make_tuple(PropertyInfo::REFERENCE,
                                   PropertyInfo::SCALAR,
                                   prop_id)];
    v.type = PropertyInfo::REFERENCE;
    v.cardinality = PropertyInfo::SCALAR;
    v.value = make_pair(class_id, uri);
}

void ObjectInstance::setReference(prop_id_t prop_id,
                                  const vector<reference_t>& value) {
    Value& v = prop_map[make_tuple(PropertyInfo::REFERENCE,
                                   PropertyInfo::VECTOR,
                                   prop_id)];
    v.type = PropertyInfo::REFERENCE;
    v.cardinality = PropertyInfo::VECTOR;
    if (v.value.which() != 0)
        delete get<vector<reference_t>*>(v.value);
    v.value = new vector<reference_t>(value);
}

void ObjectInstance::addUInt64(prop_id_t prop_id, uint64_t value) {
    Value& v = prop_map[make_tuple(PropertyInfo::U64,
                                   PropertyInfo::VECTOR,
                                   prop_id)];
    vector<uint64_t>* val;
    if (v.value.which() == 0) {
        v.type = PropertyInfo::U64;
        v.cardinality = PropertyInfo::VECTOR;
        v.value = val = new vector<uint64_t>();
    } else {
        val = get<vector<uint64_t>*>(v.value);
    }
    val->push_back(value);
}

void ObjectInstance::addMAC(prop_id_t prop_id, const MAC& value) {
    Value& v = prop_map[make_tuple(PropertyInfo::MAC,
                                   PropertyInfo::VECTOR,
                                   prop_id)];
    vector<MAC>* val;
    if (v.value.which() == 0) {
        v.type = PropertyInfo::MAC;
        v.cardinality = PropertyInfo::VECTOR;
        v.value = val = new vector<MAC>();
    } else {
        val = get<vector<MAC>*>(v.value);
    }
    val->push_back(value);
}

void ObjectInstance::addInt64(prop_id_t prop_id, int64_t value) {
    Value& v = prop_map[make_tuple(PropertyInfo::S64,
                                   PropertyInfo::VECTOR,
                                   prop_id)];
    vector<int64_t>* val;
    if (v.value.which() == 0) {
        v.type = PropertyInfo::S64;
        v.cardinality = PropertyInfo::VECTOR;
        v.value = val = new vector<int64_t>();
    } else {
        val = get<vector<int64_t>*>(v.value);
    }
    val->push_back(value);
}

void ObjectInstance::addString(prop_id_t prop_id, const string& value) {
    Value& v = prop_map[make_tuple(PropertyInfo::STRING,
                                   PropertyInfo::VECTOR,
                                   prop_id)];
    vector<string>* val;
    if (v.value.which() == 0) {
        v.type = PropertyInfo::STRING;
        v.cardinality = PropertyInfo::VECTOR;
        v.value = val = new vector<string>();
    } else {
        val = get<vector<string>*>(v.value);
    }
    val->push_back(value);
}

void ObjectInstance::addReference(prop_id_t prop_id,
                                  class_id_t class_id,
                                  const URI& uri) {
    Value& v = prop_map[make_tuple(PropertyInfo::REFERENCE,
                                   PropertyInfo::VECTOR,
                                   prop_id)];
    vector<reference_t>* val;
    if (v.value.which() == 0) {
        v.type = PropertyInfo::REFERENCE;
        v.cardinality = PropertyInfo::VECTOR;
        v.value = val = new vector<reference_t>();
    } else {
        val = get<vector<reference_t>*>(v.value);
    }
    val->push_back(make_pair(class_id, uri));
}

template <typename T>
bool equal(const ObjectInstance::Value& lhs,
           const ObjectInstance::Value& rhs) {
    if (lhs.cardinality != rhs.cardinality) return false;
    switch (lhs.cardinality) {
    case PropertyInfo::VECTOR:
        {
            const vector<T>& l = *get<vector<T>*>(lhs.value);
            const vector<T>& r = *get<vector<T>*>(rhs.value);
            return (l == r);
        }
        break;
    case PropertyInfo::SCALAR:
        {
            const T& l = get<T>(lhs.value);
            const T& r = get<T>(rhs.value);
            return (l == r);
        }
        break;
    }
    return false;
}

bool operator==(const ObjectInstance::Value& lhs,
                const ObjectInstance::Value& rhs) {
    if (lhs.type != rhs.type) return false;

    switch (lhs.type) {
    case PropertyInfo::ENUM8:
    case PropertyInfo::ENUM16:
    case PropertyInfo::ENUM32:
    case PropertyInfo::ENUM64:
    case PropertyInfo::U64:
        return equal<uint64_t>(lhs, rhs);
    case PropertyInfo::MAC:
        return equal<MAC>(lhs, rhs);
    case PropertyInfo::S64:
        return equal<int64_t>(lhs, rhs);
    case PropertyInfo::STRING:
        return equal<string>(lhs, rhs);
    case PropertyInfo::REFERENCE:
        return equal<reference_t>(lhs, rhs);
        break;
    default:
        // should not happen
        return false;
    }

    return true;
}

bool operator!=(const ObjectInstance::Value& lhs,
                const ObjectInstance::Value& rhs) {
    return !operator==(lhs,rhs);
}

bool operator==(const ObjectInstance& lhs, const ObjectInstance& rhs) {
    BOOST_FOREACH(const ObjectInstance::prop_map_t::value_type& v,
                  lhs.prop_map) {
        ObjectInstance::prop_map_t::const_iterator it =
            rhs.prop_map.find(v.first);
        if (it == rhs.prop_map.end()) return false;
        if (v.second != it->second) return false;
    }
    BOOST_FOREACH(ObjectInstance::prop_map_t::value_type v,
                  rhs.prop_map) {
        ObjectInstance::prop_map_t::const_iterator it =
            lhs.prop_map.find(v.first);
        if (it == rhs.prop_map.end()) return false;
    }
    return true;
}

bool operator!=(const ObjectInstance& lhs, const ObjectInstance& rhs) {
    return !operator==(lhs,rhs);
}

} /* namespace mointernal */
} /* namespace modb */
} /* namespace opflex */
