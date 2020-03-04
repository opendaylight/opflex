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

#include <opflex/yajr/rpc/send_handler.hpp>
#include <opflex/yajr/rpc/message_factory.hpp>
#include <opflex/yajr/yajr.hpp>
#include <opflex/yajr/rpc/rpc.hpp>
#include <opflex/yajr/transport/PlainText.hpp>

#include <opflex/logging/OFLogHandler.h>
#include "opflex/util/LockGuard.h"

#include <rapidjson/document.h>
#include <uv.h>

#include <boost/intrusive/list.hpp>

#include <sstream>  /* for basic_stringstream<> */
#include <iostream>

#define uv_close(h, cb)                        \
    do {                                       \
        uv_handle_t * _h = h;                  \
        uv_close_cb _cb = cb;                  \
        uv_close(_h, _cb);                     \
    } while(0)


namespace yajr {
    namespace comms {
        namespace internal {

template <typename Iterator>
std::vector<iovec> get_iovec(Iterator from, Iterator to) {

    std::vector<iovec> iov;

    typedef typename std::iterator_traits<Iterator>::value_type value_type;

    value_type const * base = &*from, * addr = base;

    while (from != to) {

        ++addr;
        ++from;

        if (
                ((addr) != &*from)
              ||
                (from == to)
           ) {
            iovec i = {
                const_cast< void * >(static_cast< void const * >(base)),
                static_cast<size_t>(static_cast< char const * >(addr)
                                    -
                                    static_cast< char const * >(base))
            };
            iov.push_back(i);
            addr = base = &*from;
        }

    }
            iovec i = {
                const_cast< void * >(static_cast< void const * >(base)),
                static_cast<size_t>(static_cast< char const * >(addr)
                                    -
                                    static_cast< char const * >(base))
            };
            if (i.iov_len) iov.push_back(i);

    return iov;
}

using namespace yajr::comms;
class ActivePeer;
class ActiveTcpPeer;
class CommunicationPeer;

/* we pick the storage class specifier here, and omit it at the definitions */
void alloc_cb(uv_handle_t * _, size_t size, uv_buf_t* buf);
void on_close(uv_handle_t * h);
void on_write(uv_write_t *req, int status);
void on_read(uv_stream_t * h, ssize_t nread, uv_buf_t const * buf);
void on_passive_connection(uv_stream_t * server_handle, int status);
int connect_to_next_address(ActiveTcpPeer * peer, bool swap_stack = true);
void on_active_connection(uv_connect_t *req, int status);
void on_resolved(uv_getaddrinfo_t * req, int status, struct addrinfo *resp);


typedef ::boost::intrusive::list_base_hook<
    ::boost::intrusive::link_mode< ::boost::intrusive::auto_unlink> >
    SafeListBaseHook;

/**
 * Peer
 */
class Peer : public SafeListBaseHook {
  public:

    /**
     * List of peers
     */
    typedef ::boost::intrusive::list<Peer,
            ::boost::intrusive::constant_time_size<false> > List;

    /**
     * Data about a libuv loop
     */
    class LoopData {
      public:

#define VaS(YY, s) YY(s, #s)
#define PEER_STATE_MAP(XX)               \
          VaS(XX, ONLINE)                \
          VaS(XX, LISTENING)             \
          VaS(XX, TO_RESOLVE)            \
          VaS(XX, TO_LISTEN)             \
          VaS(XX, RETRY_TO_CONNECT)      \
          VaS(XX, RETRY_TO_LISTEN)       \
          VaS(XX, ATTEMPTING_TO_CONNECT) \
          VaS(XX, PENDING_DELETE)

        /** Peer state */
        typedef enum {
#define XX(v, _) v,
          PEER_STATE_MAP(XX)
#undef XX
          /* don't touch past here */
          TOTAL_STATES
        } PeerState;

