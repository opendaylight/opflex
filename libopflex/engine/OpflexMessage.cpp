/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for OpflexMessage
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


#include "opflex/engine/internal/OpflexMessage.h"

namespace opflex {
namespace engine {
namespace internal {

OpflexMessage::OpflexMessage(const std::string& method_, MessageType type_,
                             const rapidjson::Value* id_) 
    : method(method_), type(type_), id(id_) {
}

void GenericOpflexMessage::serializePayload(yajr::rpc::SendHandler& writer) {
    (*this)(writer);
}

void GenericOpflexMessage::serializePayload(MessageWriter& writer) {
    (*this)(writer);
}

} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */
