/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for OpflexPEHandler
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <string>
#include <vector>
#include <utility>

#include <rapidjson/document.h>
#include <boost/foreach.hpp>

#include "opflex/engine/Processor.h"
#include "opflex/engine/internal/OpflexPool.h"
#include "opflex/engine/internal/OpflexConnection.h"
#include "opflex/engine/internal/OpflexPEHandler.h"
#include "opflex/logging/internal/logging.hpp"
#include "opflex/comms/comms.hpp"
#include "opflex/rpc/rpc.hpp"
#include "opflex/rpc/methods.hpp"
#include "opflex/engine/internal/MOSerializer.h"

namespace opflex {
namespace engine {
namespace internal {

using std::vector;
using std::string;
using std::pair;
using rpc::SendHandler;
using modb::class_id_t;
using modb::reference_t;
using modb::URI;
using internal::MOSerializer;
using modb::mointernal::StoreClient;
using opflex::rpc::InbReq;
using opflex::rpc::InbErr;
using rapidjson::Value;

class OpFlexMessage {
public:
    OpFlexMessage(OpflexPEHandler& pehandler_)
        : pehandler(pehandler_) {}
    virtual ~OpFlexMessage() {};
 
protected:
    OpflexPEHandler& pehandler;
};

class SendIdentity {
public:
    SendIdentity(const std::string& name_,
                 const std::string& domain_,
                 const uint8_t roles_)
        : name(name_), domain(domain_), roles(roles_) {}

    bool operator()(SendHandler & handler) {
        handler.StartArray();
        handler.StartObject();
        handler.String("proto_version");
        handler.String("1.0");
        handler.String("name");
        handler.String(name.c_str());
        handler.String("domain");
        handler.String(domain.c_str());
        handler.String("my_role");
        handler.StartArray();
        if (roles & OpflexHandler::POLICY_ELEMENT)
            handler.String("policy_element");
        if (roles & OpflexHandler::POLICY_REPOSITORY)
            handler.String("policy_repository");
        if (roles & OpflexHandler::ENDPOINT_REGISTRY)
            handler.String("endpoint_registry");
        if (roles & OpflexHandler::OBSERVER)
            handler.String("observer");
        handler.EndArray();
        handler.EndObject();
        handler.EndArray();

        return true;
    }

private:
    std::string name;
    std::string domain;
    uint8_t roles;
};

class PolicyResolve : protected OpFlexMessage {
public:
    PolicyResolve(OpflexPEHandler& handler,
                  const vector<reference_t>& policies_)
        : OpFlexMessage(handler), policies(policies_) {}
    
    bool operator()(SendHandler & handler) {
        Processor* processor = pehandler.getProcessor();
        handler.StartArray();
        BOOST_FOREACH(reference_t& p, policies) {
            try {
                handler.StartObject();
                handler.String("subject");
                handler.String(processor->getStore()
                               ->getClassInfo(p.first).getName().c_str());
                handler.String("policy_uri");
                handler.String(p.second.toString().c_str());
                handler.String("prr");
                handler.Int64(3600);
                handler.EndObject();
            } catch (std::out_of_range e) {
                LOG(WARNING) << "No class found for class ID " << p.first;
            }
        }
        handler.EndArray();
        return true;
    }

protected:
    vector<reference_t> policies;
};

class PolicyUnresolve : protected OpFlexMessage {
public:
    PolicyUnresolve(OpflexPEHandler& handler,
                    const vector<reference_t>& policies_)
        : OpFlexMessage(handler), policies(policies_) {}
    
    bool operator()(SendHandler & handler) {
        Processor* processor = pehandler.getProcessor();
        handler.StartArray();
        BOOST_FOREACH(reference_t& p, policies) {
            try {
                handler.StartObject();
                handler.String("subject");
                handler.String(processor->getStore()
                               ->getClassInfo(p.first).getName().c_str());
                handler.String("policy_uri");
                handler.String(p.second.toString().c_str());
                handler.EndObject();
            } catch (std::out_of_range e) {
                LOG(WARNING) << "No class found for class ID " << p.first;
            }
        }
        handler.EndArray();
        return true;
    }

protected:
    vector<reference_t> policies;
};

class PolicyUpdate : protected OpFlexMessage {
public:
    PolicyUpdate(OpflexPEHandler& handler,
                 const vector<reference_t>& replace_,
                 const vector<reference_t>& merge_children_,
                 const vector<URI>& del_)
        : OpFlexMessage(handler), 
          replace(replace_), 
          merge_children(merge_children_),
          del(del_) {}
    
    bool operator()(SendHandler & handler) {
        Processor* processor = pehandler.getProcessor();
        MOSerializer& serializer = processor->getSerializer();
        StoreClient* client = processor->getSystemClient();

        handler.StartArray();
        handler.StartObject();

        handler.String("replace");
        handler.StartArray();
        BOOST_FOREACH(reference_t& p, replace) {
            serializer.serialize(p.first, p.second, 
                                 *client, handler,
                                 true);
        }
        handler.EndArray();

        handler.String("merge-children");
        handler.StartArray();
        BOOST_FOREACH(reference_t& p, replace) {
            serializer.serialize(p.first, p.second, 
                                 *client, handler,
                                 false);
        }
        handler.EndArray();

        handler.String("delete");
        handler.StartArray();
        BOOST_FOREACH(URI& u, del) {
            handler.String(u.toString().c_str());
        }
        handler.EndArray();

        handler.EndObject();
        handler.EndArray();
        return true;
    }

protected:
    vector<reference_t> replace;
    vector<reference_t> merge_children;
    vector<URI> del;
};

class EndpointDeclare : protected OpFlexMessage {
public:
    EndpointDeclare(OpflexPEHandler& handler,
                    const vector<reference_t>& endpoints_)
        : OpFlexMessage(handler), 
          endpoints(endpoints_) {}
    
    bool operator()(SendHandler & handler) {
        Processor* processor = pehandler.getProcessor();
        MOSerializer& serializer = processor->getSerializer();
        StoreClient* client = processor->getSystemClient();

        handler.StartArray();
        handler.StartObject();

        handler.String("endpoint");
        handler.StartArray();
        BOOST_FOREACH(reference_t& p, endpoints) {
            serializer.serialize(p.first, p.second, 
                                 *client, handler,
                                 true);
        }
        handler.EndArray();
        handler.String("prr");
        handler.Int64(3600);

        handler.EndObject();
        handler.EndArray();
        return true;
    }

protected:
    vector<reference_t> endpoints;
};

class EndpointUndeclare : protected OpFlexMessage {
public:
    EndpointUndeclare(OpflexPEHandler& handler,
                      const vector<reference_t>& endpoints_)
        : OpFlexMessage(handler), 
          endpoints(endpoints_) {}
    
    bool operator()(SendHandler & handler) {
        Processor* processor = pehandler.getProcessor();
        MOSerializer& serializer = processor->getSerializer();
        StoreClient* client = processor->getSystemClient();

        handler.StartArray();

        BOOST_FOREACH(reference_t& p, endpoints) {
            try {
                handler.StartObject();
                handler.String("subject");
                handler.String(processor->getStore()
                               ->getClassInfo(p.first).getName().c_str());
                handler.String("endpoint_uri");
                handler.String(p.second.toString().c_str());
                handler.EndObject();
            }  catch (std::out_of_range e) {
                LOG(WARNING) << "No class found for class ID " << p.first;
            }
        }

        handler.EndArray();
        return true;
    }

protected:
    vector<reference_t> endpoints;
};

class EndpointUpdate : protected OpFlexMessage {
public:
    EndpointUpdate(OpflexPEHandler& handler,
                   const vector<reference_t>& replace_,
                   const vector<URI>& del_)
        : OpFlexMessage(handler), 
          replace(replace_), del(del_) {}
    
    bool operator()(SendHandler & handler) {
        Processor* processor = pehandler.getProcessor();
        MOSerializer& serializer = processor->getSerializer();
        StoreClient* client = processor->getSystemClient();

        handler.StartArray();
        handler.StartObject();

        handler.String("replace");
        handler.StartArray();
        BOOST_FOREACH(reference_t& p, replace) {
            serializer.serialize(p.first, p.second, 
                                 *client, handler,
                                 true);
        }
        handler.EndArray();

        handler.String("delete");
        handler.StartArray();
        BOOST_FOREACH(URI& u, del) {
            handler.String(u.toString().c_str());
        }
        handler.EndArray();

        handler.EndObject();
        handler.EndArray();
        return true;
    }

protected:
    vector<reference_t> replace;
    vector<URI> del;
};

class StateReport : protected OpFlexMessage {
public:
    StateReport(OpflexPEHandler& handler,
                const vector<reference_t>& observables_)
        : OpFlexMessage(handler), 
          observables(observables_) {}
    
    bool operator()(SendHandler & handler) {
        Processor* processor = pehandler.getProcessor();
        MOSerializer& serializer = processor->getSerializer();
        StoreClient* client = processor->getSystemClient();

        handler.StartArray();
        handler.StartObject();

        handler.String("observable");
        handler.StartArray();
        BOOST_FOREACH(reference_t& p, observables) {
            serializer.serialize(p.first, p.second, 
                                 *client, handler,
                                 true);
        }
        handler.EndArray();

        handler.EndObject();
        handler.EndArray();
        return true;
    }

protected:
    vector<reference_t> observables;
};

void OpflexPEHandler::connected() {
    // XXX - TODO
}

void OpflexPEHandler::disconnected() {
    // XXX - TODO
}

void OpflexPEHandler::ready() {
    // XXX - TODO
}

void OpflexPEHandler::handleSendIdentityRes(const Value& payload) {
    // XXX - TODO
}

void OpflexPEHandler::handlePolicyResolveRes(const Value& payload) {
    // XXX - TODO
}

void OpflexPEHandler::handlePolicyUnresolveRes(const rapidjson::Value& payload) {
    // nothing to do
}

void OpflexPEHandler::handleEPDeclareRes(const rapidjson::Value& payload) {
    // nothing to do
}

void OpflexPEHandler::handleEPUndeclareRes(const rapidjson::Value& payload) {
    // nothing to do
}

void OpflexPEHandler::handleEPResolveRes(const rapidjson::Value& payload) {
    // XXX - TODO
}

void OpflexPEHandler::handleEPUnresolveRes(const rapidjson::Value& payload) {
    // nothing to do
}

void OpflexPEHandler::handleEPUpdateReq(const rapidjson::Value& payload) {
    // XXX - TODO
}

} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */
