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
#include <rapidjson/document.h>
#include <opflex/rpc/message_factory.hpp>
#include <opflex/rpc/send_handler.hpp>
#include <iovec-utils.hh>
#include <boost/intrusive/list.hpp>

#ifndef NDEBUG
#  include <boost/version.hpp>
#  if BOOST_VERSION > 105300
#    define COMMS_DEBUG_OBJECT_COUNT
#    include <boost/atomic.hpp>
#  endif
#endif

#include <iostream>

namespace opflex { namespace comms { namespace internal {

using namespace opflex::comms;

/* we pick the storage class specifier here, and omit it at the definitions */
void alloc_cb(uv_handle_t * _, size_t size, uv_buf_t* buf);
void on_close(uv_handle_t * h);
void on_write(uv_write_t *req, int status);
void on_read(uv_stream_t * h, ssize_t nread, uv_buf_t const * buf);
void on_passive_connection(uv_stream_t * server_handle, int status);
int connect_to_next_address(ActivePeer * peer, bool swap_stack = true);
void on_active_connection(uv_connect_t *req, int status);
void on_resolved(uv_getaddrinfo_t * req, int status, struct addrinfo *resp);
int addr_from_ip_and_port(const char * ip_address, uint16_t port,
        struct sockaddr_storage * addr);



class Peer : public ::boost::intrusive::list_base_hook<
             ::boost::intrusive::link_mode< ::boost::intrusive::auto_unlink> > {
#ifdef COMMS_DEBUG_OBJECT_COUNT
    static ::boost::atomic<size_t> counter;
#endif
  public:

    typedef ::boost::intrusive::list<Peer,
            ::boost::intrusive::constant_time_size<false> > List;

    class LoopData {
#ifdef COMMS_DEBUG_OBJECT_COUNT
        static ::boost::atomic<size_t> counter;
#endif
      public:

        typedef enum {
          ONLINE,
          LISTENING,
          RETRY_TO_CONNECT,
          RETRY_TO_LISTEN,
          ATTEMPTING_TO_CONNECT,
          PENDING_DELETE,

          /* don't touch past here */
          TOTAL_STATES
        } PeerState;

#ifdef COMMS_DEBUG_OBJECT_COUNT
        explicit LoopData() {
            ++counter;
        }

        ~LoopData() {
            --counter;
        }

        static size_t getCounter() {
            return counter;
        }
#endif

        Peer::List peers[LoopData::TOTAL_STATES];

        static Peer::LoopData * getLoopData(uv_loop_t * uv_loop) {
            return static_cast<Peer::LoopData *>(uv_loop->data);
        }

        static Peer::List * getPeerList(
                uv_loop_t * uv_loop,
                Peer::LoopData::PeerState peerState) {
            return &getLoopData(uv_loop)->peers[peerState];
        }
    };

    template <typename T, typename U>
    static T * get(U * h) {
        T * peer = static_cast<T *>(h->data);

        peer->__checkInvariants();

        return peer;
    }

    static CommunicationPeer * get(uv_write_t * r);

    static CommunicationPeer * get(uv_timer_t * h);

    static ActivePeer * get(uv_connect_t * r);

    static ActivePeer * get(uv_getaddrinfo_t * r);

#ifdef COMMS_DEBUG_OBJECT_COUNT
    static size_t getCounter() {
        return counter;
    }
#endif

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
              uv_loop_selector_(uv_loop_selector ? : &uv_default_loop),
              uvRefCnt_(1),
              connected_(0),
              destroying_(0),
              passive_(passive),
              pending_(0),
              ___________(0),
              status_(status)
            {
                handle_.data = this;
                handle_.loop = uv_loop_selector_();
#ifdef COMMS_DEBUG_OBJECT_COUNT
                ++counter;
#endif
            }

#ifndef NDEBUG
    virtual char const * peerType() const {return "?";} //= 0;
#endif

    void up() {

        LOG(DEBUG) << this
            << " refcnt: " << uvRefCnt_ << " -> " << uvRefCnt_ + 1;

        ++uvRefCnt_;
    }

    void down() {

        LOG(DEBUG) << this 
            << " refcnt: " << uvRefCnt_ << " -> " << uvRefCnt_ - 1;

        if (--uvRefCnt_) {
            return;
        }

        LOG(DEBUG) << "deleting " << this;

        delete this;
    }

    virtual void destroy() = 0;

#ifndef NDEBUG
    virtual void __checkInvariants() const {
    }
#else
    static inline void __checkInvariants() const {}
#endif

    /**
     * Get the uv_loop_t * for this peer
     *
     * @return the uv_loop_t * for this peer
     */
    uv_loop_t * getUvLoop() const {
        return handle_.loop;
    }

    void insert(Peer::LoopData::PeerState peerState) {
        Peer::LoopData::getPeerList(getUvLoop(), peerState)->push_back(*this);
    }

    uv_tcp_t handle_;
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
    uv_loop_selector_fn uv_loop_selector_;
    unsigned int  uvRefCnt_;
    unsigned char connected_  :1;
    unsigned char destroying_ :1;
    unsigned char passive_    :1;
          mutable
    unsigned char pending_    :1;
    unsigned char ___________ :1;
    unsigned char status_     :3;

  protected:
    /* don't leak memory! */
    virtual ~Peer() {
#ifdef COMMS_DEBUG_OBJECT_COUNT
        --counter;
#endif
    }
    friend std::ostream& operator<< (std::ostream&, Peer const *);
};

class CommunicationPeer : public Peer {

#ifdef COMMS_DEBUG_OBJECT_COUNT
    static ::boost::atomic<size_t> counter;
#endif
  public:

    explicit CommunicationPeer(bool passive,
        ConnectionHandler & connectionHandler,
        uv_loop_selector_fn uv_loop_selector = NULL,
        Peer::PeerStatus status = kPS_UNINITIALIZED)
            :
                Peer(passive, uv_loop_selector, status),
                connectionHandler_(connectionHandler),
                writer_(s_),
                nextId_(0),
                keepAliveInterval_(0),
                lastHeard_(0)
            {
                req_.data = this;
#ifdef COMMS_DEBUG_OBJECT_COUNT
                ++counter;
#endif
            }

#ifdef COMMS_DEBUG_OBJECT_COUNT
    static size_t getCounter() {
        return counter;
    }
#endif

#ifndef NDEBUG
    virtual void __checkInvariants() const {
        if (!!keepAliveInterval_ != !!uv_is_active((uv_handle_t *)&keepAliveTimer_)) {
            LOG(DEBUG) << this
                << " keepAliveInterval_ = " << keepAliveInterval_
                << " keepAliveTimer_ = " << (
                uv_is_active((uv_handle_t *)&keepAliveTimer_) ? "" : "in")
                << "active";
            ;
            assert(!!keepAliveInterval_ == !!uv_is_active((uv_handle_t *)&keepAliveTimer_));
        }
        Peer::__checkInvariants();
    }
#endif

    size_t readChunk(char const * buffer) {
        ssize_t chunk_size = - ssIn_.tellp();
        ssIn_ << buffer;
        chunk_size += ssIn_.tellp();
        return chunk_size;
    }

    opflex::rpc::InboundMessage * parseFrame();

    void onWrite() {
        pending_ = 0;

        s_.deque_.erase(first_, last_);

        write(); /* kick the can */
    }

    bool delimitFrame() const {
        s_.Put('\0');

        return true;
    }

    int write() const {

        if (pending_) {
            return 0;
        }

        pending_ = 1;
        first_ = s_.deque_.begin();
        last_ = s_.deque_.end();

        std::vector<iovec> iov = more::get_iovec(first_, last_);

        std::vector<iovec>::size_type size = iov.size();

        if (!size) {
            pending_ = 0;
            return 0;
        }

        /* FIXME: handle errors!!! */
        return uv_write(&write_req,
                (uv_stream_t*) &handle_,
                (uv_buf_t*)&iov[0],
                size,
                on_write);

    }

    void startKeepAlive(
            uint64_t interval = 2500,
            uint64_t begin = 100,
            uint64_t repeat = 1250) {

        LOG(DEBUG) << this
            << " interval=" << interval
            << " begin=" << begin
            << " repeat=" << repeat
        ;

        sendEchoReq();
        bumpLastHeard();

        keepAliveInterval_ = interval;
        uv_timer_start(&keepAliveTimer_, on_timeout, begin, repeat);
    }

    void stopKeepAlive() {
        LOG(DEBUG) << this;
     // assert(keepAliveInterval_ && uv_is_active((uv_handle_t *)&keepAliveTimer_));

        uv_timer_stop(&keepAliveTimer_);
        keepAliveInterval_ = 0;
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
        LOG(DEBUG) << this << " " << lastHeard_ << " -> " << now();
        lastHeard_ = now();
    }

    uint64_t getLastHeard() const {
        return lastHeard_;
    }

    uint64_t now() const {
        return uv_now(getUvLoop());
    }

    void onConnect() {
        connected_ = 1;
        keepAliveTimer_.data = this;
        LOG(DEBUG) << this << " up() for a timer init";
        up();
        uv_timer_init(getUvLoop(), &keepAliveTimer_);
        uv_unref((uv_handle_t*) &keepAliveTimer_);

        connectionHandler_(this);
    }

    void onDisconnect() {
        LOG(DEBUG);
        connected_ = 0;

        if (getKeepAliveInterval()) {
            stopKeepAlive();
        }

        LOG(DEBUG) << this << " issuing close for keepAliveTimer and tcp handle";
        uv_close((uv_handle_t*)&keepAliveTimer_, on_close);
        uv_close((uv_handle_t*)&handle_, on_close);

        unlink();

        if (destroying_) {
            return;
        }

        if (!passive_) {
            LOG(DEBUG) << this << " active => retry queue";
            /* we should attempt to reconnect later */
            insert(internal::Peer::LoopData::RETRY_TO_CONNECT);
        } else {
            LOG(DEBUG) << this << " passive => eventually drop";
            /* whoever it was, hopefully will reconnect again */
            insert(internal::Peer::LoopData::PENDING_DELETE);
        }
    }

    virtual void destroy() {
        LOG(DEBUG) << this;
     // Peer::destroy();
        destroying_ = 1;
        if (connected_) {
            onDisconnect();
        }
    }

    uint64_t getKeepAliveInterval() const {
        return keepAliveInterval_;
    }

    union {
        mutable uv_write_t write_req;
        uv_connect_t connect_req;
        uv_getaddrinfo_t dns_req;
        uv_req_t req_;
    };

    ::opflex::rpc::SendHandler & getWriter() const {
        writer_.Reset(s_);
        return writer_;
    }

    int tcpInit() {

        int rc;

        if ((rc = uv_tcp_init(getUvLoop(), &handle_))) {
            LOG(WARNING) << "uv_tcp_init: [" << uv_err_name(rc) << "] " <<
                uv_strerror(rc);
            return rc;
        }

     // LOG(DEBUG) << "{" << this << "}AP up() for a tcp init";
     // up();

        if ((rc = uv_tcp_keepalive(&handle_, 1, 60))) {
            LOG(WARNING) << "uv_tcp_keepalive: [" << uv_err_name(rc) << "] " <<
                uv_strerror(rc);
        }

        if ((rc = uv_tcp_nodelay(&handle_, 1))) {
            LOG(WARNING) << "uv_tcp_nodelay: [" << uv_err_name(rc) << "] " <<
                uv_strerror(rc);
        }

        return 0;
    }

  protected:
    /* don't leak memory! */
    virtual ~CommunicationPeer() {
#ifdef COMMS_DEBUG_OBJECT_COUNT
        --counter;
#endif
    }

  private:
    ConnectionHandler connectionHandler_;

    rapidjson::Document docIn_;
    std::stringstream ssIn_;

    mutable ::opflex::rpc::internal::StringQueue s_;
    mutable ::opflex::rpc::SendHandler writer_;
    mutable std::deque<rapidjson::UTF8<>::Ch>::iterator first_;
    mutable std::deque<rapidjson::UTF8<>::Ch>::iterator last_;
    mutable uint64_t nextId_;

    uint64_t keepAliveInterval_;
    uint64_t lastHeard_;

};

class ActivePeer : public CommunicationPeer {
#ifdef COMMS_DEBUG_OBJECT_COUNT
    static ::boost::atomic<size_t> counter;
#endif
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
        {
#ifdef COMMS_DEBUG_OBJECT_COUNT
            ++counter;
#endif
        }

#ifdef COMMS_DEBUG_OBJECT_COUNT
    static size_t getCounter() {
        return counter;
    }
#endif

#ifndef NDEBUG
    virtual char const * peerType() const {
        return "A";
    }
#endif

    void reset(uv_loop_selector_fn uv_loop_selector = NULL) {
        unlink();
        if (!uv_loop_selector_) {
            uv_loop_selector_ = uv_loop_selector ? : &uv_default_loop;
        }
        this->passive_ = 0;
        this->status_  = Peer::kPS_RESOLVING;
    }

    virtual void destroy() {
        CommunicationPeer::destroy();
        down();
    }

#ifndef NDEBUG
    virtual void __checkInvariants() const {
        CommunicationPeer::__checkInvariants();
    }
#endif

  protected:
    /* don't leak memory! */
    virtual ~ActivePeer() {
#ifdef COMMS_DEBUG_OBJECT_COUNT
        --counter;
#endif
    }
};

class PassivePeer : public CommunicationPeer {
#ifdef COMMS_DEBUG_OBJECT_COUNT
    static ::boost::atomic<size_t> counter;
#endif
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
        {
#ifdef COMMS_DEBUG_OBJECT_COUNT
            ++counter;
#endif
        }

#ifdef COMMS_DEBUG_OBJECT_COUNT
    static size_t getCounter() {
        return counter;
    }
#endif

#ifndef NDEBUG
    virtual char const * peerType() const {
        return "P";
    }
#endif

#ifdef PASSIVE_PEER_COULD_BE_RESET
    void reset(uv_loop_selector_fn uv_loop_selector = NULL) {
        unlink();
        if (!this->uv_loop_selector) {
            this->uv_loop_selector = uv_loop_selector ? : &uv_default_loop;
        }
        this->passive_= 1;
        this->status_ = Peer::kPS_RESOLVING;
    }
#endif

#ifndef NDEBUG
    virtual void __checkInvariants() const {
        CommunicationPeer::__checkInvariants();
    }
#endif

  protected:
    /* don't leak memory! */
    virtual ~PassivePeer() {
#ifdef COMMS_DEBUG_OBJECT_COUNT
        --counter;
#endif
    }
};

class ListeningPeer : public Peer {
#ifdef COMMS_DEBUG_OBJECT_COUNT
    static ::boost::atomic<size_t> counter;
#endif
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
#ifdef COMMS_DEBUG_OBJECT_COUNT
            ++counter;
#endif
        }

#ifdef COMMS_DEBUG_OBJECT_COUNT
    static size_t getCounter() {
        return counter;
    }
#endif

#ifndef NDEBUG
    virtual char const * peerType() const {
        return "L";
    }
#endif

    void reset(uv_loop_t * listener_uv_loop = NULL,
            uv_loop_selector_fn uv_loop_selector = NULL) {
        unlink();
        if (!this->_.listener.uv_loop) {
            this->_.listener.uv_loop = listener_uv_loop ? : uv_default_loop();
        }
        if (!uv_loop_selector_) {
            uv_loop_selector_ = uv_loop_selector ? : &uv_default_loop;
        }
    }

    virtual void destroy() {
        LOG(DEBUG) << this;
     // Peer::destroy();
        destroying_ = 1;
        down();
        if (connected_) {
            connected_ = 0;
            uv_close((uv_handle_t*)&handle_, on_close);
        }
    }

#ifndef NDEBUG
    virtual void __checkInvariants() const {
        Peer::__checkInvariants();
    }
#endif

    struct sockaddr_storage listen_on;

    ConnectionHandler connectionHandler_;
};

}}}

#endif /* _INCLUDE__OPFLEX__COMMS_INTERNAL_HPP */
