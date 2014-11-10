/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file ProcessorMessage.h
 * @brief Interface definition for various Opflex messages used by the
 * engine
 */
/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <rapidjson/document.h>
#include <boost/foreach.hpp>

#include "opflex/engine/Processor.h"
#include "opflex/engine/internal/OpflexMessage.h"
#include "opflex/engine/internal/MOSerializer.h"

#pragma once
#ifndef OPFLEX_ENGINE_PROCESSORMESSAGE_H
#define OPFLEX_ENGINE_PROCESSORMESSAGE_H

namespace opflex {
namespace engine {
namespace internal {

/**
 * An outgoing message that references the processor
 */
class ProcessorMessage : public OpflexMessage {
public:
    /**
     * Allocate a new processor message of the given method and type
     * that references the specified processor instance
     */
    ProcessorMessage(const std::string& method, MessageType type,
                     Processor* processor_)
        : OpflexMessage(method, type), 
          processor(processor_) {}
    virtual ~ProcessorMessage() {};

protected:
    /**
     * The processor instance to use for this message
     */
    Processor* processor;
};

/**
 * Policy resolve request for a given set of references
 */
class PolicyResolveReq : protected ProcessorMessage {
public:
    PolicyResolveReq(Processor* processor,
                     const std::vector<modb::reference_t>& policies_)
        : ProcessorMessage("policy_resolve", REQUEST, processor), 
          policies(policies_) {}

    virtual void serializePayload(MessageWriter& writer) {
        (*this)(writer);
    }

    template <typename T>
    bool operator()(rapidjson::Writer<T> & writer) {
        writer.StartArray();
        BOOST_FOREACH(modb::reference_t& p, policies) {
            try {
                writer.StartObject();
                writer.String("subject");
                writer.String(processor->getStore()
                               ->getClassInfo(p.first).getName().c_str());
                writer.String("policy_uri");
                writer.String(p.second.toString().c_str());
                writer.String("prr");
                writer.Int64(3600);
                writer.EndObject();
            } catch (std::out_of_range e) {
                LOG(WARNING) << "No class found for class ID " << p.first;
            }
        }
        writer.EndArray();
        return true;
    }

protected:
    std::vector<modb::reference_t> policies;
};

/**
 * Endpoint declare request for a set of endpoints
 */
class EndpointDeclareReq : public ProcessorMessage {
public:
    EndpointDeclareReq(Processor* processor,
                       const std::vector<modb::reference_t>& endpoints_)
        : ProcessorMessage("endpoint_declare", REQUEST, processor), 
          endpoints(endpoints_) {}

    virtual void serializePayload(MessageWriter& writer) {
        (*this)(writer);
    }
    
    template <typename T>
    bool operator()(rapidjson::Writer<T> & writer) {
        MOSerializer& serializer = processor->getSerializer();
        modb::mointernal::StoreClient* client = processor->getSystemClient();

        writer.StartArray();
        writer.StartObject();

        writer.String("endpoint");
        writer.StartArray();
        BOOST_FOREACH(modb::reference_t& p, endpoints) {
            serializer.serialize(p.first, p.second, 
                                 *client, writer,
                                 true);
        }
        writer.EndArray();
        writer.String("prr");
        writer.Int64(3600);

        writer.EndObject();
        writer.EndArray();
        return true;
    }

protected:
    std::vector<modb::reference_t> endpoints;
};

class EndpointUndeclareReq : public ProcessorMessage {
public:
    EndpointUndeclareReq(Processor* processor,
                         const std::vector<modb::reference_t>& endpoints_)
        : ProcessorMessage("endpoint_undeclare", REQUEST, processor), 
          endpoints(endpoints_) {}
    
    virtual void serializePayload(MessageWriter& writer) {
        (*this)(writer);
    }
    
    template <typename T>
    bool operator()(rapidjson::Writer<T> & writer) {
        MOSerializer& serializer = processor->getSerializer();
        modb::mointernal::StoreClient* client = processor->getSystemClient();

        writer.StartArray();

        BOOST_FOREACH(modb::reference_t& p, endpoints) {
            try {
                writer.StartObject();
                writer.String("subject");
                writer.String(processor->getStore()
                               ->getClassInfo(p.first).getName().c_str());
                writer.String("endpoint_uri");
                writer.String(p.second.toString().c_str());
                writer.EndObject();
            }  catch (std::out_of_range e) {
                LOG(WARNING) << "No class found for class ID " << p.first;
            }
        }

        writer.EndArray();
        return true;
    }

protected:
    std::vector<modb::reference_t> endpoints;
};

class EndpointUpdateReq : public ProcessorMessage {
public:
    EndpointUpdateReq(Processor* processor,
                      const std::vector<modb::reference_t>& replace_,
                      const std::vector<modb::URI>& del_)
        : ProcessorMessage("endpoint_update", REQUEST, processor), 
          replace(replace_), del(del_) {}
    
