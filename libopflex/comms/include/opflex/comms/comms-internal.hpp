/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef _INCLUDE__OPFLEX__COMMS_INTERNAL_HPP
#define _INCLUDE__OPFLEX__COMMS_INTERNAL_HPP

#include <cstdlib>  /* for malloc() and free() */
#include <sstream>  /* for basic_stringstream<> */
#include <opflex/comms/comms.hpp>
#include <opflex/comms/internal/json.hpp>
#include <adt/intrusive_dlist.h>
#include <rapidjson/document.h>
#include <tricks/container_of.h>
#include <opflex/rpc/message_factory.hpp>

#define COMMS_BUF_SZ 4096

namespace opflex { namespace comms { namespace internal {

using namespace opflex::comms;

/* we pick the storage class specifier here, and omit it at the definitions */
void alloc_cb(uv_handle_t * _, size_t size, uv_buf_t* buf);
void on_close(uv_handle_t * h);
void on_write(uv_write_t *req, int status);
void on_read(uv_stream_t * h, ssize_t nread, uv_buf_t const * buf);
void on_passive_connection(uv_stream_t * server_handle, int status);
int connect_to_next_address(ActivePeer * peer);
void on_active_connection(uv_connect_t *req, int status);
void on_resolved(uv_getaddrinfo_t * req, int status, struct addrinfo *resp);
int addr_from_ip_and_port(const char * ip_address, uint16_t port,
        struct sockaddr_storage * addr);



class Peer {
  public:

    template <typename T, typename U>
    static T * get(U * h) {
        return static_cast<T *>(h->data);
    }

    static CommunicationPeer * get(uv_write_t * r);

    static CommunicationPeer * get(uv_timer_t * h);

    static ActivePeer * get(uv_connect_t * r);

    static ActivePeer * get(uv_getaddrinfo_t * r);

    /* Ideally nested as Peer::PeerStatus, but this is only possible with C++11 */
    enum PeerStatus {
        kPS_ONLINE            = 0,  /* <--- don't touch these! */
        kPS_LISTENING         = 0,  /* <--- don't touch these! */
        kPS_RESOLVING,
        kPS_RESOLVED,
        kPS_CONNECTING,

        kPS_UNINITIALIZED,
        kPS_FAILED_TO_RESOLVE,
        kPS_FAILED_BINDING,
        kPS_FAILED_LISTENING,
    };
    explicit Peer(bool passive,
         uv_loop_selector_fn uv_loop_selector = NULL,
         Peer::PeerStatus status = kPS_UNINITIALIZED)
            :
              passive(passive),
              pending(0),
              uv_loop_selector(uv_loop_selector ? : &uv_default_loop),
              status(status),
              uvRefCnt_(1)
            {
#ifndef NDEBUG
                peer_hook.link[0] = &peer_hook;
                peer_hook.link[1] = &peer_hook;
#endif
                handle.data = this;
            }

    void up() {

        LOG(DEBUG) << this << " refcnt: " << uvRefCnt_ << " -> " << uvRefCnt_ + 1;

        ++uvRefCnt_;
    }

    void down() {

        LOG(DEBUG) << this << " refcnt: " << uvRefCnt_ << " -> " << uvRefCnt_ - 1;

        if (--uvRefCnt_) {
            return;
        }

        LOG(DEBUG) << "deleting " << this;

        delete this;
    }

    d_intr_hook_t peer_hook;
    uv_tcp_t handle;
    union {
        union {
            struct {
                struct addrinfo * ai;
                struct addrinfo const * ai_next;
            };
            struct {
                uv_loop_t * uv_loop;
            } listener;
        } _;
        uv_timer_t keepAliveTimer_;
    };
    uv_loop_selector_fn uv_loop_selector;
    unsigned char passive   :1;
          mutable
    unsigned char pending   :1;
    unsigned char status    :5;
    unsigned int  uvRefCnt_;

  protected:
    /* don't leak memory! */
    virtual ~Peer() {}
};

class CommunicationPeer : public Peer {

  public:

    explicit CommunicationPeer(bool passive,
        ConnectionHandler & connectionHandler,
        uv_loop_selector_fn uv_loop_selector = NULL,
        Peer::PeerStatus status = kPS_UNINITIALIZED)
            :
                Peer(passive, uv_loop_selector, status),
                connectionHandler_(connectionHandler),
                nextId_(0),
                lastHeard_(0)
            {
                req_.data = this;
            }

    size_t readChunk(char const * buffer) {
        ssize_t chunk_size = - ssIn_.tellp();
        ssIn_ << buffer;
        chunk_size += ssIn_.tellp();
        return chunk_size;
    }

    opflex::rpc::InboundMessage * parseFrame();

    int write() const {

        uv_buf_t buf = {
            /* .base = */ buffer,
            /* .len  = */ ssOut_.readsome(buffer, sizeof(buffer))
        };

        if (!buf.len) {
            pending = 0;
            return 0;
        }

        /* FIXME: handle errors!!! */
        return uv_write(&write_req,
                (uv_stream_t*) &handle,
                &buf,
                1,
                on_write);

    }

    int writeMsg(char const * msg) const {

        LOG(DEBUG)
            << "peer = " << this
            << " Queued up for sending: \"" << msg << "\""
        ;

        ssOut_ << msg << '\0';

        /* for now, we have at most one outstanding write request per peer */
        if (pending) {
            return 0;
        }

        pending = 1;
        /* FIXME: handle errors!!! */
        return write();

    }

    /**
     * Get the uv_loop_t * for this peer
     *
     * @return the uv_loop_t * for this peer
     */
    uv_loop_t * getUvLoop() const {
        return handle.loop;
    }

    void startKeepAlive(
            uint64_t interval = 1000,
            uint64_t begin = 250,
            uint64_t repeat = 500) {

        LOG(DEBUG) << this
            << " interval=" << interval
            << " begin=" << begin
            << " repeat=" << repeat
        ;

        sendEchoReq();
        bumpLastHeard();

        keepaliveInterval_ = interval;
        uv_timer_start(&keepAliveTimer_, on_timeout, begin, repeat);
    }

    void stopKeepAlive() {
        LOG(DEBUG) << this;
        if (uv_is_active((uv_handle_t *)&keepAliveTimer_)) {
            LOG(DEBUG) << this;
            uv_timer_stop(&keepAliveTimer_);
        }
    }

    static void on_timeout(uv_timer_t * timer) {
        LOG(DEBUG);

        get(timer)->timeout();

    }

    void sendEchoReq();
    void timeout();

    /* avoid using as much as possible, and clear() greedily. if you don't know
     * why it is here, then you probably don't need it */
    rapidjson::MemoryPoolAllocator<> & getAllocator() const {

        return *static_cast<rapidjson::MemoryPoolAllocator<>*>(
                getUvLoop()->data
            );

    }

    /**
     * Get the id to use for the next message we send to this peer.
     *
     * The input string must outlive the message. You most likely
     * want to provide a pointer to a string literal for simplicity,
     * along with the sizeof that string literal decreased by one if
     * NULL-terminated.
     *
     * The purpose is to help process responses in a stateless manner.
     *
     * @return a rapidjson *array* value to be used as a jsonrpc ID
     */
    uint64_t nextId() const {
        return nextId_++;
    }

    void bumpLastHeard() {
        lastHeard_ = now();
    }

    uint64_t getLastHeard() const {
        return lastHeard_;
    }

    uint64_t now() const {
        return uv_now(getUvLoop());
    }

    void onConnect() {
        keepAliveTimer_.data = this;
        uv_timer_init(getUvLoop(), &keepAliveTimer_);
        uv_unref((uv_handle_t*) &keepAliveTimer_);

        connectionHandler_(this);
    }

    union {
        mutable uv_write_t write_req;
        uv_connect_t connect_req;
        uv_getaddrinfo_t dns_req;
        uv_req_t req_;
    };

  protected:
    /* don't leak memory! */
    virtual ~CommunicationPeer() {}

  private:
    ConnectionHandler connectionHandler_;

    mutable char buffer[COMMS_BUF_SZ];
    rapidjson::Document docIn_;

    std::stringstream ssIn_;
    mutable std::stringstream ssOut_;

    mutable uint64_t nextId_;
    uint64_t keepaliveInterval_;
    uint64_t lastHeard_;

};

class ActivePeer : public CommunicationPeer {
  public:
    explicit ActivePeer(
            ConnectionHandler connectionHandler,
            uv_loop_selector_fn uv_loop_selector = NULL)
        :
            CommunicationPeer(
                    false,
                    connectionHandler,
                    uv_loop_selector,
                    kPS_RESOLVING)
        {}

    void reset(uv_loop_selector_fn uv_loop_selector = NULL) {
        d_intr_list_unlink(&this->peer_hook);
        if (!this->uv_loop_selector) {
            this->uv_loop_selector = uv_loop_selector ? : &uv_default_loop;
        }
        this->passive = false;
        this->status  = Peer::kPS_RESOLVING;
    }

  protected:
    /* don't leak memory! */
    virtual ~ActivePeer() {}
};

class PassivePeer : public CommunicationPeer {
  public:
    explicit PassivePeer(
            ConnectionHandler connectionHandler,
            uv_loop_selector_fn uv_loop_selector = NULL)
        :
            CommunicationPeer(
                    true,
                    connectionHandler,
                    uv_loop_selector,
                    kPS_RESOLVING)
        {}

    void reset(uv_loop_selector_fn uv_loop_selector = NULL) {
        d_intr_list_unlink(&this->peer_hook);
        if (!this->uv_loop_selector) {
            this->uv_loop_selector = uv_loop_selector ? : &uv_default_loop;
        }
        this->passive = true;
        this->status  = Peer::kPS_RESOLVING;
    }

  protected:
    /* don't leak memory! */
    virtual ~PassivePeer() {}
};

class ListeningPeer : public Peer {
  public:
    explicit ListeningPeer(
            ConnectionHandler & connectionHandler,
            uv_loop_t * listener_uv_loop = NULL,
            uv_loop_selector_fn uv_loop_selector = NULL)
        :
            connectionHandler_(connectionHandler),
            Peer(false, uv_loop_selector, kPS_UNINITIALIZED)
        {
            _.listener.uv_loop = listener_uv_loop ? : uv_default_loop();
        }
    void reset(uv_loop_t * listener_uv_loop = NULL,
            uv_loop_selector_fn uv_loop_selector = NULL) {
        d_intr_list_unlink(&this->peer_hook);
        if (!this->_.listener.uv_loop) {
            this->_.listener.uv_loop = listener_uv_loop ? : uv_default_loop();
        }
        if (!this->uv_loop_selector) {
            this->uv_loop_selector = uv_loop_selector ? : &uv_default_loop;
        }
    }

    struct sockaddr_storage listen_on;

    ConnectionHandler connectionHandler_;
};

union peer_db_ {
    struct {
        d_intr_head_t online;
        d_intr_head_t retry;  /* periodically re-attempt to reconnect */
        d_intr_head_t attempting;
        d_intr_head_t listening;
        d_intr_head_t retry_listening;
    };
    d_intr_head_t __all_doubly_linked_lists[4];
};

}}}

#endif /* _INCLUDE__OPFLEX__COMMS_INTERNAL_HPP */
