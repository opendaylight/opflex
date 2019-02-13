/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for EnumInfo class.
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


#include <boost/foreach.hpp>

#include "opflex/modb/EnumInfo.h"

namespace opflex {
namespace modb {

EnumInfo::EnumInfo(const std::string &name_,
         const std::vector<ConstInfo>& consts_)
    : name(name_) {
    BOOST_FOREACH(const ConstInfo& cinst, consts_) {
        const_name_map[cinst.getName()] = cinst.getId();
        const_value_map[cinst.getId()] = cinst.getName();
    }
}

EnumInfo::~EnumInfo() {
}

const uint64_t EnumInfo::getIdByName(const std::string& name) const {
    return const_name_map.at(name);
}

const std::string& EnumInfo::getNameById(uint64_t id) const {
    return const_value_map.at(id);
}

} /* namespace modb */
} /* namespace opflex */
