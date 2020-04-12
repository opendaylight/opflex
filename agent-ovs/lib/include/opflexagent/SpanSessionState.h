/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for Span Listener
 *
 * Copyright (c) 2019 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef OPFLEX_SPANSESSIONSTATE_H
#define OPFLEX_SPANSESSIONSTATE_H

#include <opflex/modb/URI.h>
#include <modelgbp/gbp/DirectionEnumT.hpp>
#include <modelgbp/platform/AdminStateEnumT.hpp>
#include <modelgbp/span/ErspanVersionEnumT.hpp>
#include <boost/asio.hpp>

namespace  std {

    /**
     * template for hash function for address::ip.
     */
    template<>
    struct hash<boost::asio::ip::address> {
        /**
         * hash ip::address
         * @param i ip::address
         * @return hash
         */
        size_t operator()(const boost::asio::ip::address &i) const {
            return hash<string>()(i.to_string());
        }
    };


}

namespace opflexagent {

using namespace std;
using namespace boost::asio::ip;
using namespace opflex::modb;

/**
 * class to represent a source end point
 */
class SourceEndpoint {
    public:
        /**
         * constructor takes a name and port
         * @param[in] name_ name of localEp
         * @param[in] port_ source port name on the vswitch
         * @param[in] dir_ direction to be set
         */
        SourceEndpoint(const string& name_, const string& port_, const unsigned char dir_) :
            name(name_), port(port_), dir(dir_) {};

        /**
         * Copy constructor
         */
        SourceEndpoint(const SourceEndpoint& copy) : name(copy.name), port(copy.port), dir(copy.dir) {}

        /**
         * gets the name of the source end point
         * @return name of source end point
         */
        const string& getName() const { return name; };
        /**
         * gets the port name of the source end point
         * @return name of port source end point
         */
        const string& getPort() const { return port; };
        /**
         * gets the direction of spanned traffic
         * @return a value from DirectionEnumT.
         */
        const unsigned char getDirection() const { return dir; };

    private:
        string name;
        string port;
        unsigned char dir;
};

/**
 * class to represent a span session.
 */
class SessionState {
    public:
        /**
         * comparison criteria for adding to srcEndPoint set
         */
        struct SrcCompare {
        public:
            /**
             * compare the port strings of the source end points
             * @param src1 shared pointer to source end point 1
             * @param src2 shared pointer to source end point 2
             * @return bool true if comparison passes.
             */
            bool operator()(const SourceEndpoint& src1,
                            const SourceEndpoint& src2) const {
                return src1.getPort() == src2.getPort();
            }
        };
        /**
         * hash function fpr srcEndPoint set
         */
        struct SrcHash {
        public:
            /**
             * hashes the concatenation of port and direction
             * @param src shared pointer to source end point
             * @return size_t
             */
            size_t operator()(const SourceEndpoint& src) const {
                string strHash(src.getPort());
                strHash += src.getDirection();
                return std::hash<string>()(strHash);
            }
        };

        /**
         * typedef for source end point set
         */
        typedef unordered_set<SourceEndpoint, SrcHash, SrcCompare> srcEpSet;

        /**
         * constructor that takes a URI that points to a Session object
         * @param uri_ URI to a Session object
         * @param name_ name of Session object
         */
        SessionState(const URI& uri_, const string& name_) :
            uri(uri_), name(name_),
            adminState(modelgbp::platform::AdminStateEnumT::CONST_OFF),
            version(modelgbp::span::ErspanVersionEnumT::CONST_V2) {};

        /**
         * gets the URI, which points to a Session object
         * @return a URI
         */
        const URI& getUri() const { return uri;};

        /**
         * add a destination end point to the internal map
         * @param uri uri pointing to the DstSummary object
         * @param dst dst IP
         */
        void addDstEndpoint(const URI& uri, const address& dst);

        /**
         * add a source end point to the internal map
         * @param srcEp shared pointer to a SourceEndpoint object.
         */
        void addSrcEndpoint(const SourceEndpoint& srcEp);

        /**
         * are there any src endpoints
         * @return has src endpoints
         */
        bool hasSrcEndpoints();

        /**
         * get a copy of the source end points
         * @param ep reference to end point set
         */
        void getSrcEndpointSet(srcEpSet& ep);

        /**
         * are there any destination endpoints
         * @return has dest endpoints
         */
        bool hasDstEndpoints();

        /**
         * get a copy of destination end point map
         * @param dMap a reference to a map of URI, dst IP
         */
        void getDstEndpointMap(unordered_map<URI, address>& dMap);

        /**
         * gets the name string for this object
         * @return the name attribute string.
         */
        const string& getName() { return name; };

        /**
         * get session admin state
         * @returns admin state of session
         */
        const uint8_t getAdminState() { return adminState;};

        /**
         * set session admin state
         * @param ast admin state of session
         */
        void setAdminState(const uint8_t ast) { adminState = ast;};
        /**
        * get ERSPAN version setting
        * @return ERSPAN version
        */
       const uint8_t getVersion() { return version; };
       /**
        * set ERSPAN version
        * @param ver ERSPAN version
        */
       void setVersion(uint8_t ver) { version = ver;};

    private:
        URI uri;
        string name;
        uint8_t adminState;
        uint8_t version;

        srcEpSet srcEndpoints;
        // mapping DstSummary to dst IP
        unordered_map<URI, address> dstEndpoints;
};
}

#endif // SPANSESSIONSTATE_H
