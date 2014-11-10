/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef _____COMMS__INCLUDE__OPFLEX__RPC_HPP
#define _____COMMS__INCLUDE__OPFLEX__RPC_HPP

#include <rapidjson/document.h>
#include <opflex/logging/internal/logging.hpp>
#include <boost/intrusive/set.hpp>
#include <boost/functional/hash.hpp>
#include <boost/scoped_ptr.hpp>
#include <uv.h>
#include <rapidjson/writer.h>
#include <boost/function.hpp>
#include <yajr/rpc/send_handler.hpp>
#include <yajr/yajr.hpp>
#include <deque>

namespace yajr {

namespace rpc {

typedef rapidjson::Value::StringRefType const MethodName;
typedef rapidjson::Writer< yajr::internal::StringQueue > SendHandler;
typedef boost::function<bool (yajr::rpc::SendHandler &)> PayloadGenerator;

class GeneratorFromValue {

  public:
    GeneratorFromValue(rapidjson::Value const & v) : v_(v) {}

    bool operator()(yajr::rpc::SendHandler & h) {
        return v_.Accept(h);
    }

  private:
    rapidjson::Value const & v_;

};

class Identifier {
  public:
    virtual bool emitId(yajr::rpc::SendHandler & h) = 0;
  protected:
    virtual char const * requestMethod() = 0;
    virtual MethodName * getMethodName() = 0;
};

struct LocalId {
    LocalId(MethodName const * methodName, uint64_t id)
        :
            methodName_(methodName),
            id_(id)
        {}
    MethodName const * methodName_;
    uint64_t id_;
};

class LocalIdentifier : virtual public Identifier, private LocalId {
  public:
    LocalIdentifier(MethodName const * methodName, uint64_t id)
        :
            LocalId(methodName, id)
        {}

    bool emitId(yajr::rpc::SendHandler & h) {

        if (!h.StartArray())
            return false;

        if (!h.String(requestMethod())) {
            return false;
        }

        if (!h.Uint64(id_)) {
            return false;
        }

        return h.EndArray();
    }

    LocalId const getLocalId() const {
        return LocalId(methodName_, id_);
    }

  protected:
    char const * requestMethod() {
        return methodName_->s;
    }
    MethodName * getMethodName() {
        return methodName_;
    }
};

class RemoteIdentifier : virtual public Identifier {
  public:
    RemoteIdentifier(rapidjson::Value const & id) : id_(id) {}
    bool emitId(yajr::rpc::SendHandler & h) {
        return id_.Accept(h);
    }
    rapidjson::Value const & getRemoteId() const {
        return id_;
    }
  protected:
    char const * requestMethod() {
        return NULL;
    }
    MethodName * getMethodName() {
        return NULL;
    }
  private:
    rapidjson::Value const & id_;
};

/**
 * @brief A generic yajr message.
 */
class Message {

  public:

    /**
     * Get the communication peer for this message.
     *
     * @return a pointer to the communication peer for this message.
     */
    yajr::Peer const * getPeer() const {
        return peer_;
    }

    virtual ~Message() {}

    typedef struct {
        char const * const params;
        char const * const result;
        char const * const error;
    } PayloadKeys;

    static PayloadKeys kPayloadKey;

  protected:

    /**
     * @brief Constructor needed by derived classes
     *
     * Construct a new yajr message. Never invoke directly, but only
     * from derived classes' constructors.
     */
    explicit Message(
            yajr::Peer const & peer
            )
        : peer_(&peer)
        {}

    /**
     * Get the uv_loop_t * for this message
     *
     * @return the uv_loop_t * for this message
     */
    uv_loop_t const * getUvLoop() const;

    /* pointer so that it can be set multiple times */
    yajr::Peer const * peer_;

};

/**
 * @brief A generic inbound yajr message.
 *
 * A generic outbound yajr message is any message from the local peer that
 * is destined to another peer.
 */
class InboundMessage : public Message {

  public:
    /**
     * Process this incoming message
     */
    virtual void process() const = 0;

    uint64_t getReceived() const {
        return received_;
    }

    /**
     * Get the payload (params, result, error) for this request.
     *
     * @return The payload as a rapidjson::Value const &
     */
    rapidjson::Value const & getPayload() const {
        return payload_;
    }

  protected:
    /**
     * @brief Constructor needed by derived classes
     *
     * Construct a new inbound yajr message. Never invoke directly, but only
     * from derived classes' constructors.
     */
    explicit InboundMessage(
            yajr::Peer const & peer,        /**< [in] where to send to */
            rapidjson::Value const & payload
            )
        :
            Message(peer),
            payload_(payload),
            received_(uv_now(getUvLoop()))
        {
            assert(&peer);
        }

  private:
    /* making the peer private so InboundMessage subclasses won't see it */
    using Message::peer_;
    rapidjson::Value const & payload_;      /** params, result, error payload */
    uint64_t received_;
};

/**
 * @brief A generic outbound yajr message.
 *
 * A generic outbound yajr message is any message from the local peer that
 * is destined to another peer.
 */
class OutboundMessage : public Message, virtual public Identifier {

  public:

    /**
     * Set the communication peer for this message.
     *
     * Set the communication peer for this message. Can be set any number of
     * times for an Outbound message. Useful to send the same message to multiple
     * peers. FIXME: handle reference counting!!!
     *
     * @return this message.
     */
    void setPeer(
        yajr::Peer const & peer                   /**< the peer to set */
        ) {
        peer_ = &peer;
    }

    /**
     * Send this message now!
     */
    void send();

    /**
     * @brief Emit events to a handler
     *
     * Emit events to a handler that will serialize it to json.
     *
     * @return true on success, false on failure.
     */
    bool Accept(yajr::rpc::SendHandler& handler);

  protected:

    /**
     * @brief Constructor needed by derived classes
     *
     * Construct a new outbound yajr message. Never invoke directly, but only
     * from derived classes' constructors.
     */
    explicit OutboundMessage(
        yajr::Peer const & peer,            /**< [in] where to send to */
        PayloadGenerator const & payloadGenerator /**< [in] payload generator */
        )
        :
          Message(peer),
          payloadGenerator_(payloadGenerator),
          sent_(uv_now(getUvLoop())),
          retryIntervalMs_(200),
          retries_(0),
          keepPayload_(false),
          hasPayload_(true)
        {}

    /**
     * @brief Get payload key
     *
     * @return one of "params", "result" or "error"
     */
    virtual char const * getPayloadKey() const = 0;

    virtual bool isRequest() const = 0;

    virtual bool emitMethod(yajr::rpc::SendHandler& handler) = 0;
  private:
    PayloadGenerator const payloadGenerator_;
    uint64_t sent_;
    uint64_t retryIntervalMs_;
    size_t retries_;
    bool keepPayload_;
    bool hasPayload_;
};

/**
 * @brief A generic outbound yajr response message.
 *
 * A generic outbound yajr response message is any message that represents a
 * response from the local peer that is destined to another peer, whether an
 * error or a result.
 */
class OutboundResponse : public OutboundMessage, public RemoteIdentifier {

  public:
    /**
     * Tell if this response is an error (or a result)
     *
     * @return true on error, false on result
     */
    virtual bool isError() = 0;

  protected:
    /**
     * @brief Constructor needed by derived classes
     *
     * Construct a new outbound yajr response message. Never invoke directly,
     * but only * from derived classes' constructors.
     */
    explicit OutboundResponse(
        yajr::Peer const & peer,            /**< [in] where to send to */
        PayloadGenerator const & response, /**< [in] the error/result to send */
        rapidjson::Value const & id /**< [in] the desired id for this message */
        )
        :
            OutboundMessage(peer, response),
            RemoteIdentifier(id)
        {
        }

    /**
     * Tell if this is a request
     *
     * @return false, since this is a response
     */
    virtual bool isRequest() const {
        return false;
    }

    virtual bool emitMethod(yajr::rpc::SendHandler& handler) {
        return true;
    }

};

/**
 * @brief A concrete outbound yajr error response message.
 */
class OutboundError : public OutboundResponse {

  public:
    /**
     * @brief Constructor for an outbound error message.
     */
    explicit OutboundError(
        yajr::Peer const & peer,            /**< [in] where to send to */
        PayloadGenerator const & error,   /**< [in] the error value to return */
        rapidjson::Value const & id /**< [in] the desired id for this message */
        )
        : OutboundResponse(peer, error, id)
        {
        }

    /**
     * Tell if this response is an error (or a result)
     *
     * @return true, since this is an error
     */
    virtual bool isError() {
      return true;
    }

  protected:
    /**
     * @brief Get payload key
     *
     * @return "error"
     */
    virtual char const * getPayloadKey() const {
        return Message::kPayloadKey.error;
    }

};

/**
 * @brief A concrete outbound yajr result response message.
 */
class OutboundResult : public OutboundResponse {

  public:
    /**
     * @brief Constructor for an outbound result message.
     */
    explicit OutboundResult(
        yajr::Peer const & peer,            /**< [in] where to send to */
        PayloadGenerator const & result, /**< [in] the result value to return */
        rapidjson::Value const & id /**< [in] the desired id for this message */
        )
        : OutboundResponse(peer, result, id)
        {
        }

