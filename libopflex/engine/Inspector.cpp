/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for Inspector class.
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

#include <cstdio>

#include "opflex/modb/internal/ObjectStore.h"
#include "opflex/engine/internal/InspectorServerHandler.h"
#include "opflex/engine/Inspector.h"

namespace opflex {
namespace engine {

using opflex::modb::ObjectStore;
using internal::OpflexHandler;
using internal::InspectorServerHandler;
using internal::OpflexConnection;
using internal::OpflexListener;
using std::string;

Inspector::Inspector(ObjectStore* db_)
    : db(db_), serializer(db_) {

}

Inspector::~Inspector() {
    stop();
}

void Inspector::setSocketName(const std::string& name) {
    this->name = name;
}

void Inspector::start() {
    LOG(INFO) << "Starting inspector on \"" << name << "\"";
    std::remove(name.c_str());
    listener.reset(new OpflexListener(*this, name, "inspector", "inspector"));
    listener->listen();
}

void Inspector::stop() {
    listener->disconnect();
}

OpflexHandler* Inspector::newHandler(OpflexConnection* conn) {
    return new InspectorServerHandler(conn, this);
}

} /* namespace engine */
} /* namespace opflex */
