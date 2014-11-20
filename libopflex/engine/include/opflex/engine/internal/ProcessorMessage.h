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
class PolicyResolveReq : public ProcessorMessage {
public:
    /**
     * Construct a new policy request for the given list of references
     *
     * @param processor the processor for the request
     * @param policies_ the policies that should be requested
     */
    PolicyResolveReq(Processor* processor,
                     const std::vector<modb::reference_t>& policies_)
        : ProcessorMessage("policy_resolve", REQUEST, processor), 
          policies(policies_) {}

#ifndef SIMPLE_RPC
    virtual void serializePayload(yajr::rpc::SendHandler& writer) {
        (*this)(writer);
    }
#endif

    virtual void serializePayload(MessageWriter& writer) {
        (*this)(writer);
    }

    virtual PolicyResolveReq* clone() {
        return new PolicyResolveReq(*this);
    }

    /**
     * Operator that will serialize the message to the given writer.
     *
     * @param writer the rapidjson writer
     */
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

private:
    std::vector<modb::reference_t> policies;
};

/**
 * A policy unresolve request message
 */
class PolicyUnresolveReq : public ProcessorMessage {
public:
    /**
     * Construct a new policy unresolve for the given list of references
     *
     * @param processor the processor for the request
     * @param policies_ the policies that should be unresolved
     */
    PolicyUnresolveReq(Processor* processor,
                       const std::vector<modb::reference_t>& policies_)
        : ProcessorMessage("policy_unresolve", REQUEST, processor), 
          policies(policies_) {}

#ifndef SIMPLE_RPC
    virtual void serializePayload(yajr::rpc::SendHandler& writer) {
        (*this)(writer);
    }
#endif

    virtual void serializePayload(MessageWriter& writer) {
        (*this)(writer);
    }

    virtual PolicyUnresolveReq* clone() { 
        return new PolicyUnresolveReq(*this);
    }

    /**
     * Operator that will serialize the message to the given writer.
     *
     * @param writer the rapidjson writer
     */
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
                writer.EndObject();
            } catch (std::out_of_range e) {
                LOG(WARNING) << "No class found for class ID " << p.first;
            }
        }
        writer.EndArray();
        return true;
    }

private:
    std::vector<modb::reference_t> policies;
};

/**
 * Endpoint resolve request for a given set of references
 */
class EndpointResolveReq : public ProcessorMessage {
public:
    /**
     * Construct a new endpoint resolve request for the given list of
     * references
     *
     * @param processor the processor for the request
     * @param endpoints_ the endpoints that should be requested
     */
    EndpointResolveReq(Processor* processor,
                     const std::vector<modb::reference_t>& endpoints_)
        : ProcessorMessage("endpoint_resolve", REQUEST, processor), 
          endpoints(endpoints_) {}

#ifndef SIMPLE_RPC
    virtual void serializePayload(yajr::rpc::SendHandler& writer) {
        (*this)(writer);
    }
#endif

    virtual void serializePayload(MessageWriter& writer) {
        (*this)(writer);
    }

    virtual EndpointResolveReq* clone() { 
        return new EndpointResolveReq(*this);
    }

    /**
     * Operator that will serialize the message to the given writer.
     *
     * @param writer the rapidjson writer
     */
    template <typename T>
    bool operator()(rapidjson::Writer<T> & writer) {
        writer.StartArray();
        BOOST_FOREACH(modb::reference_t& p, endpoints) {
            try {
                writer.StartObject();
                writer.String("subject");
                writer.String(processor->getStore()
                               ->getClassInfo(p.first).getName().c_str());
                writer.String("endpoint_uri");
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

private:
    std::vector<modb::reference_t> endpoints;
};

/**
 * Endpoint unresolve message
 */
class EndpointUnresolveReq : public ProcessorMessage {
public:
    /**
     * Construct a new endpoint unresolve request for the given list
     * of references
     *
     * @param processor the processor for the request
     * @param endpoints_ the endpoints that should be unresolved
     */
    EndpointUnresolveReq(Processor* processor,
                       const std::vector<modb::reference_t>& endpoints_)
        : ProcessorMessage("endpoint_unresolve", REQUEST, processor), 
          endpoints(endpoints_) {}

#ifndef SIMPLE_RPC
    virtual void serializePayload(yajr::rpc::SendHandler& writer) {
        (*this)(writer);
    }
#endif

    virtual void serializePayload(MessageWriter& writer) {
        (*this)(writer);
    }

    virtual EndpointUnresolveReq* clone() { 
        return new EndpointUnresolveReq(*this);
    }

    /**
     * Operator that will serialize the message to the given writer.
     *
     * @param writer the rapidjson writer
     */
    template <typename T>
    bool operator()(rapidjson::Writer<T> & writer) {
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
            } catch (std::out_of_range e) {
                LOG(WARNING) << "No class found for class ID " << p.first;
            }
        }
        writer.EndArray();
        return true;
    }

private:
    std::vector<modb::reference_t> endpoints;
};

/**
 * Endpoint declare request for a set of endpoints
 */
class EndpointDeclareReq : public ProcessorMessage {
public:
    /**
     * Construct a new endpoint declare request for the given list of
     * endpoint references.
     *
     * @param processor the processor for the request
     * @param endpoints_ the endpoints that should be included in the
     * message
     */
    EndpointDeclareReq(Processor* processor,
                       const std::vector<modb::reference_t>& endpoints_)
        : ProcessorMessage("endpoint_declare", REQUEST, processor), 
          endpoints(endpoints_) {}

#ifndef SIMPLE_RPC
    virtual void serializePayload(yajr::rpc::SendHandler& writer) {
        (*this)(writer);
    }
#endif

