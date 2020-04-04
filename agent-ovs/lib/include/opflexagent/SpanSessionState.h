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
 * class to represent a destination end point
 */
class DstEndPoint {
    public:
        /**
         * constructor that accepts destination IP address.
         * @param ip ip address of destination end point
         */
        DstEndPoint(const address& ip) : dstIp(ip) {};
        /**
         * copy constructor
         * @param dp reference to object being copied from
         */
        DstEndPoint(const DstEndPoint& dp) : dstIp(dp.dstIp) {};
        /**
         * get the address of this destination end point
         * @return address address of this destination end point.
         */
        const address& getAddress() const { return dstIp; };

    private:
        address dstIp;
};

/**
 * class to represent a source end point
 */
class SourceEndPoint {
    public:
        /**
         * constructor takes a name and port
         * @param[in] name_ name of localEp
         * @param[in] port_ source port name on the vswitch
         * @param[in] dir_ direction to be set
         */
        SourceEndPoint(const string& name_, const string& port_, const unsigned char dir_) :
            name(name_), port(port_), dir(dir_) {};
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
            bool operator()(const std::shared_ptr<SourceEndPoint>& src1,
                            const std::shared_ptr<SourceEndPoint>& src2) const {
                return src1->getPort() == src2->getPort();
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
            size_t operator()(shared_ptr<SourceEndPoint> src) const {
                string strHash(src->getPort());
                strHash += src->getDirection();
                return std::hash<string>()(strHash);
            }
        };
        /**
         * typedef for source end point set
         */
        typedef unordered_set<shared_ptr<SourceEndPoint>, SrcHash, SrcCompare>
            srcEpSet;
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
         * @param dEp shared pointer to a DstEndPoint object.
         */
        void addDstEndPoint(const URI& uri, const shared_ptr<DstEndPoint>& dEp);

        /**
         * add a source end point to the internal map
         * @param srcEp shared pointer to a SourceEndPoint object.
         */
        void addSrcEndPoint(const shared_ptr<SourceEndPoint>& srcEp);

        /**
         * get the source end point set reference
         * @return a reference to the source end point set
         */
        const srcEpSet& getSrcEndPointSet();

        /**
         * get a copy of the source end points
         * @param ep reference to end point set
         */
        const void getSrcEndPointSet(srcEpSet& ep);

        /**
         * gets the destination end point map reference
         * @return a reference to the destination end point map.
         */
        const unordered_map<URI, shared_ptr<DstEndPoint>>&
             getDstEndPointMap();

        /**
         * get a copy of destination end point map
         * @param dMap a reference to a map of URI,DstEndPoint
         */
        const void getDstEndPointMap(unordered_map<URI, shared_ptr<DstEndPoint>>& dMap);
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
       /**
        * overloading output stream operator
        * @param out output stream reference
        * @param seSt SessionState reference
        * @return output stream.
        */
       friend ostream& operator<< (ostream& out, const SessionState& seSt);

    private:
        URI uri;
        string name;
        uint8_t adminState;
        uint8_t version;

        srcEpSet srcEndPoints;
        // mapping DstSummary to DstEndPoint
        unordered_map<URI, shared_ptr<DstEndPoint>> dstEndPoints;
};
}

#endif // SPANSESSIONSTATE_H
