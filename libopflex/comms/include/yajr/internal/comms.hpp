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

#include <yajr/rpc/send_handler.hpp>
#include <yajr/rpc/message_factory.hpp>
#include <yajr/yajr.hpp>
#include <yajr/rpc/rpc.hpp>

#include <rapidjson/document.h>
#include <iovec-utils.hh>
#include <uv.h>

#include <boost/intrusive/list.hpp>

#include <sstream>  /* for basic_stringstream<> */
#include <iostream>

#ifndef NDEBUG
#  include <boost/version.hpp>
#  if BOOST_VERSION > 105300
#    define COMMS_DEBUG_OBJECT_COUNT
#    include <boost/atomic.hpp>
#  endif
#endif

namespace yajr { namespace comms { namespace internal {

using namespace yajr::comms;
class ActivePeer;
class CommunicationPeer;

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



typedef ::boost::intrusive::list_base_hook<
    ::boost::intrusive::link_mode< ::boost::intrusive::auto_unlink> >
    SafeListBaseHook;

class Peer : public SafeListBaseHook {
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
          TO_RESOLVE,
          TO_LISTEN,
          RETRY_TO_CONNECT,
          RETRY_TO_LISTEN,
          ATTEMPTING_TO_CONNECT,
          PENDING_DELETE,

          /* don't touch past here */
          TOTAL_STATES
        } PeerState;

        struct CloseHandle {
            LoopData const * loopData;
            uv_close_cb      closeCb;
        };

        struct CountHandle {
            LoopData const * loopData;
            size_t           counter;
        };

        explicit LoopData(uv_loop_t * loop)
        :
            lastRun_(uv_now(loop)),
            destroying_(0),
            refCount_(1)
        {
#ifdef COMMS_DEBUG_OBJECT_COUNT
            ++counter;
#endif
            uv_prepare_init(loop, &prepare_);
            uv_prepare_start(&prepare_, &onPrepareLoop);
            prepare_.data = this;
            uv_timer_init(loop, &prepareAgain_);
            prepareAgain_.data = this;
        }

#ifdef COMMS_DEBUG_OBJECT_COUNT 
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

        void onPrepareLoop() __attribute__((no_instrument_function));

        void destroy();

        struct RetryPeer {
            void operator()(Peer *peer)
            {
                LOG(DEBUG) << peer << " retry";
                peer->retry();
            }
        };

        void up() {
            LOG(DEBUG) << this
                << " LoopRefCnt: " << refCount_ << " -> " << refCount_ + 1;

            ++refCount_;
        }

        void down() {
            LOG(DEBUG) << " Down() on Loop";
            LOG(DEBUG) << this
                << " LoopRefCnt: " << refCount_ << " -> " << refCount_ - 1;

            assert(refCount_);

            --refCount_;

            if (destroying_ && !refCount_) {
                LOG(INFO) << this << " walking uv_loop before stopping it";
                uv_walk(prepare_.loop, walkAndDumpHandlesCb< ERROR >, this);

                CloseHandle closeHandle = { this, NULL };

                uv_walk(prepare_.loop, walkAndCloseHandlesCb, &closeHandle);

            }
        }

        void ensureResolvePending();

        template < ::opflex::logging::OFLogHandler::Level LOGGING_LEVEL >
        static void walkAndDumpHandlesCb(uv_handle_t* handle, void* _) __attribute__((no_instrument_function));
        static void walkAndCloseHandlesCb(uv_handle_t* handle, void* closeHandles) __attribute__((no_instrument_function));
        static void walkAndCountHandlesCb(uv_handle_t* handle, void* countHandles) __attribute__((no_instrument_function));

      private:
        friend std::ostream& operator<< (std::ostream&, Peer::LoopData const *);

        ~LoopData() {
            LOG(DEBUG) << "Delete on Loop";
            LOG(DEBUG) << this << " is being deleted";
#ifdef COMMS_DEBUG_OBJECT_COUNT
            --counter;
#endif
        }

        struct PeerDisposer {
            void operator()(Peer *peer)
            {
                LOG(INFO) << peer << " destroy() because this communication thread is shutting down";
                peer->destroy();
            }
        };

        static void onPrepareLoop(uv_prepare_t *) __attribute__((no_instrument_function));
        static void fini(uv_handle_t *);

        uv_prepare_t prepare_;
        uv_timer_t prepareAgain_;
        uint64_t lastRun_;
        bool destroying_;
        uint64_t refCount_;
    };

    template <typename T, typename U>
    static T * get(U * h) {
        T * peer = static_cast<T *>(h->data);

        assert(peer->__checkInvariants());

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
         ::yajr::Peer::UvLoopSelector uvLoopSelector = NULL,
         Peer::PeerStatus status = kPS_UNINITIALIZED)
            :
              uvLoopSelector_(uvLoopSelector ? : &uvDefaultLoop),
              uvRefCnt_(1),
              connected_(0),
              destroying_(0),
              passive_(passive),
              ___________(0),
              status_(status)
            {
                handle_.data = this;
#ifdef COMMS_DEBUG_OBJECT_COUNT
                ++counter;
#endif
            }

#ifndef NDEBUG
    virtual char const * peerType() const {return "?";} //= 0;
#endif

    virtual void retry() = 0;

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

        onDelete();

        delete this;
    }

    virtual void onDelete() {}

    virtual void destroy() = 0;

#ifndef NDEBUG
    virtual
#else
    static
#endif
    bool __checkInvariants()
#ifndef NDEBUG
    const
#endif
    __attribute__((no_instrument_function));

    /**
     * Get the uv_loop_t * for this peer
     *
     * @return the uv_loop_t * for this peer
     */
    uv_loop_t * getUvLoop() const {
        return handle_.loop;
    }

    void insert(Peer::LoopData::PeerState peerState) {
        LOG(DEBUG) << this << " is being inserted in " << peerState;
        Peer::LoopData::getPeerList(getUvLoop(), peerState)->push_back(*this);
    }

    void unlink() {
        LOG(DEBUG) << this << " manually unlinking";

        SafeListBaseHook::unlink();

        return;
    }

    uv_tcp_t handle_;
    union {
        union {
            struct {
                struct addrinfo * ai;
                struct addrinfo const * ai_next;
            };
            struct {
                uv_loop_t * uvLoop_;
            } listener_;
        } _;
        uv_timer_t keepAliveTimer_;
    };
    ::yajr::Peer::UvLoopSelector uvLoopSelector_;
    unsigned int  uvRefCnt_;
    unsigned char connected_  :1;
    unsigned char destroying_ :1;
    unsigned char passive_    :1;
          mutable
    unsigned char ___________ :2;
    unsigned char status_     :3;

  protected:
    /* don't leak memory! */
    virtual ~Peer() {
        getLoopData()->down();
#ifdef COMMS_DEBUG_OBJECT_COUNT
        --counter;
#endif
    }
    static uv_loop_t * uvDefaultLoop(void *) {
        return uv_default_loop();
    }

    Peer::LoopData * getLoopData() const {
        return Peer::LoopData::getLoopData(getUvLoop());
    }
    friend std::ostream& operator<< (std::ostream&, Peer const *);
};

class CommunicationPeer : public Peer, virtual public ::yajr::Peer {

#ifdef COMMS_DEBUG_OBJECT_COUNT
    static ::boost::atomic<size_t> counter;
#endif
  public:

    explicit CommunicationPeer(bool passive,
        ::yajr::Peer::StateChangeCb connectionHandler,
        void * data,
        ::yajr::Peer::UvLoopSelector uvLoopSelector = NULL,
        internal::Peer::PeerStatus status = kPS_UNINITIALIZED)
            :
                internal::Peer(passive, uvLoopSelector, status),
                ::yajr::Peer(),
                connectionHandler_(connectionHandler),
                data_(data),
                writer_(s_),
                pendingBytes_(0),
                nextId_(0),
                keepAliveInterval_(0),
                lastHeard_(0)
            {
                req_.data = this;
                handle_.loop = uvLoopSelector_(getData());
                getLoopData()->up();
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
    virtual bool __checkInvariants() const __attribute__((no_instrument_function));
#endif
    virtual void retry() = 0;

    size_t readChunk(char const * buffer) {
        ssize_t chunk_size = - ssIn_.tellp();
        ssIn_ << buffer;
        chunk_size += ssIn_.tellp();
        return chunk_size;
    }

    yajr::rpc::InboundMessage * parseFrame();

    void onWrite();

    bool delimitFrame() const {
        s_.Put('\0');

        return true;
    }

    int write() const;

    void * getData() const {
        return data_;
    }

    virtual void onDelete() {
        connectionHandler_(dynamic_cast<yajr::Peer *>(this), data_, ::yajr::StateChange::DELETE, 0);
    }

    virtual void startKeepAlive(
            uint64_t begin    =  100,
            uint64_t repeat   = 1250,
            uint64_t interval = 2500) {

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

    virtual void stopKeepAlive() {
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

    void onError(int error) {
        connectionHandler_(this, data_, ::yajr::StateChange::FAILURE, error);
    }

    void onConnect() {
        connected_ = 1;
        keepAliveTimer_.data = this;
        LOG(DEBUG) << this << " up() for a timer init";
        up();
        uv_timer_init(getUvLoop(), &keepAliveTimer_);
        uv_unref((uv_handle_t*) &keepAliveTimer_);

        connectionHandler_(this, data_, ::yajr::StateChange::CONNECT, 0);
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

        /* FIXME: might be called too many times? */
        connectionHandler_(this, data_, ::yajr::StateChange::DISCONNECT, 0);

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

    virtual void disconnect() {
        if (connected_) {
            onDisconnect();
        }
    }

    virtual int getPeerName(struct sockaddr* remoteAddress, int* len) const {
        return uv_tcp_getpeername(&handle_, remoteAddress, len);
    }

    virtual void destroy() {
        LOG(DEBUG) << this;

        assert(!destroying_);
        if(destroying_) {
            return;
        }

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
        mutable uv_write_t write_req_;
        uv_connect_t connect_req_;
        uv_getaddrinfo_t dns_req_;
        uv_req_t req_;
    };

    ::yajr::rpc::SendHandler & getWriter() const {
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

    static void dumpIov(
            std::stringstream & dbgLog,
            std::vector<iovec> const & iov
    );

#ifdef    NEED_DESPERATE_CPU_BOGGING_AND_THREAD_UNSAFE_DEBUGGING
// Not thread-safe. Build only as needed for ad-hoc debugging builds.
    void logDeque() const;
#endif // NEED_DESPERATE_CPU_BOGGING_AND_THREAD_UNSAFE_DEBUGGING

  protected:
    /* don't leak memory! */
    virtual ~CommunicationPeer() {
#ifdef COMMS_DEBUG_OBJECT_COUNT
        --counter;
#endif
    }

  private:
    ::yajr::Peer::StateChangeCb connectionHandler_;
    void * data_;

    rapidjson::Document docIn_;
    std::stringstream ssIn_;

    mutable ::yajr::internal::StringQueue s_;
    mutable ::yajr::rpc::SendHandler writer_;
    mutable size_t pendingBytes_;
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
            std::string const & hostname,
            std::string const & service,
            ::yajr::Peer::StateChangeCb connectionHandler,
            void * data,
            ::yajr::Peer::UvLoopSelector uvLoopSelector = NULL)
        :
            CommunicationPeer(
                    false,
                    connectionHandler,
                    data,
                    uvLoopSelector,
                    kPS_RESOLVING),
            hostname_(hostname),
            service_(service)
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

    virtual void retry();

    virtual void destroy() {
        CommunicationPeer::destroy();
        down();
    }

#ifndef NDEBUG
    virtual bool __checkInvariants() const __attribute__((no_instrument_function));
#endif

    char const * getHostname() const {
        return hostname_.c_str();
    }

    char const * getService() const {
        return service_.c_str();
    }

  protected:
    /* don't leak memory! */
    virtual ~ActivePeer() {
#ifdef COMMS_DEBUG_OBJECT_COUNT
        --counter;
#endif
    }
    std::string const hostname_;
    std::string const service_;
};

class PassivePeer : public CommunicationPeer {
#ifdef COMMS_DEBUG_OBJECT_COUNT
    static ::boost::atomic<size_t> counter;
#endif
  public:
    explicit PassivePeer(
            ::yajr::Peer::StateChangeCb connectionHandler,
            void * data,
            ::yajr::Peer::UvLoopSelector uvLoopSelector = NULL)
        :
            CommunicationPeer(
                    true,
                    connectionHandler,
                    data,
                    uvLoopSelector,
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

    virtual void retry() {
        assert(0);
    }

#ifndef NDEBUG
    virtual bool __checkInvariants() const __attribute__((no_instrument_function));
#endif

  protected:
    /* don't leak memory! */
    virtual ~PassivePeer() {
#ifdef COMMS_DEBUG_OBJECT_COUNT
        --counter;
#endif
    }
};

class ListeningPeer : public Peer, virtual public ::yajr::Listener {
#ifdef COMMS_DEBUG_OBJECT_COUNT
    static ::boost::atomic<size_t> counter;
#endif
  public:
    explicit ListeningPeer(
            ::yajr::Peer::StateChangeCb connectionHandler,
            ::yajr::Listener::AcceptCb acceptHandler,
            void * data,
            uv_loop_t * listenerUvLoop = NULL,
            ::yajr::Peer::UvLoopSelector uvLoopSelector = NULL)
        :
            connectionHandler_(connectionHandler),
            acceptHandler_(acceptHandler),
            data_(data),
            ::yajr::comms::internal::Peer(false, uvLoopSelector, kPS_UNINITIALIZED),
            ::yajr::Listener()
        {
            handle_.loop = _.listener_.uvLoop_ = listenerUvLoop ? : uv_default_loop();
            getLoopData()->up();
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
    void onError(int error) {
        if(acceptHandler_) {
            acceptHandler_(this, data_, error);
        }
    }
    virtual void retry();

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
    virtual bool __checkInvariants() const __attribute__((no_instrument_function));
#endif
    ::yajr::Peer::StateChangeCb getConnectionHandler() const {
        return connectionHandler_;
    }

    void * getConnectionHandlerData() {
        return
            acceptHandler_
            ?
            acceptHandler_(this, data_, 0)
            :
            NULL;
    }

    ::yajr::Peer::UvLoopSelector getUvLoopSelector() const {
        return uvLoopSelector_;
    }

    int setAddrFromIpAndPort(const std::string& ip_address, uint16_t port);

  private:
    struct sockaddr_storage listen_on_;

    ::yajr::Peer::StateChangeCb const connectionHandler_;
    ::yajr::Listener::AcceptCb const acceptHandler_;
    void * const data_;
};

template < ::opflex::logging::OFLogHandler::Level LOGGING_LEVEL >
void internal::Peer::LoopData::walkAndDumpHandlesCb(uv_handle_t* h, void* loopData) {

    char const * type;

    switch (h->type) {
#define X(uc, lc)                               \
        case UV_##uc: type = #lc;               \
            break;
      UV_HANDLE_TYPE_MAP(X)
#undef X
      default: type = "<unknown>";
    }

    LOG(LOGGING_LEVEL)
        << static_cast<Peer::LoopData const *>(loopData)
        << " pending handle of type "
        << type
        << " @"
        << reinterpret_cast<void const *>(h)
        << " which is "
        << (uv_is_closing(h) ? "" : "not ")
        << "closing"
        ;

}

}}}

#endif /* _INCLUDE__OPFLEX__COMMS_INTERNAL_HPP */
