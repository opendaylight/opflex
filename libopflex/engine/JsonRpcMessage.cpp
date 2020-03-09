/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for JsonRpcMessage
 *
 * Copyright (c) 2020 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

/* This must be included before anything else */
#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include "opflex/rpc/JsonRpcMessage.h"

namespace opflex {
namespace jsonrpc {

JsonRpcMessage::JsonRpcMessage(const std::string& method_, MessageType type_,
                               const rapidjson::Value* id_)
    : method(method_), type(type_), id(id_) {
}

} /* namespace jsonrpc */
} /* namespace opflex */