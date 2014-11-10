/*
 * Test main for enforcer.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#define BOOST_TEST_MODULE "enforcer"
#include <boost/test/unit_test.hpp>

#include <glog/logging.h>

struct LogInitializer {
public:
    LogInitializer() {
        google::InitGoogleLogging("");
        FLAGS_logtostderr = 0;      // set to 1 to enable logging
    }
};

static LogInitializer logInit;