    /**
     * Tell if this response is an error (or a result)
     *
     * @return false, since this is a result
     */
    virtual bool isError() {
      return false;
    }

    /**
     * @brief Get payload key
     *
     * @return "result"
     */
    virtual char const * getPayloadKey() const {
        return Message::kPayloadKey.result;
    }

};

class InboundResult;
class InboundError;

bool operator< (rapidjson::Value const & l, rapidjson::Value const & r);

/**
 * @brief A generic outbound yajr request message.
 */
class OutboundRequest : public OutboundMessage,
                        public LocalIdentifier {

  protected:

    /**
     * @brief Constructor for an outbound request message, needed by derived
     * classes
     *
     * Construct a new yajr outbound message. Never invoke directly, but only
     * from derived classes' constructors.
     */
    explicit OutboundRequest(
        yajr::Peer const & peer,            /**< [in] where to send to */
        PayloadGenerator const & params,   /**< [in] the params value to send */
        MethodName const * methodName,
        uint64_t id
        )
        :
            OutboundMessage(peer, params),
            LocalIdentifier(methodName, id)
        {
        }

    /**
     * @brief Get payload key
     *
     * @return "request"
     */
    virtual char const * getPayloadKey() const {
        return Message::kPayloadKey.params;
    }

    /**
     * Tell if this is a request
     *
     * @return true, since this is a request
     */
    virtual bool isRequest() const {
        return true;
    }

    virtual bool emitMethod(yajr::rpc::SendHandler& handler) {

        return handler.String("method")
            && handler.String(requestMethod());

    }

};

/**
 * @brief A generic inbound yajr response message.
 *
 * A generic inbound yajr response message is any message that represents a
 * response from a remote peer that is destined to the local peer, whether an
 * error or a result.
 */
class InboundResponse : public InboundMessage,
                        public LocalIdentifier {

  public:
    /**
     * Tell if this response is an error (or a result)
     *
     * @return true on error, false on result
     */
    virtual bool isError() = 0;

  protected:

    /**
     * @brief Constructor for an inbound response message, needed by derived
     * classes
     *
     * Construct a new yajr inbound response message. Never invoke directly,
     * but only from derived classes' constructors.
     */
    InboundResponse(
        yajr::Peer const & peer,      /**< [in] where we received from */
        rapidjson::Value const & response,/**< [in] the error/result received */
        rapidjson::Value const & id          /**< [in] the id of this message */
        );
};

/**
 * @brief A concrete inbound yajr error response message.
 */
class InboundError : public InboundResponse {

  public:
    /**
     * @brief Constructor for an inbound error response message
     */
    InboundError(
            yajr::Peer const & peer,  /**< [in] where we received from */
            rapidjson::Value const & error,      /**< [in] the error received */
            rapidjson::Value const & id      /**< [in] the id of this message */
            )
        : InboundResponse(peer, error, id)
        {}

    /**
     * Tell if this response is an error (or a result)
     *
     * @return true, since this is an error
     */
    bool isError() {
      return true;
    }

};

/**
 * @brief A concrete inbound yajr result response message.
 */
class InboundResult : public InboundResponse {

  public:
    /**
     * @brief Constructor for an inbound result response message
     */
    InboundResult(
            yajr::Peer const & peer,  /**< [in] where we received from */
            rapidjson::Value const & result,    /**< [in] the result received */
            rapidjson::Value const & id      /**< [in] the id of this message */
            )
        : InboundResponse(peer, result, id)
        {}

    /**
     * Tell if this response is an error (or a result)
     *
     * @return false, since this is a result
     */
    bool isError() {
      return false;
    }

};

/**
 * @brief A concrete inbound yajr request message.
 */
class InboundRequest : public InboundMessage, public RemoteIdentifier {

  public:
    /**
     * @brief Constructor for an inbound request message
     */
    InboundRequest(
            yajr::Peer const & peer,  /**< [in] where we received from */
            rapidjson::Value const & params,/**< [in] the params value to send */
            rapidjson::Value const & id      /**< [in] the id of this message */
            )
        :
            InboundMessage(peer, params),
            RemoteIdentifier(id)
        {}

};

std::size_t hash_value(rapidjson::Value const& v);
std::size_t hash_value(yajr::rpc::LocalId const & id);
std::size_t hash_value(yajr::rpc::LocalIdentifier const & id);
std::size_t hash_value(yajr::rpc::RemoteIdentifier const & id);

}}

#include <yajr/rpc/message_factory.hpp>

#endif/*_____COMMS__INCLUDE__OPFLEX__RPC_HPP*/
