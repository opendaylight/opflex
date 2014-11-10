/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for md fixture
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef MODB_TEST_MDFIXTURE_H
#define MODB_TEST_MDFIXTURE_H

#include <vector>
#include <boost/assign/list_of.hpp>

#include "opflex/modb/ModelMetadata.h"

namespace opflex {
namespace modb {

using namespace boost::assign;

/**
 * A base fixture that defines a simple object model
 */
class MDFixture {
public:
    MDFixture()
        : md("example",
             list_of(ClassInfo(1, ClassInfo::LOCAL_ONLY, "class1", "owner1",
                               list_of 
                                   (PropertyInfo(1, "prop1", 
                                                 PropertyInfo::U64,
                                                 PropertyInfo::SCALAR))
                                   (PropertyInfo(2, "prop2", 
                                                 PropertyInfo::STRING, 
                                                 PropertyInfo::VECTOR))
                                   (PropertyInfo(3, "class2", 
                                                 PropertyInfo::COMPOSITE, 
                                                 2, 
                                                 PropertyInfo::VECTOR))
                                   (PropertyInfo(8, "class4",
                                                 PropertyInfo::COMPOSITE, 
                                                 4, 
                                                 PropertyInfo::VECTOR))
                                   (PropertyInfo(12, "class5",
                                                 PropertyInfo::COMPOSITE, 
                                                 5, 
                                                 PropertyInfo::VECTOR)),
                               std::vector<prop_id_t>()))
                    (ClassInfo(2, ClassInfo::LOCAL_ENDPOINT, "class2", "owner1",
                               list_of 
                                   (PropertyInfo(4, "prop4", 
                                                 PropertyInfo::S64, 
                                                 PropertyInfo::SCALAR))
                                   (PropertyInfo(5, "class3", 
                                                 PropertyInfo::COMPOSITE, 
                                                 3,
                                                 PropertyInfo::VECTOR)),
                               list_of(4)))
                    (ClassInfo(3, ClassInfo::OBSERVABLE, "class3", "owner2",
                               list_of 
                                   (PropertyInfo(6, "prop6", 
                                                 PropertyInfo::S64, 
                                                 PropertyInfo::SCALAR))
                                   (PropertyInfo(7, "prop7", 
                                                 PropertyInfo::STRING, 
                                                 PropertyInfo::SCALAR)),
                               list_of(6)(7)))
                    (ClassInfo(4, ClassInfo::POLICY, "class4", "owner2",
                               list_of 
                                   (PropertyInfo(9, "prop9",
                                                 PropertyInfo::STRING, 
                                                 PropertyInfo::SCALAR))
                                   (PropertyInfo(12, "class6",
                                                 PropertyInfo::COMPOSITE, 
                                                 6, 
                                                 PropertyInfo::VECTOR)),
                               list_of(9)))
                    (ClassInfo(5, ClassInfo::LOCAL_ONLY, "class5", "owner2",
                               list_of 
                                   (PropertyInfo(10, "prop10", 
                                                 PropertyInfo::STRING,
                                                 PropertyInfo::SCALAR))
                                   (PropertyInfo(11, "class4Ref", 
                                                 PropertyInfo::REFERENCE,
                                                 PropertyInfo::VECTOR)),
                               list_of(10)))
                    (ClassInfo(6, ClassInfo::POLICY, "class6", "owner2",
                               list_of 
                                   (PropertyInfo(13, "prop12",
                                                 PropertyInfo::STRING, 
                                                 PropertyInfo::SCALAR)),
                               list_of(12)))
                    (ClassInfo(7, ClassInfo::LOCAL_ONLY, "class7", "owner2",
                               list_of
                                   (PropertyInfo(14, "prop14",
                                                 PropertyInfo::ENUM8,
                                                 PropertyInfo::SCALAR,
                                                 EnumInfo("PlatformAdminState",
                                                          list_of
                                                              (ConstInfo("off", 0))
                                                              (ConstInfo("on", 1))))),
                               list_of(14)))
             ) { }

    ModelMetadata md;
};

} /* namespace modb */
} /* namespace opflex */

#endif /* MODB_TEST_MDFIXTURE_H */
