/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Supported Faults.
 *
 * Copyright (c) 2019-2020 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef __OPFLEXAGENT_FAULTS_H__
#define __OPFLEXAGENT_FAULTS_H__

namespace opflexagent {

    enum FaultCodes {
        SAMPLE_FAULT = 1,
        ENCAP_MISMATCH = 2,
        TEP_ENCAP_MISMATCH = 3
    };
}

#endif