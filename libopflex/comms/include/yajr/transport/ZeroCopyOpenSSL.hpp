/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef _____COMMS__INCLUDE__YAJR__TRANSPORT__ZEROCOPYOPENSSL_H
#define _____COMMS__INCLUDE__YAJR__TRANSPORT__ZEROCOPYOPENSSL_H

#include <yajr/transport/engine.hpp>

#include <openssl/bio.h>

#include <openssl/ssl.h>

#include <deque>
#include <string>

namespace yajr {

    class Peer;

namespace transport {

/**
 * @brief A secure transport layer based on OpenSSL and its zero-copy APIs,
 * BIO_nread() and BIO_nwrite()
 */
struct ZeroCopyOpenSSL : public Transport::Engine {
  public:

    class Ctx;

    /**
     * @brief Initializes OpenSSL
     *
     * @return An error code is returned on failure, or 0 on success.
     */
    static int  initOpenSSL(
            bool forMultipleThreads
             /**< [in] whether the transport will be used by multiple threads */
    );

    /**
     * @brief Finalizes OpenSSL
     */
    static void finiOpenSSL();

    static bool attachTransport(
            yajr::Peer * p,
            /**< [in] the peer to attach the transport to */
            ZeroCopyOpenSSL::Ctx * ctx,
            /**< [in] the context for this peer, as created by the static
             *        method Ctx::createCtx(). Can be shared across multiple
             *        peers.
             */
            bool inverted_roles = false
            /**< [in] whether to have the server connect and the client accept,
             *        so as to save CPU on the server at the price of losing
             *        interoperability. Don't use it unless you control both
             *        clients and servers and want to push the heavy asymmetric
             *        crypto operations from the servers to the clients.
             */
    );

    ~ZeroCopyOpenSSL();
    /* will restrict access to the following later */
    BIO * bioInternal_;
    BIO * bioExternal_;
    BIO * bioSSL_;
    char * lastOutBuf_;
    static std::string const dumpOpenSslErrorStackAsString();
  private:
    SSL* ssl_;
    bool ready_;
    static uv_rwlock_t * rwlock;
    static void lockingCallback(int, int, const char *, int);
    static void infoCallback(SSL const *, int, int);
    ZeroCopyOpenSSL(ZeroCopyOpenSSL::Ctx * ctx, bool passive);
};

class ZeroCopyOpenSSL::Ctx {
  public:
    /**
     * @brief Creates a secure transport context
     *
     * @return the context that has been created, or NULL on failure.
     */
    static Ctx * createCtx(
        char const * caFileOrDirectory = NULL,
        /**< [in] either the path to a single PEM file for the trusted root CA
         *        certificate or the path to a directory containing certificates
         *        for multiple trusted root CA's. Can be NULL for SSL servers.
         */
        char const * keyAndCertFilePath = NULL,
        /**< [in] the path to the PEM file for this peer, containing its
         *        certificate and its private key, possibly encrypted.
         *        Can be NULL for SSL clients, must be present for servers.
         */
        char const * passphrase = NULL
        /**< [in] the passphrase to be used to decrypt the private key within
         *        this peer's PEM file
         */
    );

    /**
     * @brief Adds a key file to a secure transport context
     *
     * @return 0 on success, or the number of errors upon failure
     */
    size_t addPrivateKeyFile(
        char const * keyFilePath
        /**< [in] the path to the PEM file for this peer's private key
         */
    );

    /**
     * @brief Adds a key file to a secure transport context
     *
     * @return 0 on success, or the number of errors upon failure
     */
    size_t addCertificateFile(
        char const * certFilePath
        /**< [in] the path to the PEM file for this peer's certificate
         */
    );


    /**
     * @brief Adds trusted root certificates to a secure transport context
     *
     * @return 0 on success, or the number of errors upon failure
     */
    size_t addCaFileOrDirectory(
        char const * caFileOrDirectory
        /**< [in] either the path to a single PEM file for the trusted root CA
         *        certificate or the path to a directory containing certificates
         *        for multiple trusted root CA's. Can be NULL for SSL servers.
         */
    );

    /**
     * @brief Require successful verification of peer's certificate.
     */
    void setVerify(
        int (*verify_callback)(int, X509_STORE_CTX *) = NULL
        /**< [in] a verification callback to verify the other peer's certificate
         */
    );

    /**
     * @brief Do not require successful verification of peer's certificate.
     */
    void setNoVerify(
        int (*verify_callback)(int, X509_STORE_CTX *) = NULL
        /**< [in] a verification callback to verify the other peer's certificate
         */
    );


    SSL_CTX * getSslCtx() const {
        return sslCtx_;
    }
    ~Ctx();
  private:
    Ctx(SSL_CTX * c, char const * passphrase);
    static int pwdCb(char *, int, int, void *);
    SSL_CTX * sslCtx_;
    std::string passphrase_;
};

} /* yajr::transport namespace */
} /* yajr namespace */

#endif /* _____COMMS__INCLUDE__YAJR__TRANSPORT__ZEROCOPYOPENSSL_H */

