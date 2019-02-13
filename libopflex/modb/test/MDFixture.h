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
                                   (PropertyInfo(24, "class5",
                                                 PropertyInfo::COMPOSITE, 
                                                 5, 
                                                 PropertyInfo::VECTOR))
                                   (PropertyInfo(22, "class8",
                                                 PropertyInfo::COMPOSITE, 
                                                 4, 
                                                 PropertyInfo::VECTOR))
                                   (PropertyInfo(23, "class9",
                                                 PropertyInfo::COMPOSITE, 
                                                 9, 
                                                 PropertyInfo::VECTOR))))
                    (ClassInfo(2, ClassInfo::LOCAL_ENDPOINT, "class2", "owner1",
                               list_of 
                                   (PropertyInfo(4, "prop4", 
                                                 PropertyInfo::S64, 
                                                 PropertyInfo::SCALAR))
                                   (PropertyInfo(15, "prop15", 
                                                 PropertyInfo::MAC, 
                                                 PropertyInfo::SCALAR))
                                   (PropertyInfo(5, "class3", 
                                                 PropertyInfo::COMPOSITE, 
                                                 3,
                                                 PropertyInfo::VECTOR))))
                    (ClassInfo(3, ClassInfo::OBSERVABLE, "class3", "owner2",
                               list_of 
                                   (PropertyInfo(6, "prop6", 
                                                 PropertyInfo::S64, 
                                                 PropertyInfo::SCALAR))
                                   (PropertyInfo(7, "prop7", 
                                                 PropertyInfo::STRING, 
                                                 PropertyInfo::SCALAR))
                                   (PropertyInfo(16, "prop16", 
                                                 PropertyInfo::STRING, 
                                                 PropertyInfo::SCALAR))))
                    (ClassInfo(4, ClassInfo::POLICY, "class4", "owner2",
                               list_of 
                                   (PropertyInfo(9, "prop9",
                                                 PropertyInfo::STRING, 
                                                 PropertyInfo::SCALAR))
                                   (PropertyInfo(12, "class6",
                                                 PropertyInfo::COMPOSITE, 
                                                 6, 
                                                 PropertyInfo::VECTOR))
                                   (PropertyInfo(25, "class7",
                                                 PropertyInfo::COMPOSITE, 
                                                 7, 
                                                 PropertyInfo::VECTOR))))
                    (ClassInfo(5, ClassInfo::RELATIONSHIP, "class5", "owner2",
                               list_of 
                                   (PropertyInfo(10, "prop10", 
                                                 PropertyInfo::STRING,
                                                 PropertyInfo::SCALAR))
                                   (PropertyInfo(11, "class4Ref", 
                                                 PropertyInfo::REFERENCE,
                                                 PropertyInfo::VECTOR))))
                    (ClassInfo(6, ClassInfo::POLICY, "class6", "owner2",
                               list_of 
                                   (PropertyInfo(13, "prop13",
                                                 PropertyInfo::STRING, 
                                                 PropertyInfo::SCALAR))))
                    (ClassInfo(7, ClassInfo::LOCAL_ONLY, "class7", "owner2",
                               list_of
                                   (PropertyInfo(14, "prop14",
                                                 PropertyInfo::ENUM8,
                                                 PropertyInfo::SCALAR,
                                                 EnumInfo("PlatformAdminState",
                                                          list_of
                                                              (ConstInfo("off", 0))
                                                              (ConstInfo("on", 1)))))))
                    (ClassInfo(8, ClassInfo::REMOTE_ENDPOINT, "class8", "owner2",
                               list_of 
                                   (PropertyInfo(17, "prop17",
                                                 PropertyInfo::STRING, 
                                                 PropertyInfo::SCALAR))
                                   (PropertyInfo(20, "class10",
                                                 PropertyInfo::COMPOSITE, 
                                                 10, 
                                                 PropertyInfo::VECTOR))))
                    (ClassInfo(9, ClassInfo::RELATIONSHIP, "class9", "owner2",
                               list_of 
                                   (PropertyInfo(18, "prop18", 
                                                 PropertyInfo::STRING,
                                                 PropertyInfo::SCALAR))
                                   (PropertyInfo(19, "class8Ref", 
                                                 PropertyInfo::REFERENCE,
                                                 PropertyInfo::SCALAR))))
                    (ClassInfo(10, ClassInfo::REMOTE_ENDPOINT, "class10", "owner2",
                               list_of 
                                   (PropertyInfo(21, "prop21",
                                                 PropertyInfo::STRING, 
                                                 PropertyInfo::SCALAR))))
             ) { }

    ModelMetadata md;
};

} /* namespace modb */
} /* namespace opflex */

#endif /* MODB_TEST_MDFIXTURE_H */