        /**
         * Used to close a set of handles
         */
        struct CloseHandle {
            /** libuv loop data */
            LoopData const * loopData;
            /** libuv close cb */
            uv_close_cb      closeCb;
        };

        /**
         * Used to count the number of handles
         */
        struct CountHandle {
            /** libuv loop data */
            LoopData const * loopData;
            /** counter */
            size_t           counter;
        };

        /**
         * UV loop data
         * @param loop uv loop
         */
        explicit LoopData(uv_loop_t * loop)
        :
            lastRun_(uv_now(loop)),
            destroying_(false),
            refCount_(1)
        {
            uv_prepare_init(loop, &prepare_);
            uv_prepare_start(&prepare_, &onPrepareLoop);
            prepare_.data = this;
            uv_timer_init(loop, &prepareAgain_);
            prepareAgain_.data = this;
            uv_async_init(loop, &kickLibuv_, NULL);
        }

        /**
         * Get libuv loop data
         * @param uv_loop libuv loop
         * @return libuv loop data
         */
        static Peer::LoopData * getLoopData(uv_loop_t * uv_loop) {
            return static_cast<Peer::LoopData *>(uv_loop->data);
        }

        /**
         * Add a new peer
         * @param uv_loop libuv loop
         * @param peerState initial peer state
         * @param peer peer
         */
        static void addPeer(
                uv_loop_t * uv_loop,
                Peer::LoopData::PeerState peerState,
                Peer* peer) {
            opflex::util::LockGuard guard(&peerMutex);
            (&getLoopData(uv_loop)->peers[peerState])->push_back(*peer);
        }

        /**
         * Get the current number of peers in the given state
         *
         * @param uv_loop libuv loop
         * @param peerState Peer state to match on
         * @return count of peers in given state
         */
        static std::size_t getPeerCount(
                uv_loop_t * uv_loop,
                Peer::LoopData::PeerState peerState) {
            opflex::util::LockGuard guard(&peerMutex);
            return (&getLoopData(uv_loop)->peers[peerState])->size();
        }

        /**
         * Destroy the peer
         *
         * @param now Whether the peer should be destroyed immediately
         */
        void destroy(bool now = false);

        /** Mark the peer up */
        void up();

        /** Mark the peer down */
        void down();

        /** Workaround libuv issues by manually */
        void kickLibuv() {
            /* workaround for libuv syncronous uv_pipe_connect() failures bug */
            uv_async_send(&kickLibuv_);
        }

        /** Iterate over handles and close each one
         *
         * @param handle UV handle
         * @param closeHandles Close handle
         */
        static void walkAndCloseHandlesCb(uv_handle_t* handle, void* closeHandles);

        /** Iterate and count the number of handles
         *
         * @param handle UV handle
         * @param countHandles Resulting count
         */
        static void walkAndCountHandlesCb(uv_handle_t* handle, void* countHandles);

      private:
        friend std::ostream& operator<< (std::ostream&, Peer::LoopData const *);

        ~LoopData();

        struct RetryPeer;

        struct PeerDisposer;

        struct PeerDeleter;

        Peer::List peers[LoopData::TOTAL_STATES];
        void onPrepareLoop();
        static void onPrepareLoop(uv_prepare_t *);
        static void fini(uv_handle_t *);
        static uv_mutex_t peerMutex;
        uv_prepare_t prepare_;
        uv_async_t kickLibuv_;
        uv_timer_t prepareAgain_;
        uint64_t lastRun_;
        bool destroying_;
        uint64_t refCount_;

        friend class Peer;
    };

    /**
     * Get the peer from the handle
     *
     * @tparam T Type of expected Peer
     * @tparam U Type of handle
     * @param h libuv handle
     * @return Peer of type T
     */
    template <typename T, typename U>
    static T * get(U * h);

    /**
     * Get the peer from the handle
     *
     * @param r libuv handle
     * @return The associated Peer
     */
    static CommunicationPeer * get(uv_write_t * r);