    virtual void serializePayload(MessageWriter& writer) {
        (*this)(writer);
    }
    
    template <typename T>
    bool operator()(rapidjson::Writer<T> & writer) {
        MOSerializer& serializer = processor->getSerializer();
        modb::mointernal::StoreClient* client = processor->getSystemClient();

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
        BOOST_FOREACH(modb::URI& u, del) {
            writer.String(u.toString().c_str());
        }
        writer.EndArray();

        writer.EndObject();
        writer.EndArray();
        return true;
    }

protected:
    std::vector<modb::reference_t> replace;
    std::vector<modb::URI> del;
};

#if 0

class PolicyUnresolve : protected OpflexPEMessage {
public:
    PolicyUnresolve(OpflexPEHandler& pehandler,
                    const vector<reference_t>& policies_)
        : OpflexPEMessage(pehandler), policies(policies_) {}
    
    bool operator()(SendHandler & writer) {
        Processor* processor = pehandler.getProcessor();
        writer.StartArray();
        BOOST_FOREACH(reference_t& p, policies) {
            try {
                writer.StartObject();
                writer.String("subject");
                writer.String(processor->getStore()
                               ->getClassInfo(p.first).getName().c_str());
                writer.String("policy_uri");
                writer.String(p.second.toString().c_str());
                writer.EndObject();
            } catch (std::out_of_range e) {
                LOG(WARNING) << "No class found for class ID " << p.first;
            }
        }
        writer.EndArray();
        return true;
    }

protected:
    vector<reference_t> policies;
};

class PolicyUpdate : protected OpflexPEMessage {
public:
    PolicyUpdate(OpflexPEHandler& pehandler,
                 const vector<reference_t>& replace_,
                 const vector<reference_t>& merge_children_,
                 const vector<URI>& del_)
        : OpflexPEMessage(pehandler), 
          replace(replace_), 
          merge_children(merge_children_),
          del(del_) {}
    
    bool operator()(SendHandler & writer) {
        Processor* processor = pehandler.getProcessor();
        MOSerializer& serializer = processor->getSerializer();
        StoreClient* client = processor->getSystemClient();

        writer.StartArray();
        writer.StartObject();

        writer.String("replace");
        writer.StartArray();
        BOOST_FOREACH(reference_t& p, replace) {
            serializer.serialize(p.first, p.second, 
                                 *client, writer,
                                 true);
        }
        writer.EndArray();

        writer.String("merge-children");
        writer.StartArray();
        BOOST_FOREACH(reference_t& p, replace) {
            serializer.serialize(p.first, p.second, 
                                 *client, writer,
                                 false);
        }
        writer.EndArray();

        writer.String("delete");
        writer.StartArray();
        BOOST_FOREACH(URI& u, del) {
            writer.String(u.toString().c_str());
        }
        writer.EndArray();

        writer.EndObject();
        writer.EndArray();
        return true;
    }

protected:
    vector<reference_t> replace;
    vector<reference_t> merge_children;
    vector<URI> del;
};

class StateReport : protected OpflexPEMessage {
public:
    StateReport(OpflexPEHandler& pehandler,
                const vector<reference_t>& observables_)
        : OpflexPEMessage(pehandler), 
          observables(observables_) {}
    
    bool operator()(SendHandler & writer) {
        Processor* processor = pehandler.getProcessor();
        MOSerializer& serializer = processor->getSerializer();
        StoreClient* client = processor->getSystemClient();

        writer.StartArray();
        writer.StartObject();

        writer.String("observable");
        writer.StartArray();
        BOOST_FOREACH(reference_t& p, observables) {
            serializer.serialize(p.first, p.second, 
                                 *client, writer,
                                 true);
        }
        writer.EndArray();

        writer.EndObject();
        writer.EndArray();
        return true;
    }

protected:
    vector<reference_t> observables;
};

#endif

} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */

#endif /* OPFLEX_ENGINE_PROCESSORMESSAGE_H */
