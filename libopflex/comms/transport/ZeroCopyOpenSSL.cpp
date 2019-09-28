/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

/* This must be included before anything else */
#if HAVE_CONFIG_H
#  include <config.h>
#endif


#include <yajr/transport/ZeroCopyOpenSSL.hpp>

#include <yajr/internal/comms.hpp>

#include <opflex/logging/internal/logging.hpp>

#include <uv.h>
#include <openssl/err.h>

#include <algorithm>

#include <sys/uio.h>
#include <sys/stat.h>
#include <cassert>

namespace {

    bool const SSL_ERROR = true;

}

#define                                                           \
__IF_SSL_ERROR_INTO(_, variable, condition, ...)                  \
    if (condition)                                                \
        for (                                                     \
            std::string const & variable =                        \
                ZeroCopyOpenSSL::dumpOpenSslErrorStackAsString()  \
          ,                                                       \
            * b = &variable                                       \
          ;                                                       \
            variable.size() && b                                  \
          ;                                                       \
            b = NULL                                              \
        )
#define                                                           \
IF_SSL_ERROR(...)                                                 \
    __IF_SSL_ERROR_INTO(_, ##__VA_ARGS__, SSL_ERROR, true)

#define IF_SSL_EMIT_ERRORS(___peer___)                                          \
    for (                                                                       \
          int ___firstErr = ERR_peek_error(), ___lastErr = ERR_peek_last_error()\
        ;                                                                       \
          (                                                                     \
            ___firstErr && (___peer___->onTransportError(___firstErr), true) && \
            ___lastErr  && (___lastErr != ___firstErr) &&                       \
            (___peer___->onTransportError( ___lastErr), true)                   \
          ) || (                                                                \
            ___firstErr                                                         \
          )                                                                     \
        ;                                                                       \
          ___firstErr = 0                                                       \
        )

namespace yajr {
    namespace transport {

using namespace yajr::comms::internal;

template<>
struct Cb< ZeroCopyOpenSSL >::StaticHelpers {

/*
                     +---------+--------------+--------------+
  tryToDecrypt() <---|         |              |              |<--- unchoke(),
                     | bioSSL_ | bioInternal_ | bioExternal_ |     on_read()
  tryToEncrypt() --->|         |              |              |---> tryToSend()
                     +---------+--------------+--------------+
 */

    static ssize_t tryToDecrypt(CommunicationPeer const * peer);
    static ssize_t tryToEncrypt(CommunicationPeer const * peer);
    static int tryToSend(CommunicationPeer const * peer);

};

ssize_t Cb< ZeroCopyOpenSSL >::StaticHelpers::tryToDecrypt(
        CommunicationPeer const * peer) {

    VLOG(5)
        << peer
    ;

    /* we have to process the decrypted data, if any is available */

    ZeroCopyOpenSSL * e = peer->getEngine<ZeroCopyOpenSSL>();

#  define tryRead 24576
    char buffer[tryRead + 1];
    ssize_t nread = 0;
    ssize_t totalRead = 0;

    while(0 < (nread = BIO_read(
                    e->bioSSL_,
                    buffer,
                    tryRead))) {
#  undef tryRead

        if (nread > 0) {
            VLOG(6)
                << peer
                << " just decrypted "
                << nread
                << " bytes into buffer starting @"
                << reinterpret_cast<void*>(buffer)
                << " ("
                << std::string(buffer, nread)
                << ")"
            ;
            peer->readBuffer(buffer, nread, true);
            totalRead += nread;
        } else {
            VLOG(2)
                << peer
                << " nread = "
                << nread
                << " RETRY="
                << BIO_should_retry     (e->bioSSL_)
                << " R="
                << BIO_should_read      (e->bioSSL_)
                << " W="
                << BIO_should_write     (e->bioSSL_)
                << " S="
                << BIO_should_io_special(e->bioSSL_)
            ;
        }
    }
    IF_SSL_EMIT_ERRORS(peer) {
        IF_SSL_ERROR(sslErr) {
            LOG(ERROR)
                << peer
                << " Failed to decrypt input: "
                << sslErr
            ;
        }
        const_cast<CommunicationPeer *>(peer)->onDisconnect();
    }

    VLOG(totalRead ? 4 : 3)
        << peer
        << " Returning: "
        << (totalRead ?: nread)
    ;
    /* short-circuit a single non-positive nread */
    return totalRead ?: nread;

}

ssize_t Cb< ZeroCopyOpenSSL >::StaticHelpers::tryToEncrypt(
        CommunicationPeer const * peer) {

    VLOG(5)
        << peer
    ;

    assert(!peer->pendingBytes_);
    if (peer->pendingBytes_) {
        VLOG(3)
            << peer
            << " has already got pending bytes. Should have not tried!"
        ;
        return 0;
    }

    /* we have to encrypt the plaintext data, if any is available */
    if (!peer->s_.deque_.size()) {
        VLOG(4)
            << peer
            << " has no data to send"
        ;

        return 0;
    }

    ZeroCopyOpenSSL * e = peer->getEngine<ZeroCopyOpenSSL>();

    ssize_t totalWrite = 0;
    ssize_t nwrite = 0;
    ssize_t tryWrite;

    std::vector<iovec> iovIn =
        ::yajr::comms::internal::get_iovec(
                peer->s_.deque_.begin(),
                peer->s_.deque_.end()
        );

    std::vector<iovec>::iterator iovInIt;
    for (iovInIt = iovIn.begin(); iovInIt != iovIn.end(); ++iovInIt) {

        tryWrite = iovInIt->iov_len;

        assert(tryWrite);

        nwrite = BIO_write(
                e->bioSSL_,
                iovInIt->iov_base,
                tryWrite);

        if (nwrite > 0) {

            totalWrite += nwrite;

            VLOG(5)
                << peer
                << " nwrite = "
                << nwrite
                << " tryWrite = "
                << tryWrite
                << " totalWrite = "
                << totalWrite
            ;

        } else {

            VLOG(3)
                << peer
                << " nwrite = "
                << nwrite
                << " tryWrite = "
                << tryWrite
                << " totalWrite = "
                << totalWrite
                << " RETRY="
                << BIO_should_retry     (e->bioSSL_)
                << " R="
                << BIO_should_read      (e->bioSSL_)
                << " W="
                << BIO_should_write     (e->bioSSL_)
                << " S="
                << BIO_should_io_special(e->bioSSL_)
            ;

        }

        if (nwrite < tryWrite) {
            /* includes case in which (nwrite <= 0) */
            break;
        }

    }
    IF_SSL_EMIT_ERRORS(peer) {
        IF_SSL_ERROR(sslErr, nwrite <= 0) {
            LOG(ERROR)
                << peer
                << " Failed to encrypt output: "
                << sslErr
            ;
        }
        const_cast<CommunicationPeer *>(peer)->onDisconnect();

        return 0;
    }

    peer->s_.deque_.erase(
            peer->s_.deque_.begin(),
            peer->s_.deque_.begin() + totalWrite
    );

    VLOG(totalWrite ? 4 : 3)
        << peer
        << " Returning: "
        << (totalWrite ?: nwrite)
    ;
    /* short-circuit a single non-positive nread */
    return totalWrite ?: nwrite;

}

int Cb< ZeroCopyOpenSSL >::StaticHelpers::tryToSend(
        CommunicationPeer const * peer) {

    VLOG(5)
        << peer
    ;

    if (peer->pendingBytes_) {
        LOG(WARNING)
            << peer
            << " has already "
            << peer->pendingBytes_
            << " pending"
        ;

        return 0;
    }

    iovec buf;

    ZeroCopyOpenSSL * e = peer->getEngine<ZeroCopyOpenSSL>();

    ssize_t nread = BIO_nread0(
            e->bioExternal_,
            (char**)&buf.iov_base);

    VLOG(4)
        << peer
        << ": "
        << nread
        << " bytes to be sent"
    ;

    if (nread <= 0) {
        VLOG(4)
            << peer
            << " nread = "
            << nread
            << " RETRY="
            << BIO_should_retry     (e->bioExternal_)
            << " R="
            << BIO_should_read      (e->bioExternal_)
            << " W="
            << BIO_should_write     (e->bioExternal_)
            << " S="
            << BIO_should_io_special(e->bioExternal_)
        ;

        return 0;
    }

    peer->pendingBytes_ = buf.iov_len = nread;
    e->lastOutBuf_ = static_cast<char *>(buf.iov_base);

    std::vector<iovec> iov(1, buf);

    return peer->writeIOV(iov);
}

template<>
int Cb< ZeroCopyOpenSSL >::send_cb(CommunicationPeer const * peer) {

    VLOG(5)
        << peer
    ;

    assert(!peer->pendingBytes_);

    (void) Cb< ZeroCopyOpenSSL >::StaticHelpers::tryToEncrypt(peer);
    return Cb< ZeroCopyOpenSSL >::StaticHelpers::tryToSend(peer);

}

template<>
void Cb< ZeroCopyOpenSSL >::on_sent(CommunicationPeer const * peer) {

    VLOG(4)
        << peer
    ;

    ZeroCopyOpenSSL * e = peer
        ->getEngine<ZeroCopyOpenSSL>();

    char * whereTheReadShouldHaveStarted = NULL;

    ssize_t advancement = BIO_nread(
            e->bioExternal_,
            &whereTheReadShouldHaveStarted,
            peer->pendingBytes_);

    bool giveUp = false;

    if (whereTheReadShouldHaveStarted != e->lastOutBuf_) {

        LOG(ERROR)
            << peer
            << "unexpected discrepancy: e->lastOutBuf_ = "
            << e->lastOutBuf_
            << " whereTheReadShouldHaveStarted = "
            << whereTheReadShouldHaveStarted
            << " will disconnect"
        ;

        giveUp = true;
    }

    if (advancement != static_cast<ssize_t>(peer->pendingBytes_)) {

        LOG(ERROR)
            << peer
            << "unexpected discrepancy: peer->pendingBytes_ = "
            << peer->pendingBytes_
            << " advancement = "
            << advancement
            << " will disconnect"
        ;

        giveUp = true;
    }

    assert(e->lastOutBuf_ == whereTheReadShouldHaveStarted);
    assert(advancement == static_cast<ssize_t>(peer->pendingBytes_));

    if (giveUp) {
        const_cast<CommunicationPeer *>(peer)->onDisconnect();
        return;
    }
}

template<>
void Cb< ZeroCopyOpenSSL >::alloc_cb(
        uv_handle_t * h
      , size_t size
      , uv_buf_t* buf
      ) {

    CommunicationPeer * peer = comms::internal::Peer::get<CommunicationPeer>(h);

    VLOG(4)
        << peer
    ;

    ZeroCopyOpenSSL * e = peer->getEngine<ZeroCopyOpenSSL>();

    ssize_t avail = BIO_nwrite0(
            e->bioExternal_,
            &buf->base);

    assert(avail >= 0);

    if ((size > SSIZE_MAX) || (size > static_cast<size_t>(avail))) {
        size = avail;
    }

    /* choke() peer if necessary, although it might deadlock us if we don't
     * make sure to properly unchoke() it after making any progress on any of
     * the other three SSL ports.
     */
    if (!size) {
        LOG(WARNING)
            << peer
            << " BIO pair is full, have to choke sender"
        ;

        peer->choke();
    }

    buf->len = size;
}

template<>
void Cb< ZeroCopyOpenSSL >::on_read(
        uv_stream_t * h
      , ssize_t nread
      , uv_buf_t const * buf
      ) {

    CommunicationPeer * peer = comms::internal::Peer::get<CommunicationPeer>(h);

    VLOG(4)
        << peer
    ;

    if (!peer->connected_) {
        return;
    }

    if (nread < 0) {

        VLOG(3)
            << "nread = "
            << nread
            << " ["
            << uv_err_name(nread)
            << "] "
            << uv_strerror(nread)
            << " => closing"
        ;

        peer->onDisconnect();
    }

    if (nread > 0) {

        VLOG(5)
            << peer
            << " read "
            << nread
            << " into buffer of size "
            << buf->len
        ;

        ZeroCopyOpenSSL * e = peer
            ->getEngine<ZeroCopyOpenSSL>();

        char * whereTheWriteShouldHaveStarted = NULL;

        /* we have to finally tell openSSL we have inserted this data */
        ssize_t advancement = BIO_nwrite(
                e->bioExternal_,
                &whereTheWriteShouldHaveStarted,
                nread);

        bool giveUp = false;

        if (whereTheWriteShouldHaveStarted != buf->base) {

            LOG(ERROR)
                << peer
                << "unexpected discrepancy: buf->base = "
                << buf->base
                << " whereTheWriteShouldHaveStarted = "
                << whereTheWriteShouldHaveStarted
                << " will disconnect"
            ;

            giveUp = true;
        }

        if (advancement != nread) {

            LOG(ERROR)
                << peer
                << "unexpected discrepancy: nread = "
                << nread
                << " advancement = "
                << advancement
                << " will disconnect"
            ;

            giveUp = true;
        }

        assert(buf->base == whereTheWriteShouldHaveStarted);
        assert(advancement == nread);

        if (giveUp) {
            peer->onDisconnect();
            return;
        }

        ssize_t decrypted =
            Cb< ZeroCopyOpenSSL >::StaticHelpers::tryToDecrypt(peer);

        if (decrypted <= 0) {
            VLOG(2)
                << peer
                << " decrypted = "
                << decrypted
                << " RETRY="
                << BIO_should_retry     (e->bioSSL_)
                << " R="
                << BIO_should_read      (e->bioSSL_)
                << " W="
                << BIO_should_write     (e->bioSSL_)
                << " S="
                << BIO_should_io_special(e->bioSSL_)
            ;
            if (BIO_should_retry(e->bioSSL_) && !peer->pendingBytes_) {
                (void) Cb< ZeroCopyOpenSSL >::StaticHelpers::tryToSend(peer);

                if (peer->pendingBytes_) {

                    VLOG(4)
                        << peer
                        << " Retried to send and emitted "
                        << peer->pendingBytes_
                        << " bytes"
                    ;

                    return;

                }

                if (Cb< ZeroCopyOpenSSL >::StaticHelpers::
                        tryToEncrypt(peer)) {
                    /* kick the can */
                    (void) Cb< ZeroCopyOpenSSL >::StaticHelpers::
                        tryToSend(peer);

                    VLOG(4)
                        << peer
                        << " Found no handshake data,"
                           " but found actual payload and sent "
                        << peer->pendingBytes_
                        << " bytes"
                    ;

                }
            }
        }

    }

}

#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
uv_rwlock_t * ZeroCopyOpenSSL::rwlock = NULL;

void ZeroCopyOpenSSL::lockingCallback(
        int mode,
        int index,
        const char * file,
        int line)
{

    if (mode & CRYPTO_LOCK) {

        LOG(TRACE)
            << "Locking from "
            << file
            << ":"
            << line
        ;

        if (mode & CRYPTO_WRITE) {
            uv_rwlock_wrlock(&rwlock[index]);
        } else {
            uv_rwlock_rdlock(&rwlock[index]);
        }

    } else {

        LOG(TRACE)
            << "Unlocking from "
            << file
            << ":"
            << line
        ;

        if (mode & CRYPTO_WRITE) {
            uv_rwlock_wrunlock(&rwlock[index]);
        } else {
            uv_rwlock_rdunlock(&rwlock[index]);
        }

    }
}
#endif

int ZeroCopyOpenSSL::initOpenSSL(bool forMultipleThreads) {

    LOG(INFO);
#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
    assert(!rwlock);

    if (rwlock) {

        LOG(ERROR)
            << "OpenSSL was already initialized"
        ;

        return UV_EEXIST;
    }
    SSL_library_init();
    SSL_load_error_strings();
#else
   OPENSSL_init_ssl(0, NULL);
#endif
    ERR_load_SSL_strings();
#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
    OpenSSL_add_all_algorithms();
    if (forMultipleThreads) {

        rwlock = new (std::nothrow) uv_rwlock_t[CRYPTO_num_locks()];

        if (!rwlock) {

            LOG(ERROR)
                << "Unable to allocate rwlock's for OpenSSL"
            ;

            return UV_ENOMEM;
        }

        for (ssize_t i=0; i < CRYPTO_num_locks(); ++i) {
            uv_rwlock_init(&rwlock[i]);
        }

        CRYPTO_set_locking_callback(lockingCallback);

    }
#endif
    return 0;
}

#include <openssl/conf.h>
#include <openssl/engine.h>
void ZeroCopyOpenSSL::finiOpenSSL() {

    LOG(INFO);

#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
    if (rwlock) {

        CRYPTO_set_locking_callback(NULL);

        for (ssize_t i=0; i<CRYPTO_num_locks(); ++i) {
            uv_rwlock_destroy(&rwlock[i]);
        }

        delete [] rwlock;
    }

    CONF_modules_free();
#endif
#if (OPENSSL_VERSION_NUMBER > 0x10000000L && OPENSSL_VERSION_NUMBER < 0x10100000L)
    ERR_remove_thread_state(NULL);
#endif
#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
    ENGINE_cleanup();
#endif
    CONF_modules_unload(1);
#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
    ERR_free_strings();
    EVP_cleanup();
    CRYPTO_cleanup_all_ex_data();
#endif
}

ZeroCopyOpenSSL::ZeroCopyOpenSSL(ZeroCopyOpenSSL::Ctx * ctx, bool passive)
    :
        bioInternal_(BIO_new(BIO_s_bio())),
        bioExternal_(BIO_new(BIO_s_bio())),
        bioSSL_(BIO_new(BIO_f_ssl())),
        lastOutBuf_(NULL),
        ssl_(NULL),
        ready_(false)
    {

                           bioInternal_                       &&
    BIO_set_write_buf_size(bioInternal_, 24576)               &&

                           bioExternal_                       &&
    BIO_set_write_buf_size(bioExternal_, 24576)               &&

    BIO_make_bio_pair     (bioInternal_, bioExternal_)        &&

                           bioSSL_                            &&

    (ssl_               = SSL_new(ctx->getSslCtx()))          &&

    BIO_set_ssl           (bioSSL_, ssl_, BIO_CLOSE)          &&

    (ready_             = true);

    if (!ready_) {
        LOG(ERROR)
            << "Fatal failure: "
            << ZeroCopyOpenSSL::dumpOpenSslErrorStackAsString()
        ;
        this->~ZeroCopyOpenSSL();
        return;
    }

    SSL_set_bio           (ssl_, bioInternal_, bioInternal_);

    SSL_set_mode(ssl_, SSL_MODE_AUTO_RETRY);
    SSL_set_mode(ssl_, SSL_MODE_ENABLE_PARTIAL_WRITE);
    SSL_set_mode(ssl_, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
#ifdef SSL_MODE_RELEASE_BUFFERS
    /* save about 34kB per idle SSL session */
    SSL_set_mode(ssl_, SSL_MODE_RELEASE_BUFFERS);
#endif

    if (passive) {

        SSL_set_accept_state(ssl_);

    } else {

        SSL_set_connect_state(ssl_);

    }

    /* This is the best way I found to do nothing visible yet trigger the SSL
     * handshake. Either call would do, at least with the version of OpenSSL
     * I am testing against. But to err on the safe side, I'd call them both.
     * Like I said, they have no other side effect than triggering the handshake.
     */
    BIO_read (bioSSL_, reinterpret_cast< void * >(1), 0);
    BIO_write(bioSSL_, reinterpret_cast< void * >(1), 0);

}

ZeroCopyOpenSSL::~ZeroCopyOpenSSL() {

    if (bioSSL_) {
        BIO_free_all(bioSSL_);
    }

    if (bioExternal_) {
        BIO_free_all(bioExternal_);
    }

}

void ZeroCopyOpenSSL::infoCallback(SSL const *, int where, int ret) {

    VLOG(3)
        << " ret = "
        << ret
        << " where = "
        << reinterpret_cast< void * >(where)
    ;

    switch (where) {
        case SSL_CB_HANDSHAKE_START:
            VLOG(2)
                << " Handshake start!"
            ;
            break;
        case SSL_CB_HANDSHAKE_DONE:
            VLOG(2)
                << " Handshake done!"
            ;
            break;
    }

}

int ZeroCopyOpenSSL::Ctx::pwdCb(
        char *buf,
        int size,
        int rwflag,
        void * zcCtx) {

    if (!zcCtx) {

        assert(0);

        LOG(ERROR)
            << "No zcCtx was provided"
        ;

        return 0;
    }

    std::string const & passphrase =
        static_cast<ZeroCopyOpenSSL::Ctx *>(zcCtx)->passphrase_;

    size_t actualSize = passphrase.size();

    if (actualSize > static_cast<size_t>(size)) {
        LOG(ERROR)
            << "OpenSSL can't accept passphrases longer than "
            << size
            << " bytes, and we have a "
            << actualSize
            << " bytes passphrase"
        ;

        return -1;
    }

    memcpy(buf, passphrase.data(), actualSize);

    return actualSize;
}

std::string const ZeroCopyOpenSSL::dumpOpenSslErrorStackAsString() {

    static const std::string failedBioNew(
            "Failed to create error bio, "
            "hence unable to dump OpenSSL error stack");

    /* TODO: re-use thread-local BIO */
    BIO * bioErr = BIO_new (BIO_s_mem ());

    if (!bioErr) {
        return failedBioNew;
    }

    ERR_print_errors(bioErr);

    char *buffer = NULL;
    size_t buflen = BIO_get_mem_data (bioErr, &buffer);

    std::string const ret(buffer, buflen);
    BIO_free(bioErr);

    return ret;
}

ZeroCopyOpenSSL::Ctx::Ctx(
        SSL_CTX * c,
        char const * passphrase
    )
        :
            sslCtx_(c),
            passphrase_(passphrase?:"")
        {};

ZeroCopyOpenSSL::Ctx::~Ctx(){
    if (!sslCtx_) {
        return;
    }

    SSL_CTX_free(sslCtx_);

}

size_t ZeroCopyOpenSSL::Ctx::addCaFileOrDirectory(
        char const * caFileOrDirectory
    ) {

    size_t failure = 0;
    bool isDir;

    struct stat s;
    if (stat(caFileOrDirectory, &s)) {
        char buf[256];
        if (0 != strerror_r(errno, buf, sizeof(buf))) {
            buf[0] = '\0';
        }
        LOG(ERROR)
            << "Error ["
            << errno
            << "] (\""
            << buf
            << "\") on path \""
            << caFileOrDirectory
            << "\" does not exist"
        ;
        return ++failure;

    }

    if ((s.st_mode & S_IFDIR) && !(s.st_mode & S_IFREG)) {

        isDir = true;

    } else {

        if (!(s.st_mode & S_IFDIR) && (s.st_mode & S_IFREG)) {

            isDir = false;

        } else {

            LOG(ERROR)
                << "Path \""
                << caFileOrDirectory
                << "\" must be either a regular file or a directory"
            ;
            return ++failure;

        }
    }

    if(1 != SSL_CTX_load_verify_locations(
                    sslCtx_,
                    isDir ? NULL : caFileOrDirectory,
                    isDir ? caFileOrDirectory : NULL)) {
        ++failure;

        LOG(ERROR)
            << "SSL_CTX_load_verify_locations() failed to open CA(s) @ \""
            << caFileOrDirectory
            << "\": "
            << ZeroCopyOpenSSL::dumpOpenSslErrorStackAsString()
        ;
    }

    return failure;

}

size_t ZeroCopyOpenSSL::Ctx::addCertificateFile(
        char const * certificateChainFile
    ) {

    size_t failure = 0;

    if (1 != SSL_CTX_use_certificate_chain_file(sslCtx_, certificateChainFile)) {
        ++failure;

        LOG(ERROR)
            << "SSL_CTX_use_certificate_chain_file() failed to open certificate @ \""
            << certificateChainFile
            << "\": "
            << ZeroCopyOpenSSL::dumpOpenSslErrorStackAsString()
        ;
    }

    return failure;

}

size_t ZeroCopyOpenSSL::Ctx::addPrivateKeyFile(
        char const * privateKeyFilePath
    ) {

    size_t failure = 0;

    if (1 != SSL_CTX_use_PrivateKey_file(sslCtx_, privateKeyFilePath, SSL_FILETYPE_PEM)) {

        ++failure;

        LOG(ERROR)
            << " SSL_CTX_use_PrivateKey_file() failed to open private key @ \""
            << privateKeyFilePath
            << "\": "
            << ZeroCopyOpenSSL::dumpOpenSslErrorStackAsString()
        ;

    }

    return failure;

}


ZeroCopyOpenSSL::Ctx * ZeroCopyOpenSSL::Ctx::createCtx(
        char const * caFileOrDirectory,
        char const * keyAndCertFilePath,
        char const * passphrase
   ) {

#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
    SSL_CTX * sslCtx = SSL_CTX_new(SSLv23_method());
#else
    SSL_CTX * sslCtx = SSL_CTX_new(TLS_method());
#endif

    if (!sslCtx) {
        IF_SSL_ERROR(sslErr) {
            LOG(ERROR)
                << "Failed to create sslCtx: "
                << sslErr
            ;
        }
        return NULL;
    }

    SSL_CTX_set_options(sslCtx, SSL_OP_NO_TLSv1 | SSL_OP_NO_SSLv3 | SSL_OP_NO_SSLv2);

    size_t failure = 0;

    Ctx * ctx = new (std::nothrow) Ctx(sslCtx, passphrase);

    if (!ctx) {

        ++failure;
        LOG(ERROR)
            << "Failed to create ZeroCopyOpenSSL::Ctx"
        ;

        SSL_CTX_free(sslCtx);

    } else {

        /* This needs to be done before invoking SSL_CTX_use_PrivateKey_file()
         * and irrespectively of whether a passphrase was provided or not.
         * Otherwise if the certificate has an encrypted private key and
         * createCtx() was invoked without a passphrase, OpenSSL would always
         * resort to asking for the passphrase from the standand input, hence
         * blocking forever the progress of the current thread.
         */

        SSL_CTX_set_default_passwd_cb(sslCtx, pwdCb);
        SSL_CTX_set_default_passwd_cb_userdata(sslCtx, ctx); /* Important! */

        if (caFileOrDirectory) {
            failure += ctx->addCaFileOrDirectory(caFileOrDirectory);
        }

        if (keyAndCertFilePath) {
            failure += ctx-> addPrivateKeyFile(keyAndCertFilePath);
            failure += ctx->addCertificateFile(keyAndCertFilePath);
        }

        if (failure) {

            delete ctx;
            ctx = NULL;

        } else {

            SSL_CTX_set_info_callback(sslCtx, infoCallback);

            /* just ask for a certificate from the peer anyway */
            (void) SSL_CTX_set_verify(
                    sslCtx,
                    SSL_VERIFY_PEER,
                    NULL);

            /* GREAT SUCCESS! */
            return ctx;

        }

    }

    assert(!ctx);
    /* FALL-THROUGH */

    LOG(ERROR)
        << "Encountered "
        << failure
        << " failure(s)"
    ;

    return NULL;
}

void ZeroCopyOpenSSL::Ctx::setVerify(
        int (*verify_callback)(int, X509_STORE_CTX *)) {

    (void) SSL_CTX_set_verify(
            sslCtx_,
            SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
            verify_callback);

}

void ZeroCopyOpenSSL::Ctx::setNoVerify(
        int (*verify_callback)(int, X509_STORE_CTX *)) {

    (void) SSL_CTX_set_verify(
            sslCtx_,
            SSL_VERIFY_NONE,
            verify_callback);

}

bool ZeroCopyOpenSSL::attachTransport(
        yajr::Peer * p,
        ZeroCopyOpenSSL::Ctx * ctx,
        bool inverted_roles) {

    if (!ctx) {
        return false;
    }

    CommunicationPeer * peer = dynamic_cast<CommunicationPeer *>(p);

    ZeroCopyOpenSSL * const e = new (std::nothrow)
        ZeroCopyOpenSSL(ctx, peer->passive_ ^ inverted_roles);

    if (!e) {
        return false;
    }

    if (!e->ready_) {
        delete e;
        return false;
    }

    new (peer->detachTransport()) TransportEngine< ZeroCopyOpenSSL >(e);

    if (!peer->choked_) {

        /* need to swap in the callback functions */
        peer->choke();
        peer->unchoke();

    }

    return true;
}

} /* yajr::transport namespace */
} /* yajr namespace */

