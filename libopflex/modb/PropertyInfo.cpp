/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for PropertyInfo class.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <boost/assert.hpp>

#include "opflex/modb/PropertyInfo.h"

namespace opflex {
namespace modb {

PropertyInfo::PropertyInfo(prop_id_t prop_id_,
                           const std::string& property_name_, 
                           property_type_t type_,
                           cardinality_t cardinality_)
    : prop_id(prop_id_),
      property_name(property_name_), 
      prop_type(type_), 
      class_id(0),
      cardinality(cardinality_) {
    switch (prop_type) {
    case ENUM8:
    case ENUM16:
    case ENUM32:
    case ENUM64:
        prop_type = U64;
        break;
    default:
        break;
    }
}

PropertyInfo::PropertyInfo(prop_id_t prop_id_,
                           const std::string& property_name_,
                           property_type_t type_,
                           cardinality_t cardinality_,
                           const EnumInfo& enum_info_)
    : prop_id(prop_id_),
      property_name(property_name_),
      prop_type(type_),
      class_id(0),
      cardinality(cardinality_),
      enum_info(enum_info_) {
    switch(prop_type) {
    case ENUM8:
    case ENUM16:
    case ENUM32:
    case ENUM64:
       prop_type = U64;
       break;
    default:
       break;
    }
}

PropertyInfo::PropertyInfo(prop_id_t prop_id_,
                           const std::string& property_name_, 
                           property_type_t type_,
                           class_id_t class_id_,
                           cardinality_t cardinality_)
    : prop_id(prop_id_),
      property_name(property_name_), 
      prop_type(type_), 
      class_id(class_id_),
      cardinality(cardinality_) {
    BOOST_ASSERT(prop_type == COMPOSITE);
}

PropertyInfo::~PropertyInfo() {

}

} /* namespace modb */
} /* namespace opflex */
