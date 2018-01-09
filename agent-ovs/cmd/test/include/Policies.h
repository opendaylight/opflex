/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for test policy writers
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <opflex/ofcore/OFFramework.h>

#pragma once
#ifndef OPFLEXAGENT_TEST_POLICIES_H
#define OPFLEXAGENT_TEST_POLICIES_H

namespace opflexagent {

/**
 * Write sample policies to an MODB
 */
class Policies {
public:
    static void writeBasicInit(opflex::ofcore::OFFramework& framework);
    static void writeTestPolicy(opflex::ofcore::OFFramework& framework);

};

} /* namespace opflexagent */

#endif /* OPFLEXAGENT_TEST_POLICIES_H */
