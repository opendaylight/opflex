/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation of opflex messages for engine
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <vector>
#include <utility>

#include <boost/foreach.hpp>

#include "opflex/engine/OpFlexHandler.h"
#include "opflex/engine/Processor.h"
#include "opflex/engine/internal/MOSerializer.h"

#include "opflex/logging/internal/logging.hpp"
#include "opflex/comms/comms.hpp"
#include "opflex/rpc/rpc.hpp"
#include "opflex/rpc/methods.hpp"

namespace opflex {
namespace engine {

using std::vector;
using std::pair;
using comms::internal::CommunicationPeer;
using rpc::SendHandler;
using modb::class_id_t;
using modb::reference_t;
using modb::URI;
using internal::MOSerializer;
using modb::mointernal::StoreClient;
using opflex::rpc::InbReq;
using opflex::rpc::InbErr;

class OpFlexMessage {
public:
    OpFlexMessage(CommunicationPeer const & peer_,
                  Processor* processor_) 
        : peer(peer_), processor(processor_) {}
    virtual ~OpFlexMessage() {};
 
protected:
    CommunicationPeer const & peer;
    Processor* processor;
};

class SendIdentity : protected OpFlexMessage {
public:
    SendIdentity(CommunicationPeer const & peer,
                 Processor* processor) 
        : OpFlexMessage(peer, processor) {}

    bool operator()(SendHandler & handler) {
        return  handler.StartArray()
            &&  handler.StartObject()
            &&  handler.String("proto_version")
            &&  handler.String("1.0")
            &&  handler.String("model")
            &&  handler.String(processor->getStore()->getModelName().c_str())
            &&  handler.String("model_version")
            &&  handler.String(processor->getStore()->getModelVersion().c_str())
            &&  handler.String("my_role")
            &&  handler.StartArray()
            &&  handler.String("policy_element")
            &&  handler.EndArray()
            &&  handler.EndObject()
            &&  handler.EndArray()
        ;
    }
};

class PolicyResolve : protected OpFlexMessage {
public:
    PolicyResolve(CommunicationPeer const & peer,
                  Processor* processor,
                  const vector<reference_t>& policies_)
        : OpFlexMessage(peer, processor), policies(policies_) {}
    
    bool operator()(SendHandler & handler) {
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
    PolicyUnresolve(CommunicationPeer const & peer,
                    Processor* processor,
                    const vector<reference_t>& policies_)
        : OpFlexMessage(peer, processor), policies(policies_) {}
    
    bool operator()(SendHandler & handler) {
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
    PolicyUpdate(CommunicationPeer const & peer,
                 Processor* processor,
                 const vector<reference_t>& replace_,
                 const vector<reference_t>& merge_children_,
                 const vector<URI>& del_)
        : OpFlexMessage(peer, processor), 
          replace(replace_), 
          merge_children(merge_children_),
          del(del_) {}
    
    bool operator()(SendHandler & handler) {
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
    EndpointDeclare(CommunicationPeer const & peer,
                 Processor* processor,
                 const vector<reference_t>& endpoints_)
        : OpFlexMessage(peer, processor), 
          endpoints(endpoints_) {}
    
    bool operator()(SendHandler & handler) {
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
    EndpointUndeclare(CommunicationPeer const & peer,
                      Processor* processor,
                      const vector<reference_t>& endpoints_)
        : OpFlexMessage(peer, processor), 
          endpoints(endpoints_) {}
    
    bool operator()(SendHandler & handler) {
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
    EndpointUpdate(CommunicationPeer const & peer,
                   Processor* processor,
                   const vector<reference_t>& replace_,
                   const vector<URI>& del_)
        : OpFlexMessage(peer, processor), 
          replace(replace_), del(del_) {}
    
    bool operator()(SendHandler & handler) {
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
    StateReport(CommunicationPeer const & peer,
                Processor* processor,
                const vector<reference_t>& observables_)
        : OpFlexMessage(peer, processor), 
          observables(observables_) {}
    
    bool operator()(SendHandler & handler) {
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

} /* namespace engine */
namespace rpc {

template<>
void InbReq<&opflex::rpc::method::send_identity>::process() const {

}
template<>
void InbRes<&opflex::rpc::method::send_identity>::process() const {

}
template<>
void InbErr<&opflex::rpc::method::send_identity>::process() const {

}

template<>
void InbReq<&opflex::rpc::method::policy_resolve>::process() const {

}
template<>
void InbRes<&opflex::rpc::method::policy_resolve>::process() const {

}
template<>
void InbErr<&opflex::rpc::method::policy_resolve>::process() const {

}


template<>
void InbReq<&opflex::rpc::method::policy_unresolve>::process() const {

}
template<>
void InbRes<&opflex::rpc::method::policy_unresolve>::process() const {

}
template<>
void InbErr<&opflex::rpc::method::policy_unresolve>::process() const {

}

template<>
void InbReq<&opflex::rpc::method::policy_update>::process() const {

}
template<>
void InbRes<&opflex::rpc::method::policy_update>::process() const {

}
template<>
void InbErr<&opflex::rpc::method::policy_update>::process() const {

}

template<>
void InbReq<&opflex::rpc::method::endpoint_declare>::process() const {

}
template<>
void InbRes<&opflex::rpc::method::endpoint_declare>::process() const {

}
template<>
void InbErr<&opflex::rpc::method::endpoint_declare>::process() const {

}

template<>
void InbReq<&opflex::rpc::method::endpoint_undeclare>::process() const {

}
template<>
void InbRes<&opflex::rpc::method::endpoint_undeclare>::process() const {

}
template<>
void InbErr<&opflex::rpc::method::endpoint_undeclare>::process() const {

}

template<>
void InbReq<&opflex::rpc::method::endpoint_resolve>::process() const {

}
template<>
void InbRes<&opflex::rpc::method::endpoint_resolve>::process() const {

}
template<>
void InbErr<&opflex::rpc::method::endpoint_resolve>::process() const {

}

template<>
void InbReq<&opflex::rpc::method::endpoint_unresolve>::process() const {

}
template<>
void InbRes<&opflex::rpc::method::endpoint_unresolve>::process() const {

}
template<>
void InbErr<&opflex::rpc::method::endpoint_unresolve>::process() const {

}

template<>
void InbReq<&opflex::rpc::method::endpoint_update>::process() const {

}
template<>
void InbRes<&opflex::rpc::method::endpoint_update>::process() const {

}
template<>
void InbErr<&opflex::rpc::method::endpoint_update>::process() const {

}

template<>
void InbReq<&opflex::rpc::method::state_report>::process() const {

}
template<>
void InbRes<&opflex::rpc::method::state_report>::process() const {

}
template<>
void InbErr<&opflex::rpc::method::state_report>::process() const {

}


} /* namespace rpc */
} /* namespace opflex */