    /**
     * Get the peer from the handle
     *
     * @param h libuv handle
     * @return The associated Peer
     */
    static CommunicationPeer * get(uv_timer_t * h);

    /**
     * Get the peer from the handle
     *
     * @param r libuv handle
     * @return The associated Peer
     */
    static ActivePeer * get(uv_connect_t * r);

    /**
     * Get the peer from the handle
     *
     * @param r libuv handle
     * @return The associated Peer
     */
    static ActiveTcpPeer * get(uv_getaddrinfo_t * r);

    /* Ideally nested as Peer::PeerStatus, but this is only possible with C++11 */
    /** Valid peer statuses */
    enum PeerStatus {
        kPS_ONLINE            = 0,  /* <--- don't touch these! */
        kPS_LISTENING         = 0,  /* <--- don't touch these! */
        kPS_UNINITIALIZED     = 1,
        kPS_DISCONNECTED      = 2,
        kPS_RESOLVING         = 3,
        kPS_RESOLVED          = 4,
        kPS_CONNECTING        = 5,

        kPS_FAILED_TO_RESOLVE = 6,
        kPS_FAILED_BINDING    = 6,
        kPS_FAILED_LISTENING  = 6,

        kPS_PENDING_DELETE    = 7,
    };
    /**
     * Construct a new peer
     *
     * @param passive Is this a passive peer
     * @param uvLoopSelector Function pointer type for the uv_loop selector
     * @param status Initial peer status
     */
    explicit Peer(bool passive,
         ::yajr::Peer::UvLoopSelector uvLoopSelector = NULL,
         Peer::PeerStatus status = kPS_UNINITIALIZED)
            :
              uvLoopSelector_(uvLoopSelector ? : &uvDefaultLoop),
              uvRefCnt_(1),
              connected_(0),
              destroying_(0),
              passive_(passive),
              choked_(1),
              createFail_(1),
              status_(status),
              nullTermination(true)
            {
                getHandle()->data = this;
                /* FIXME: this hack is filthy and unix-only */
                getHandle()->flags = 0x02 /* UV_CLOSED */;
            }

    /**
     * Retry connection to peer
     */
    virtual void retry() = 0;

    /**
     * Mark the peer as up
     */
    void up();

    /**
     * Mark the peer as down
     *
     * @return False if there are still other references to this peer
     */
    bool down();

    /**
     * Destroy the peer
     *
     * @param now Destroy immediately or not
     */
    virtual void destroy(bool now = false) = 0;

    /**
     * Get the uv_loop_t * for this peer
     *
     * @return the uv_loop_t * for this peer
     */
    uv_loop_t * getUvLoop() const {
        return getHandle()->loop;
    }

    /** Add a new peer to the list */
    void insert(Peer::LoopData::PeerState peerState);

    /** Manually unlink peer */
    void unlink();

    /** libuv handles */
    union {
        uv_handle_t     handle_;
        uv_tcp_t    tcp_handle_;
        uv_pipe_t  pipe_handle_;
    };

    /** Get the libuv handle */
    uv_handle_t * getHandle() {
        return &handle_;
    }

    /** Get the libuv handle */
    uv_handle_t const * getHandle() const {
        return &handle_;
    }

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
    /** Function pointer type for the uv_loop selector method to be used to
     *  select which particular uv_loop to assign a peer to.
     */
    ::yajr::Peer::UvLoopSelector uvLoopSelector_;
    /** reference count */
    unsigned int  uvRefCnt_;
    /** Is this peer connected */
    unsigned char connected_  :1;
    /** Is the peer begin destroyed */
    unsigned char destroying_ :1;
    /** Is this peer passive */
    unsigned char passive_    :1;
    /** Is the peer currently choked */
    mutable
    unsigned char choked_     :1;
    /** Did the peer creation fail */
    unsigned char createFail_ :1;
    /** Current status of the peer */
    unsigned char status_     :3;
    /** Should messages be null terminated */
    bool nullTermination;

  protected:
    /* don't leak memory! */
    virtual ~Peer();
    /** Get the default UV loop */
    static uv_loop_t * uvDefaultLoop(void *) {
        return uv_default_loop();
    }

    /** Called when peer is being deleted */
    virtual void onDelete() {}

    /**
     * Get libuv loop data
     *
     * @return libuv loop data
     */
    Peer::LoopData * getLoopData() const {
        return Peer::LoopData::getLoopData(getUvLoop());
    }

    /**
     * << operator
     * @return stream
     */
    friend std::ostream& operator<< (std::ostream&, Peer const *);
};
static_assert (sizeof(Peer) <= 4096, "Peer won't fit on one page");

/**
 * Abstract communication peer
 */
class CommunicationPeer : public Peer, virtual public ::yajr::Peer {

    friend
    std::ostream& operator<< (
        std::ostream& os,
        ::yajr::comms::internal::Peer const * p
    );

    template< typename E >
    friend
    int
    transport::Cb< E >::send_cb(CommunicationPeer const *);

    template< typename E >
    friend
    void
    transport::Cb< E >::on_sent(CommunicationPeer const *);

    template< typename E >
    friend
    void
    transport::Cb< E >::on_read(uv_stream_t *, ssize_t, uv_buf_t const *);

    template< typename E >
    friend
    struct
    transport::Cb< E >::StaticHelpers;

  public:

    /**
     * Construct a communication peer
     *
     * @param passive Is this a passive peer
     * @param connectionHandler connection handler
     * @param data opaque data
     * @param uvLoopSelector uv loop selector
     * @param status initial peer status
     */
    explicit CommunicationPeer(bool passive,
        ::yajr::Peer::StateChangeCb connectionHandler,
        void * data,
        ::yajr::Peer::UvLoopSelector uvLoopSelector = NULL,
        internal::Peer::PeerStatus status = kPS_UNINITIALIZED)
            :
                ::yajr::Peer(),
                internal::Peer(passive, uvLoopSelector, status),
                connectionHandler_(connectionHandler),
                data_(data),
                writer_(s_),
                pendingBytes_(0),
                nextId_(0),
                keepAliveInterval_(0),
                lastHeard_(0),
                transport_(transport::PlainText::getPlainTextTransport())
            {
                req_.data = this;
                getHandle()->loop = uvLoopSelector_(getData());
                getLoopData()->up();
            }

    /**
     * Retry connection to peer
     */
    virtual void retry() = 0;

    /**
     * Read from the buffer.
     * This impl does not use null chars to delimit msgs
     *
     * @param buffer buffer
     * @param nread Number of bytes
     */
    void readBufNoNull(char* buffer,
                       size_t nread);


    /** Called on write to peer */
    void onWrite();

    /**
     * Add frame delimiter
     */
    void delimitFrame() const {
        s_.Put('\0');
    }

    /**
     * Write to peer
     * @return rc
     */
    int write() const;

    /**
     * Write iovec to peer
     * @return rc
     */
    int writeIOV(std::vector<iovec> &) const;

    /** Stop reading from the stream of data */
    int   choke() const;
    /** Start reading from the stream of data again */
    int unchoke() const;

    /** get opaque data associated with peer */
    void * getData() const {
        return data_;
    }

    /** Called when peer is being deleted */
    virtual void onDelete() {
        connectionHandler_(dynamic_cast<yajr::Peer *>(this), data_, ::yajr::StateChange::DELETE, 0);
    }

    /**
     * Start TCP keepalive to peer
     * @param begin delay before starting
     * @param repeat repeat
     * @param interval interval
     */
    virtual void startKeepAlive(
            uint64_t begin    =  100,
            uint64_t repeat   = 1250,
            uint64_t interval = 9000);

    /**
     * Stop TCP keepalive
     */
    virtual void stopKeepAlive();

    /**
     * CB for timer expiry
     * @param timer libuv timer
     */
    static void on_timeout(uv_timer_t * timer);

    /** send echo req to peer */
    void sendEchoReq();

    /**
     * Called on timeout
     */
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

    /**
     * Bump the last time the peer was heard
     */
    void bumpLastHeard() const;

    /**
     * Current timestamp in ms
     * @return current timestamp in ms
     */
    uint64_t now() const {
        return uv_now(getUvLoop());
    }

    /**
     * Called on error
     * @param error rc
     */
    void onError(int error) const {
        connectionHandler_(
                const_cast<CommunicationPeer *>(this),
                data_,
                ::yajr::StateChange::FAILURE,
                error);
    }

    /**
     * Called on transport error
     * @param error rc
     */
    void onTransportError(int error) const {
        connectionHandler_(
                const_cast<CommunicationPeer *>(this),
                data_,
                ::yajr::StateChange::TRANSPORT_FAILURE,
                error);
    }

    /**
     * Called on connect
     */
    void onConnect();

    /**
     * Trigger cleanup of resource on a disconnect
     */
    virtual void onDisconnect();

    /**
     * Disconnect a peer
     * @param now Disconnect immediately if true
     */
    virtual void disconnect(bool now = false) {
        onDisconnect();
    }

    /**
     * Get the peer name
     * @param remoteAddress sockaddr
     * @param len length
     * @return peer name
     */
    virtual int getPeerName(struct sockaddr* remoteAddress, int* len) const {
        return uv_tcp_getpeername(
                reinterpret_cast<uv_tcp_t const *>(getHandle()),
                remoteAddress,
                len);
    }

    /**
     * Get the socket name
     * @param remoteAddress sockaddr
     * @param len length
     * @return socket name
     */
    virtual int getSockName(struct sockaddr* remoteAddress, int* len) const {
        return uv_tcp_getsockname(
                reinterpret_cast<uv_tcp_t const *>(getHandle()),
                remoteAddress,
                len);
    }

    /**
     * Destroy the peer
     *
     * @param now Whether the peer should be destroyed immediately
     */
    virtual void destroy(bool now = false);

    /**
     * Get the keepalive interval
     *
     * @return Current keepalive interval
     */
    uint64_t getKeepAliveInterval() const {
        return keepAliveInterval_;
    }

    union {
        mutable uv_write_t write_req_;
        uv_connect_t connect_req_;
        uv_getaddrinfo_t dns_req_;
        uv_req_t req_;
    };

    /**
     * Get writer
     * @return writer
     */
    ::yajr::rpc::SendHandler & getWriter() const {
        writer_.Reset(s_);
        return writer_;
    }

    /**
     * Initialize TCP
     *
     * @return 0 on success
     */
    int tcpInit();

    /**
     * Get the transport engine associated with the peer
     * @tparam E Type of transport engine
     * @return Transport engine of type E
     */
    template< class E >
    E * getEngine() const {
        return transport_.getEngine< E >();
    }

    /**
     * Detach transport
     *
     * @return Tranport pointer
     */
    void * detachTransport() {
        transport_.~Transport();

        return &transport_;
    }
  protected:
    /* don't leak memory! */
    virtual ~CommunicationPeer() {}

  private:

    mutable ::yajr::internal::StringQueue s_;

    ::yajr::Peer::StateChangeCb connectionHandler_;
    void * data_;

    mutable rapidjson::Document docIn_;

    mutable ::yajr::rpc::SendHandler writer_;
    mutable size_t pendingBytes_;
    mutable uint64_t nextId_;

    uint64_t keepAliveInterval_;
    mutable uint64_t lastHeard_;

    ::yajr::transport::Transport transport_;

    mutable std::stringstream ssIn_;

    void resetSsIn() const {
        const static std::stringstream initialFmt;
        const static std::string emptyStr;

        ssIn_.str(emptyStr);
        ssIn_.clear();
        ssIn_.copyfmt(initialFmt);
    }

    yajr::rpc::InboundMessage * parseFrame() const;

    void readBufferZ(
            char const * bufferZ,
            size_t n) const;

    void readBuffer(
            char * buffer,
            size_t nread,
            bool canWriteJustPastTheEnd = false) const;

    size_t readChunk(char const * buffer) const {
        ssize_t chunk_size = - ssIn_.tellp();
        ssIn_ << buffer;
        chunk_size += ssIn_.tellp();
        return chunk_size;
    }
};
static_assert (sizeof(CommunicationPeer) <= 4096, "CommunicationPeer won't fit on one page");

/**
 * Active peer
 */
class ActivePeer : public CommunicationPeer {
  public:
    /**
     * Construct a new active peer
     * @param connectionHandler connection handler
     * @param data opaque data
     * @param uvLoopSelector uv loop selector
     */
    explicit ActivePeer(
            ::yajr::Peer::StateChangeCb connectionHandler,
            void * data,
            ::yajr::Peer::UvLoopSelector uvLoopSelector = NULL)
        :
            CommunicationPeer(
                    false,
                    connectionHandler,
                    data,
                    uvLoopSelector,
                    kPS_RESOLVING)
        {
        }

    /**
     * Called on a failed connection
     *
     * @param rc return code received from failure
     */
    virtual void onFailedConnect(int rc) = 0;

    /**
     * Retry connection to peer
     */
    virtual void retry() = 0;

    /**
     * Destroy the peer
     *
     * @param now Whether the peer should be destroyed immediately
     */
    virtual void destroy(bool now = false);

  protected:
    /* don't leak memory! */
    virtual ~ActivePeer() {}
};
static_assert (sizeof(ActivePeer) <= 4096, "ActivePeer won't fit on one page");

/**
 * Active TCP peer
 */
class ActiveTcpPeer : public ActivePeer {
  public:
    /**
     * Construct an active TCP peer
     * @param hostname hostname
     * @param service service name
     * @param connectionHandler connection handler
     * @param data opaque data
     * @param uvLoopSelector libuv loop selector
     */
    explicit ActiveTcpPeer(
            std::string const & hostname,
            std::string const & service,
            ::yajr::Peer::StateChangeCb connectionHandler,
            void * data,
            ::yajr::Peer::UvLoopSelector uvLoopSelector = NULL)
        :
            ActivePeer(
                    connectionHandler,
                    data,
                    uvLoopSelector),
            hostname_(hostname),
            service_(service)
        {
            createFail_ = 0;
        }

    /**
     * Called on a failed connection
     *
     * @param rc return code received from failure
     */
    virtual void onFailedConnect(int rc);

    /**
     * Get hostname
     * @return hostname
     */
    char const * getHostname() const {
        return hostname_.c_str();
    }

    /**
     * Get the service name
     * @return service name
     */
    char const * getService() const {
        return service_.c_str();
    }

    /**
     * Retry the connection to the peer
     */
    virtual void retry();
  protected:
    /* don't leak memory! */
    virtual ~ActiveTcpPeer() {}
  private:
    std::string const hostname_;
    std::string const service_;
};
static_assert (sizeof(ActiveTcpPeer) <= 4096, "ActiveTcpPeer won't fit on one page");

/**
 * Active UNIX Peer
 */
class ActiveUnixPeer : public ActivePeer {
  public:
    /**
     * Construct an active UNIX peer
     * @param socketName socket name
     * @param connectionHandler connection handler
     * @param data opaque data
     * @param uvLoopSelector libuv loop selector
     */
    explicit ActiveUnixPeer(
            std::string const & socketName,
            ::yajr::Peer::StateChangeCb connectionHandler,
            void * data,
            ::yajr::Peer::UvLoopSelector uvLoopSelector = NULL)
        :
            ActivePeer(
                    connectionHandler,
                    data,
                    uvLoopSelector),
            socketName_(socketName)
        {
            _.ai = NULL;
            createFail_ = 0;
        }

    /**
     * Called on a failed connection
     *
     * @param rc return code received from failure
     */
    virtual void onFailedConnect(int rc);
    /**
     * Retry the connection to the peer
     */
    virtual void retry();
  protected:
    /* don't leak memory! */
    virtual ~ActiveUnixPeer() {}

  private:
    std::string const socketName_;
};
static_assert (sizeof(ActiveUnixPeer) <= 4096, "ActiveUnixPeer won't fit on one page");

/**
 * Passive peer
 */
class PassivePeer : public CommunicationPeer {
  public:
    /**
     * Construct a passive peer
     * @param connectionHandler connection handler
     * @param data opaque data
     * @param uvLoopSelector libuv loop selector
     */
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
            createFail_ = 0;
        }

    /**
     * Retry connection to peer
     */
    virtual void retry() {
        assert(0);
    }

  protected:
    /* don't leak memory! */
    virtual ~PassivePeer() {}
};
static_assert (sizeof(PassivePeer) <= 4096, "PassivePeer won't fit on one page");

/**
 * Listening Peer
 */
class ListeningPeer : public Peer, virtual public ::yajr::Listener {
  public:
    /**
     * Construct a new listening peer
     * @param connectionHandler connection handler
     * @param acceptHandler accept handler
     * @param data opaque data
     * @param listenerUvLoop libuv loop listener
     * @param uvLoopSelector libuv loop selector
     */
    explicit ListeningPeer(
            ::yajr::Peer::StateChangeCb connectionHandler,
            ::yajr::Listener::AcceptCb acceptHandler,
            void * data,
            uv_loop_t * listenerUvLoop = NULL,
            ::yajr::Peer::UvLoopSelector uvLoopSelector = NULL)
        :
            ::yajr::Listener(),
            ::yajr::comms::internal::Peer(false, uvLoopSelector, kPS_UNINITIALIZED),
            connectionHandler_(connectionHandler),
            acceptHandler_(acceptHandler),
            data_(data)
        {
            getHandle()->loop = _.listener_.uvLoop_ = listenerUvLoop ? : uv_default_loop();
            getLoopData()->up();
        }

    /**
     * Create a new passive peer
     * @return passive peer
     */
    virtual PassivePeer * getNewPassive() = 0;

    /**
     * Called on error
     * @param error rc
     */
    void onError(int error) {
        if(acceptHandler_) {
            acceptHandler_(this, data_, error);
        }
    }

    /**
     * Retry connection to peer
     */
    virtual void retry() = 0;

    /**
     * Destroy the peer
     *
     * @param now Whether the peer should be destroyed immediately
     */
    virtual void destroy(bool now = false);

    /**
     * Get the connection handler for the peer
     * @return connection handler
     */
    ::yajr::Peer::StateChangeCb getConnectionHandler() const {
        return connectionHandler_;
    }

    /**
     * Get the connection handler data
     * @return connection handler data
     */
    void * getConnectionHandlerData() {
        return
            acceptHandler_
            ?
            acceptHandler_(this, data_, 0)
            :
            NULL;
    }

    /**
     * Get the libuv loop selector
     * @return libuv loop selector
     */
    ::yajr::Peer::UvLoopSelector getUvLoopSelector() const {
        return uvLoopSelector_;
    }

  private:

    ::yajr::Peer::StateChangeCb const connectionHandler_;
    ::yajr::Listener::AcceptCb const acceptHandler_;
    void * const data_;
};
static_assert (sizeof(ListeningPeer) <= 4096, "ListeningPeer won't fit on one page");

/**
 * Listening TCP peer
 */
class ListeningTcpPeer : public ListeningPeer {
  public:
    /**
     * Retry the connection to the peer
     */
    virtual void retry();
    /**
     * Set src ip/port
     * @param ip_address src address
     * @param port src port
     * @return rc
     */
    int setAddrFromIpAndPort(const std::string& ip_address, uint16_t port);

    /**
     * Construct a listening TCP peer
     * @param connectionHandler connection handler
     * @param acceptHandler accept handler
     * @param data opaque data
     * @param listenerUvLoop libuv loop listener
     * @param uvLoopSelector libuv loop selector
     */
    explicit ListeningTcpPeer(
            ::yajr::Peer::StateChangeCb connectionHandler,
            ::yajr::Listener::AcceptCb acceptHandler,
            void * data,
            uv_loop_t * listenerUvLoop = NULL,
            ::yajr::Peer::UvLoopSelector uvLoopSelector = NULL)
        :
          ListeningPeer(
                  connectionHandler,
                  acceptHandler,
                  data,
                  listenerUvLoop,
                  uvLoopSelector
          ) {
              createFail_ = 0;
              listen_on_ = sockaddr_storage();
          }

    /**
     * Create a new passive peer
     * @return passive peer
     */
    virtual PassivePeer * getNewPassive();

  private:
    struct sockaddr_storage listen_on_;

};
static_assert (sizeof(ListeningTcpPeer) <= 4096, "ListeningTcpPeer won't fit on one page");

/**
 * Listening UNIX peer
 */
class ListeningUnixPeer : public ListeningPeer {
  public:
    /**
     * Retry connection to peer
     */
    virtual void retry();

    /**
     * Construct a new ListeningUnixPeer
     * @param socketName socket name
     * @param connectionHandler connection handler
     * @param acceptHandler accept handler
     * @param data opaque data
     * @param listenerUvLoop libuv loop
     * @param uvLoopSelector libuv loop selector
     */
    explicit ListeningUnixPeer(
            std::string const & socketName,
            ::yajr::Peer::StateChangeCb connectionHandler,
            ::yajr::Listener::AcceptCb acceptHandler,
            void * data,
            uv_loop_t * listenerUvLoop = NULL,
            ::yajr::Peer::UvLoopSelector uvLoopSelector = NULL)
        :
          ListeningPeer(
                  connectionHandler,
                  acceptHandler,
                  data,
                  listenerUvLoop,
                  uvLoopSelector
          ),
          socketName_(socketName)
        {
            createFail_ = 0;
        }

    /**
     * Create a new passive peer
     * @return passive peer
     */
    virtual PassivePeer * getNewPassive();

  private:
    std::string const socketName_;
};
static_assert (sizeof(ListeningUnixPeer) <= 4096, "ListeningUnixPeer won't fit on one page");

/** Peer disposer */
struct Peer::LoopData::PeerDisposer {

    /** () operator */
    void operator () (Peer *peer);

    /**
     * Construct a peer dispose
     * @param now Should peer be disposed immediately
     */
    PeerDisposer(bool now = false);

  private:
    bool const now_;
};

/** Peer retryer */
struct Peer::LoopData::RetryPeer {

    /** () operator */
    void operator () (Peer *peer);

};

/** Peer deleter */
struct Peer::LoopData::PeerDeleter {

    /** () operator */
    void operator () (Peer *peer);

};

template <typename T, typename U>
T * Peer::get(U * h) {
    T * peer = static_cast<T *>(h->data);

    return peer;
}

/**
 * Get libuv handle type
 * @param h libuv handle
 * @return type name
 */
char const * getUvHandleType(uv_handle_t * h);

/**
 * Get libuv handle field
 * @param h libuv handle
 * @param peer peer
 * @return field name
 */
char const * getUvHandleField(uv_handle_t * h, internal::Peer * peer);

} // namespace internal
} // namespace comms
} // namespace yajr

#endif /* _INCLUDE__OPFLEX__COMMS_INTERNAL_HPP */

