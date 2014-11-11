/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for MockOpflexServer
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <boost/foreach.hpp>

#include "opflex/engine/internal/OpflexMessage.h"

#include "MockOpflexServer.h"
#include "MockServerHandler.h"

namespace opflex {
namespace engine {
namespace internal {

using rapidjson::Value;
using rapidjson::Writer;
using modb::mointernal::StoreClient;

MockOpflexServer::MockOpflexServer(int port_, uint8_t roles_, 
                                   peer_vec_t peers_,
                                   const modb::ModelMetadata& md)
    : port(port_), roles(roles_), peers(peers_),
      listener(*this, port_, "name", "domain"), 
      serializer(&db) {
    db.init(md);
    db.start();
    client = &db.getStoreClient("_SYSTEM_");
}

MockOpflexServer::~MockOpflexServer() {
    listener.disconnect();
    db.stop();
}

OpflexHandler* MockOpflexServer::newHandler(OpflexConnection* conn) {
    return new MockServerHandler(conn, this);
}

class PolicyUpdateReq : public OpflexMessage {
public:
    PolicyUpdateReq(MockOpflexServer& server_,
                    const std::vector<modb::reference_t>& replace_,
                    const std::vector<modb::reference_t>& merge_children_,
                    const std::vector<modb::reference_t>& del_)
        : OpflexMessage("policy_update", REQUEST),
          server(server_),
          replace(replace_), 
          merge_children(merge_children_),
          del(del_) {}

    virtual void serializePayload(MessageWriter& writer) {
        (*this)(writer);
    }

    template <typename T>
    bool operator()(Writer<T> & writer) {
        MOSerializer& serializer = server.getSerializer();
        modb::mointernal::StoreClient* client = server.getSystemClient();

        writer.StartArray();
        writer.StartObject();

        writer.String("replace");
        writer.StartArray();
        BOOST_FOREACH(modb::reference_t& p, replace) {
            serializer.serialize(p.first, p.second, 
                                 *client, writer,
                                 true);
        }
        writer.EndArray();

        writer.String("merge-children");
        writer.StartArray();
        BOOST_FOREACH(modb::reference_t& p, replace) {
            serializer.serialize(p.first, p.second, 
                                 *client, writer,
                                 false);
        }
        writer.EndArray();

        writer.String("delete");
        writer.StartArray();
        BOOST_FOREACH(modb::reference_t& p, del) {
            const modb::ClassInfo& ci = 
                server.getStore().getClassInfo(p.first);
            writer.StartObject();
            writer.String("subject");
            writer.String(ci.getName().c_str());
            writer.String("uri");
            writer.String(p.second.toString().c_str());
            writer.EndObject();
        }
        writer.EndArray();

        writer.EndObject();
        writer.EndArray();
        return true;
    }

protected:
    MockOpflexServer& server;
    std::vector<modb::reference_t> replace;
    std::vector<modb::reference_t> merge_children;
    std::vector<modb::reference_t> del;
};

void MockOpflexServer::policyUpdate(const std::vector<modb::reference_t>& replace,
                                    const std::vector<modb::reference_t>& merge_children,
                                    const std::vector<modb::reference_t>& del) {
    PolicyUpdateReq req(*this, replace, merge_children, del);
    listener.writeToAll(req);
}

} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */
