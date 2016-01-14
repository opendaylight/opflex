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

/* This must be included before anything else */
#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include <cstdio>

#include <boost/foreach.hpp>

#include "opflex/test/MockOpflexServer.h"
#include "opflex/engine/internal/OpflexMessage.h"
#include "opflex/engine/internal/MockOpflexServerImpl.h"
#include "opflex/engine/internal/MockServerHandler.h"

namespace opflex {
namespace test {

MockOpflexServer::MockOpflexServer(int port, uint8_t roles,
                                   peer_vec_t peers,
                                   const modb::ModelMetadata& md)
    : pimpl(new engine::internal
            ::MockOpflexServerImpl(port, roles, peers, md)) { }

MockOpflexServer::~MockOpflexServer() {
    delete pimpl;
}

void MockOpflexServer::enableSSL(const std::string& caStorePath,
                                 const std::string& serverKeyPath,
                                 const std::string& serverKeyPass,
                                 bool verifyPeers) {
    pimpl->enableSSL(caStorePath, serverKeyPath,
                     serverKeyPass, verifyPeers);
}
void MockOpflexServer::start() {
    pimpl->start();
}
void MockOpflexServer::stop() {
    pimpl->stop();
}

void MockOpflexServer::readPolicy(const std::string& file) {
    pimpl->readPolicy(file);
}

const MockOpflexServer::peer_vec_t& MockOpflexServer::getPeers() const {
    return pimpl->getPeers();
};
int MockOpflexServer::getPort() const { return pimpl->getPort(); };
uint8_t MockOpflexServer::getRoles() const { return pimpl->getRoles(); };

} /* namespace test */

namespace engine {
namespace internal {

using rapidjson::Value;
using rapidjson::Writer;
using modb::mointernal::StoreClient;
using test::MockOpflexServer;

MockOpflexServerImpl::MockOpflexServerImpl(int port_, uint8_t roles_,
                                           MockOpflexServer::peer_vec_t peers_,
                                           const modb::ModelMetadata& md)
    : port(port_), roles(roles_), peers(peers_),
      listener(*this, port_, "name", "domain"),
      db(threadManager), serializer(&db) {
    db.init(md);
    client = &db.getStoreClient("_SYSTEM_");
}

MockOpflexServerImpl::~MockOpflexServerImpl() {

}

void MockOpflexServerImpl::enableSSL(const std::string& caStorePath,
                                     const std::string& serverKeyPath,
                                     const std::string& serverKeyPass,
                                     bool verifyPeers) {
    listener.enableSSL(caStorePath, serverKeyPath,
                       serverKeyPass, verifyPeers);
}

void MockOpflexServerImpl::start() {
    db.start();
    listener.listen();
}

void MockOpflexServerImpl::stop() {
    listener.disconnect();
    db.stop();
    client = NULL;
}

void MockOpflexServerImpl::readPolicy(const std::string& file) {
    FILE* pfile = fopen(file.c_str(), "r");
    if (pfile == NULL) {
        LOG(ERROR) << "Could not open policy file "
                   << file << " for reading";
        return;
    }

    size_t objs = serializer.readMOs(pfile, *getSystemClient());
    LOG(INFO) << "Read " << objs
              << " managed objects from policy file \"" << file << "\"";
}

OpflexHandler* MockOpflexServerImpl::newHandler(OpflexConnection* conn) {
    return new MockServerHandler(conn, this);
}

class PolicyUpdateReq : public OpflexMessage {
public:
    PolicyUpdateReq(MockOpflexServerImpl& server_,
                    const std::vector<modb::reference_t>& replace_,
                    const std::vector<modb::reference_t>& merge_children_,
                    const std::vector<modb::reference_t>& del_)
        : OpflexMessage("policy_update", REQUEST),
          server(server_),
          replace(replace_),
          merge_children(merge_children_),
          del(del_) {}

    virtual void serializePayload(yajr::rpc::SendHandler& writer) {
        (*this)(writer);
    }

    virtual void serializePayload(MessageWriter& writer) {
        (*this)(writer);
    }

    virtual PolicyUpdateReq* clone() {
        return new PolicyUpdateReq(*this);
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

        writer.String("merge_children");
        writer.StartArray();
        BOOST_FOREACH(modb::reference_t& p, merge_children) {
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
    MockOpflexServerImpl& server;
    std::vector<modb::reference_t> replace;
    std::vector<modb::reference_t> merge_children;
    std::vector<modb::reference_t> del;
};

void MockOpflexServerImpl::policyUpdate(const std::vector<modb::reference_t>& replace,
                                    const std::vector<modb::reference_t>& merge_children,
                                    const std::vector<modb::reference_t>& del) {
    PolicyUpdateReq* req =
        new PolicyUpdateReq(*this, replace, merge_children, del);
    listener.sendToAll(req);
}

class EndpointUpdateReq : public OpflexMessage {
public:
    EndpointUpdateReq(MockOpflexServerImpl& server_,
                    const std::vector<modb::reference_t>& replace_,
                    const std::vector<modb::reference_t>& del_)
        : OpflexMessage("endpoint_update", REQUEST),
          server(server_),
          replace(replace_),
          del(del_) {}

    virtual void serializePayload(yajr::rpc::SendHandler& writer) {
        (*this)(writer);
    }

    virtual void serializePayload(MessageWriter& writer) {
        (*this)(writer);
    }

    virtual EndpointUpdateReq* clone() {
        return new EndpointUpdateReq(*this);
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
    MockOpflexServerImpl& server;
    std::vector<modb::reference_t> replace;
    std::vector<modb::reference_t> del;
};

void MockOpflexServerImpl::endpointUpdate(const std::vector<modb::reference_t>& replace,
                                          const std::vector<modb::reference_t>& del) {
    EndpointUpdateReq* req = new EndpointUpdateReq(*this, replace, del);
    listener.sendToAll(req);
}

} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */
