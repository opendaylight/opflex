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

#include <utility>

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

namespace mointernal {

using std::string;
using std::vector;
using std::pair;
using std::make_pair;
using boost::make_tuple;
using boost::any_cast;

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
        if (type == PropertyInfo::STRING) {
            if (!val.value.empty())
                value = new string(*any_cast<string*>(val.value));
        } else {
            value = val.value;
        }
    } else if (cardinality == PropertyInfo::VECTOR && 
               !val.value.empty()) {
        if (type == PropertyInfo::U64)
            value = new vector<uint64_t>(*any_cast<vector<uint64_t>*>(val.value));
        else if (type == PropertyInfo::S64)
            value = new vector<int64_t>(*any_cast<vector<int64_t>*>(val.value));
        else if (type == PropertyInfo::REFERENCE)
            value = new vector<reference_t>(*any_cast<vector<reference_t>*>(val.value));
        else if (type == PropertyInfo::STRING)
            value = new vector<string>(*any_cast<vector<string>*>(val.value));
    }
}

ObjectInstance::Value::~Value() {
    clear();
}

void ObjectInstance::Value::clear() {
    if (cardinality == PropertyInfo::SCALAR &&
        type == PropertyInfo::STRING) {
        if (!value.empty())
            delete any_cast<string*>(value);
    } else if (cardinality == PropertyInfo::VECTOR && 
               !value.empty()) {
        if (type == PropertyInfo::U64)
            delete any_cast<vector<uint64_t>*>(value);
        else if (type == PropertyInfo::S64)
            delete any_cast<vector<int64_t>*>(value);
        else if (type == PropertyInfo::REFERENCE)
            delete any_cast<vector<reference_t>*>(value);
        else if (type == PropertyInfo::STRING)
            delete any_cast<vector<string>*>(value);
    }
}

ObjectInstance::Value& ObjectInstance::Value::operator=(const Value& val) {
    clear();

    type = val.type;
    cardinality = val.cardinality;

    if (cardinality == PropertyInfo::SCALAR) {
        if (type == PropertyInfo::STRING) {
            if (!val.value.empty())
                value = new string(*any_cast<string*>(val.value));
        } else {
            value = val.value;
        }
    } else if (cardinality == PropertyInfo::VECTOR && 
               !val.value.empty()) {
        if (type == PropertyInfo::U64)
            value = new vector<uint64_t>(*any_cast<vector<uint64_t>*>(val.value));
        else if (type == PropertyInfo::S64)
            value = new vector<int64_t>(*any_cast<vector<int64_t>*>(val.value));
        else if (type == PropertyInfo::REFERENCE)
            value = new vector<reference_t>(*any_cast<vector<reference_t>*>(val.value));
        else if (type == PropertyInfo::STRING)
            value = new vector<string>(*any_cast<vector<string>*>(val.value));
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
    return any_cast<uint64_t>(v.value);
}

uint64_t ObjectInstance::getUInt64(prop_id_t prop_id, 
                                   size_t index) const {
    const Value& v = prop_map.at(make_tuple(PropertyInfo::U64, 
                                            PropertyInfo::VECTOR, 
                                            prop_id));
    return any_cast<vector<uint64_t>*>(v.value)->at(index);
}

size_t ObjectInstance::getUInt64Size(prop_id_t prop_id) const {
    prop_map_t::const_iterator it = 
        prop_map.find(make_tuple(PropertyInfo::U64, 
                                 PropertyInfo::VECTOR, 
                                 prop_id));
    if (it == prop_map.end()) return 0;
    return any_cast<vector<uint64_t>*>(it->second.value)->size();
}

int64_t ObjectInstance::getInt64(prop_id_t prop_id) const {
    const Value& v = prop_map.at(make_tuple(PropertyInfo::S64, 
                                            PropertyInfo::SCALAR, 
                                            prop_id));
    return any_cast<int64_t>(v.value);
}

int64_t ObjectInstance::getInt64(prop_id_t prop_id, 
                                 size_t index) const {
    const Value& v = prop_map.at(make_tuple(PropertyInfo::S64, 
                                            PropertyInfo::VECTOR, 
                                            prop_id));
    return any_cast<vector<int64_t>*>(v.value)->at(index);
}

size_t ObjectInstance::getInt64Size(prop_id_t prop_id) const {
    prop_map_t::const_iterator it = 
        prop_map.find(make_tuple(PropertyInfo::S64, 
                                 PropertyInfo::VECTOR, 
                                 prop_id));
    if (it == prop_map.end()) return 0;
    return any_cast<vector<int64_t>*>(it->second.value)->size();
}

const string& ObjectInstance::getString(prop_id_t prop_id) const {
    const Value& v = prop_map.at(make_tuple(PropertyInfo::STRING, 
                                            PropertyInfo::SCALAR, 
                                            prop_id));
    return *any_cast<string*>(v.value);
}

const string& ObjectInstance::getString(prop_id_t prop_id, 
                                        size_t index) const {
    const Value& v = prop_map.at(make_tuple(PropertyInfo::STRING, 
                                            PropertyInfo::VECTOR, 
                                            prop_id));
    return any_cast<vector<string>*>(v.value)->at(index);
}

size_t ObjectInstance::getStringSize(prop_id_t prop_id) const {
    prop_map_t::const_iterator it = 
        prop_map.find(make_tuple(PropertyInfo::STRING, 
                                 PropertyInfo::VECTOR, 
                                 prop_id));
    if (it == prop_map.end()) return 0;
    return any_cast<vector<string>*>(it->second.value)->size();
}

reference_t ObjectInstance::getReference(prop_id_t prop_id) const {
    const Value& v = prop_map.at(make_tuple(PropertyInfo::REFERENCE, 
                                            PropertyInfo::SCALAR, 
                                            prop_id));
    return any_cast<reference_t>(v.value);
}

reference_t ObjectInstance::getReference(prop_id_t prop_id, 
                                         size_t index) const {
    const Value& v = prop_map.at(make_tuple(PropertyInfo::REFERENCE, 
                                            PropertyInfo::VECTOR, 
                                            prop_id));
    return any_cast<vector<reference_t>*>(v.value)->at(index);
}

size_t ObjectInstance::getReferenceSize(prop_id_t prop_id) const {
    prop_map_t::const_iterator it = 
        prop_map.find(make_tuple(PropertyInfo::REFERENCE, 
                                 PropertyInfo::VECTOR, 
                                 prop_id));
    if (it == prop_map.end()) return 0;
    return any_cast<vector<reference_t>*>(it->second.value)->size();
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
    if (!v.value.empty())
        delete any_cast<vector<uint64_t>*>(v.value);
    v.value = new vector<uint64_t>(value);
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
    if (!v.value.empty())
        delete any_cast<vector<int64_t>*>(v.value);
    v.value = new vector<int64_t>(value);
}

void ObjectInstance::setString(prop_id_t prop_id, const string& value) {
    Value& v = prop_map[make_tuple(PropertyInfo::STRING, 
                                   PropertyInfo::SCALAR, 
                                   prop_id)];
    v.type = PropertyInfo::STRING;
    v.cardinality = PropertyInfo::SCALAR;
    if (!v.value.empty())
        delete any_cast<vector<string>*>(v.value);
    v.value = new string(value);
}

void ObjectInstance::setString(prop_id_t prop_id, 
                               const vector<string>& value) {
    Value& v = prop_map[make_tuple(PropertyInfo::STRING, 
                                   PropertyInfo::VECTOR, 
                                   prop_id)];
    v.type = PropertyInfo::STRING;
    v.cardinality = PropertyInfo::VECTOR;
    if (!v.value.empty())
        delete any_cast<vector<string>*>(v.value);
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
    if (!v.value.empty())
        delete any_cast<vector<reference_t>*>(v.value);
    v.value = new vector<reference_t>(value);
}

void ObjectInstance::addUInt64(prop_id_t prop_id, uint64_t value) {
    Value& v = prop_map[make_tuple(PropertyInfo::U64, 
                                   PropertyInfo::VECTOR, 
                                   prop_id)];
    vector<uint64_t>* val;
    if (v.value.empty()) {
        v.type = PropertyInfo::U64;
        v.cardinality = PropertyInfo::VECTOR;
        v.value = val = new vector<uint64_t>();
    } else {
        val = any_cast<vector<uint64_t>*>(v.value);
    }
    val->push_back(value);
}

void ObjectInstance::addInt64(prop_id_t prop_id, int64_t value) {
    Value& v = prop_map[make_tuple(PropertyInfo::S64, 
                                   PropertyInfo::VECTOR, 
                                   prop_id)];
    vector<int64_t>* val;
    if (v.value.empty()) {
        v.type = PropertyInfo::S64;
        v.cardinality = PropertyInfo::VECTOR;
        v.value = val = new vector<int64_t>();
    } else {
        val = any_cast<vector<int64_t>*>(v.value);
    }
    val->push_back(value);
}

void ObjectInstance::addString(prop_id_t prop_id, const string& value) {
    Value& v = prop_map[make_tuple(PropertyInfo::STRING, 
                                   PropertyInfo::VECTOR, 
                                   prop_id)];
    vector<string>* val;
    if (v.value.empty()) {
        v.type = PropertyInfo::STRING;
        v.cardinality = PropertyInfo::VECTOR;
        v.value = val = new vector<string>();
    } else {
        val = any_cast<vector<string>*>(v.value);
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
    if (v.value.empty()) {
        v.type = PropertyInfo::REFERENCE;
        v.cardinality = PropertyInfo::VECTOR;
        v.value = val = new vector<reference_t>();
    } else {
        val = any_cast<vector<reference_t>*>(v.value);
    }
    val->push_back(make_pair(class_id, uri));
}

} /* namespace mointernal */
} /* namespace modb */
} /* namespace opflex */

