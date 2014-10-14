/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for ClassInfo class.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "opflex/modb/ClassInfo.h"

namespace opflex {
namespace modb {

ClassInfo::ClassInfo(class_id_t class_id_,
                     class_type_t class_type_,
                     const std::string& class_name_,
                     const std::string& owner_,
                     const std::vector<PropertyInfo>& properties_,
                     const std::vector<prop_id_t>& naming_props_)
    : class_id(class_id_),
      class_type(class_type_),
      class_name(class_name_), 
      owner(owner_),
      naming_props(naming_props_) {
    std::vector<PropertyInfo>::const_iterator it;
    for (it = properties_.begin(); it != properties_.end(); ++it) {
        properties[it->getId()] = *it;
        prop_names[it->getName()] = it->getId();
    }
}

ClassInfo::~ClassInfo() {
}

} /* namespace modb */
} /* namespace opflex */
