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

#include "opflex/engine/internal/OpflexMessage.h"

namespace opflex {
namespace engine {
namespace internal {

using rapidjson::StringBuffer;
using rapidjson::Writer;

OpflexMessage::OpflexMessage(const std::string& method_, MessageType type_) 
    : method(method_), type(type_) {

}

StringBuffer* OpflexMessage::serialize() {
    StringBuffer* sb = new StringBuffer();
    Writer<StringBuffer> writer(*sb);
    writer.StartObject();
    switch (type) {
    case REQUEST:
        writer.String("method");
        writer.String(method.c_str());
        writer.String("params");
        serializePayload(writer);
        break;
    case RESPONSE:
        writer.String("result");
        serializePayload(writer);
        break;
    case ERROR_RESPONSE:
        writer.String("error");
        serializePayload(writer);
        break;
    }
    writer.String("id");
    writer.String(method.c_str());
    writer.EndObject();

    // we delimit our frames with a nul byte
    sb->Put('\0');

    return sb;
}

} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */
