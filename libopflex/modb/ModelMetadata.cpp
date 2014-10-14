/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for ModelMetadata class.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "opflex/modb/ModelMetadata.h"

namespace opflex {
namespace modb {

ModelMetadata::ModelMetadata(const std::string& name,
                             const std::vector<ClassInfo>& classes_)
    : classes(classes_) {
    
}

ModelMetadata::~ModelMetadata() {
}

} /* namespace modb */
} /* namespace opflex */