    virtual void serializePayload(MessageWriter& writer) {
        (*this)(writer);
    }

    virtual EndpointDeclareReq* clone() { 
        return new EndpointDeclareReq(*this);
    }

    /**
     * Operator that will serialize the message to the given writer.
     *
     * @param writer the rapidjson writer
     */
    template <typename T>
    bool operator()(rapidjson::Writer<T> & writer) {
        MOSerializer& serializer = processor->getSerializer();
        modb::mointernal::StoreClient* client = processor->getSystemClient();

        writer.StartArray();
        writer.StartObject();

        writer.String("endpoint");
        writer.StartArray();
        BOOST_FOREACH(modb::reference_t& p, endpoints) {
            try {
                serializer.serialize(p.first, p.second, 
                                     *client, writer,
                                     true);
            } catch (std::out_of_range e) {
                // endpoint no longer exists locally
            }
        }
        writer.EndArray();
        writer.String("prr");
        writer.Int64(3600);

        writer.EndObject();
        writer.EndArray();
        return true;
    }

private:
    std::vector<modb::reference_t> endpoints;
};

/**
 * Endpoint undeclare message
 */
class EndpointUndeclareReq : public ProcessorMessage {
public:
    /**
     * Construct a new endpoint undeclare request for the given list of
     * endpoint references.
     *
     * @param processor the processor for the request
     * @param endpoints_ the endpoints that should be undeclared
     */
    EndpointUndeclareReq(Processor* processor,
                         const std::vector<modb::reference_t>& endpoints_)
        : ProcessorMessage("endpoint_undeclare", REQUEST, processor), 
          endpoints(endpoints_) {}
    
#ifndef SIMPLE_RPC
    virtual void serializePayload(yajr::rpc::SendHandler& writer) {
        (*this)(writer);
    }
#endif

    virtual void serializePayload(MessageWriter& writer) {
        (*this)(writer);
    }

    virtual EndpointUndeclareReq* clone() { 
        return new EndpointUndeclareReq(*this);
    }

    /**
     * Operator that will serialize the message to the given writer.
     *
     * @param writer the rapidjson writer
     */
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

private:
    std::vector<modb::reference_t> endpoints;
};

/**
 * State report request message
 */
class StateReportReq : public ProcessorMessage {
public:
    /**
     * Construct a new state report request for the given list of
     * observable references
     *
     * @param processor the processor for the request
     * @param observables_ the observables that should be included in
     * the message
     */
    StateReportReq(Processor* processor,
                   const std::vector<modb::reference_t>& observables_)
        : ProcessorMessage("state_report", REQUEST, processor), 
          observables(observables_) {}

#ifndef SIMPLE_RPC
    virtual void serializePayload(yajr::rpc::SendHandler& writer) {
        (*this)(writer);
    }
#endif

    virtual void serializePayload(MessageWriter& writer) {
        (*this)(writer);
    }

    virtual StateReportReq* clone() { 
        return new StateReportReq(*this);
    }

    /**
     * Operator that will serialize the message to the given writer.
     *
     * @param writer the rapidjson writer
     */
    template <typename T>
    bool operator()(rapidjson::Writer<T> & writer) {
        MOSerializer& serializer = processor->getSerializer();
        modb::mointernal::StoreClient* client = processor->getSystemClient();

        writer.StartArray();
        writer.StartObject();

        writer.String("observable");
        writer.StartArray();
        BOOST_FOREACH(modb::reference_t& p, observables) {
            serializer.serialize(p.first, p.second, 
                                 *client, writer,
                                 true);
        }
        writer.EndArray();

        writer.EndObject();
        writer.EndArray();
        return true;
    }

private:
    std::vector<modb::reference_t> observables;
};

} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */

#endif /* OPFLEX_ENGINE_PROCESSORMESSAGE_H */
