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

#include <opflex/yajr/rpc/send_handler.hpp>
#include <opflex/yajr/yajr.hpp>

#include <rapidjson/document.h>
#include <rapidjson/writer.h>

#include <uv.h>

#include <boost/functional/hash.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/function.hpp>

#include <deque>

namespace yajr {

namespace rpc {

typedef rapidjson::Value::StringRefType const MethodName;
typedef rapidjson::Writer< yajr::internal::StringQueue > SendHandler;
typedef boost::function<bool (yajr::rpc::SendHandler &)> PayloadGenerator;

/**
 * Generate JSON from value
 */
class GeneratorFromValue {

  public:
    /**
     * Generate JSON from value
     * @param v rapidjson value
     */
    GeneratorFromValue(rapidjson::Value const & v) : v_(v) {}

    /** () operator */
    bool operator()(yajr::rpc::SendHandler & h) {
        return v_.Accept(h);
    }

  private:
    rapidjson::Value const & v_;

};

/**
 * Identifier
 */
class Identifier {
  public:
    /**
     * Emit the identifier
     * @param h handler
     * @return valid ID
     */
    virtual bool emitId(yajr::rpc::SendHandler & h) const = 0;
  protected:
    /**
     * Name of the request method
     * @return name of the request method
     */
    virtual char const * requestMethod() const = 0;
};

/**
 * Loacl ID
 */
struct LocalId {
    /**
     * Construct a LocalId
     * @param methodName method name
     * @param id id
     */
    LocalId(MethodName const * methodName, uint64_t id)
        :
            methodName_(methodName),
            id_(id)
        {}
    /** method name */
    MethodName const * methodName_;
    /** id */
    uint64_t id_;
};

/**
 * Local identifier
 */
class LocalIdentifier : virtual public Identifier, private LocalId {
  public:
    /**
     * Construct a local identifier
     * @param methodName method naem
     * @param id id
     */
    LocalIdentifier(MethodName const * methodName, uint64_t id)
        :
            LocalId(methodName, id)
        {}

    /**
     * Emit the identifier
     * @param h handler
     * @return valid ID
     */
    bool emitId(yajr::rpc::SendHandler & h) const;

    /**
     * Get the local ID
     * @return local ID
     */
    LocalId const getLocalId() const {
        return LocalId(methodName_, id_);
    }

  protected:
    /**
     * Name of the request method
     * @return name of the request method
     */
    char const * requestMethod() const {
        return methodName_->s;
    }
};

/**
 * Remote Identifier
 */
class RemoteIdentifier : virtual public Identifier {
  public:
    /**
     * Construct a remote identitier
     * @param id JSON ID
     */
    RemoteIdentifier(rapidjson::Value const & id) : id_(id) {}

    /**
     * Emit the identifier
     * @param h handler
     * @return valid ID
     */
    bool emitId(yajr::rpc::SendHandler & h) const;

    /**
     * Get the remote ID
     * @return remote ID
     */
    rapidjson::Value const & getRemoteId() const {
        return id_;
    }
  protected:
    /**
     * Name of the request method
     * @return name of the request method
     */
    char const * requestMethod() const {
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
    yajr::Peer* getPeer() const {
        return peer_;
    }

    virtual ~Message() {}

    /** Payload keys */
    typedef struct {
        /** params */
        char const * const params;
        /** result */
        char const * const result;
        /** error */
        char const * const error;
    } PayloadKeys;

    /** Payload keys */
    static PayloadKeys kPayloadKey;

  protected:

    /**
     * @brief Constructor needed by derived classes
     *
     * Construct a new yajr message. Never invoke directly, but only
     * from derived classes' constructors.
     */
    explicit Message(
            yajr::Peer* peer
            )
        : peer_(peer)
        {}

    /**
     * Get the uv_loop_t * for this message
     *
     * @return the uv_loop_t * for this message
     */
    uv_loop_t const * getUvLoop() const;

    /** pointer to the peer so that it can be set multiple times */
    yajr::Peer* peer_;

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

    /**
     * When was the message received
     * @return timestamp msg was received
     */
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
     *
     * @param peer Peer
     * @param payload payload
     */
    explicit InboundMessage(
            yajr::Peer& peer,               /* < [in] where to send to */
            rapidjson::Value const & payload
            )
        :
            Message(&peer),
            payload_(payload),
            received_(uv_now(getUvLoop()))
        {
            assert(&peer);
        }

  private:
    /* making the peer private so InboundMessage subclasses won't see it */
    using Message::peer_;
    rapidjson::Value const & payload_;/**< [in] params, result, error payload */
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
     * @brief Emit events to a handler
     *
     * Emit events to a handler that will serialize it to json.
     *
     * @return true on success, false on failure.
     */
    bool Accept(yajr::rpc::SendHandler& handler);

    /**
     * Send this message now!
     */
    bool send();

  protected:

    /**
     * @brief Constructor needed by derived classes
     *
     * Construct a new outbound yajr message. Never invoke directly, but only
     * from derived classes' constructors.
     */
    explicit OutboundMessage(
        PayloadGenerator const & payloadGenerator,/**< [in] payload generator */
        yajr::Peer* peer                    /**< [in] where to send to */
        )
        :
          Message(peer),
          payloadGenerator_(payloadGenerator),
          sent_(uv_now(getUvLoop()))
        {}

    /**
     * @brief Get payload key
     *
     * @return one of "params", "result" or "error"
     */
    virtual char const * getPayloadKey() const = 0;

    /**
     * Method name
     * @param handler send handler
     * @return success or not
     */
    virtual bool emitMethod(yajr::rpc::SendHandler& handler) = 0;
  private:
    PayloadGenerator const payloadGenerator_;
    uint64_t sent_;
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
            yajr::Peer& peer,  /**< [in] where we received from */
            rapidjson::Value const & params,/**< [in] the params value to send */
            rapidjson::Value const & id      /**< [in] the id of this message */
            )
        :
            InboundMessage(peer, params),
            RemoteIdentifier(id)
        {}

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
     * Send this message now!
     */
    using OutboundMessage::send;

  protected:
    /**
     * @brief Constructor needed by derived classes
     *
     * Construct a new outbound yajr response message. Never invoke directly,
     * but only * from derived classes' constructors.
     */
    explicit OutboundResponse(
        InboundRequest const * inbReq,          /**< [in] request we reply to */
        PayloadGenerator const & response  /**< [in] the error/result to send */
        )
        :
            OutboundMessage(response, inbReq->getPeer()),
            RemoteIdentifier(inbReq->getRemoteId())
        {
        }

    /**
     * @brief Constructor needed by derived classes
     *
     * Construct a new outbound yajr response message. Never invoke directly,
     * but only * from derived classes' constructors.
     */
    explicit OutboundResponse(
        yajr::Peer& peer,            /**< [in] where to send to */
        PayloadGenerator const & response, /**< [in] the error/result to send */
        rapidjson::Value const & id /**< [in] the desired id for this message */
        )
        :
            OutboundMessage(response, &peer),
            RemoteIdentifier(id)
        {
        }

    /**
     * Method name
     * @param handler send handler
     * @return success or not
     */
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
        InboundRequest const * inbReq,          /**< [in] request we reply to */
        PayloadGenerator const & error    /**< [in] the error value to return */
        )
        : OutboundResponse(inbReq, error)
        {
        }

    /**
     * @brief Constructor for an outbound error message.
     */
    explicit OutboundError(
        yajr::Peer& peer,                   /**< [in] where to send to */
        PayloadGenerator const & error,   /**< [in] the error value to return */
        rapidjson::Value const & id /**< [in] the desired id for this message */
        )
        : OutboundResponse(peer, error, id)
        {
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
     *
     * @param inbReq Request we're replying to
     * @param result Result of request
     */
    explicit OutboundResult(
        InboundRequest const * inbReq,          /**< [in] request we reply to */
        PayloadGenerator const & result  /**< [in] the result value to return */
        )
        : OutboundResponse(inbReq, result)
        {
        }

    /**
     * @brief Constructor for an outbound result message.
     *
     * @param peer peer
     * @param result result
     * @param id identifier
     */
    explicit OutboundResult(
        yajr::Peer& peer,            /**< [in] where to send to */
        PayloadGenerator const & result, /**< [in] the result value to return */
        rapidjson::Value const & id /**< [in] the desired id for this message */
        )
        : OutboundResponse(peer, result, id)
        {
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

  public:

    /**
     * @brief Constructor for an outbound request message, needed by derived
     * classes
     *
     * Construct a new yajr outbound message. Never invoke directly, but only
     * from derived classes' constructors.
     *
     * @param params params
     * @param methodName method name
     * @param id identifier
     * @param peer peer
     */
    explicit OutboundRequest(
        PayloadGenerator const & params,   /* < [in] the params value to send */
        MethodName const * methodName,
        uint64_t id, /* FIXME */
        yajr::Peer* peer                    /* < [in] where to send to */
        )
        :
            OutboundMessage(params, peer),
            LocalIdentifier(methodName, id)
        {
        }

    /**
     * Send this message now!
     */
    using OutboundMessage::send;

  protected:
    /**
     * @brief Get payload key
     *
     * @return "request"
     */
    virtual char const * getPayloadKey() const {
        return Message::kPayloadKey.params;
    }

    /**
     * Method name
     * @param handler send handler
     * @return success or not
     */
    virtual bool emitMethod(yajr::rpc::SendHandler& handler) {

        return handler.String("method")
            && handler.String(requestMethod());

    }

  private:
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
        yajr::Peer* peer                     /**< [in] the peer to set */
        ) {
        peer_ = peer;
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
     *
     * @param peer peer
     * @param response response
     * @param id identifier
     */
    InboundResponse(
        yajr::Peer& peer,      /**< [in] where we received from */
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
     *
     * @param peer peer
     * @param error error
     * @param id identifier
     */
    InboundError(
            yajr::Peer& peer,  /**< [in] where we received from */
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
     *
     * @param peer peer
     * @param result result
     * @param id identifier
     */
    InboundResult(
            yajr::Peer& peer,  /**< [in] where we received from */
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

std::size_t hash_value(rapidjson::Value const& v);
std::size_t hash_value(yajr::rpc::LocalId const & id);
std::size_t hash_value(yajr::rpc::LocalIdentifier const & id);
std::size_t hash_value(yajr::rpc::RemoteIdentifier const & id);

} /* yajr::rpc namespace */
} /* yajr namespace */

#endif/*_____COMMS__INCLUDE__OPFLEX__RPC_HPP*/

