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
     * filter criteria for sourcing traffic
     */
    typedef struct {
        /**
         * name of filter
         */
        string name;
        /**
         * ip address to be filtered
         */
        address ip;
        /**
         * start of port range to be filtered
         */
        uint16_t portStart;
        /**
         * end pf port range to be filtered
         */
        uint16_t portEnd;
    } filter_t;

    /**
     * hash function for filter struct to use in sets and maps
     */
    struct filter_t_hash {
        /**
         * calculate hash of filter
         * @param k a filter_t struct
         * @return hash
         */
        size_t operator()(const filter_t& k) const {
            size_t v = 0;
            boost::hash_combine(v, k.portEnd);
            boost::hash_combine(v, k.portStart);
            boost::hash_combine(v, k.name);
            boost::hash_combine(v, hash<address>()(k.ip));
            return v;
        }
    };

    /**
     * class to represent a destination end point
     */
    class DstEndPoint {
    public:
        /**
         * constructor that accepts destination IP address.
         * @param ip ip address of destination end point
         */
        DstEndPoint(address ip) : dstIp(ip) {};
        /**
         * get the name of this destination end point
         * @return string name of destination end point
         */
        const string& getName() const { return name; };
        /**
         * set name of this destination point
         * @param[in] name_ name of the destination end point
         */
        void setName(const string& name_)  { name = name_;};
        /**
         * get the address of this destination end point
         * @return address address of this destination end point.
         */
        const address& getAddress() const { return dstIp; };
    private:
        string name;
        address dstIp;
    };

    /**
     * class to represent a source end point
     */
    class SourceEndPoint {
    public:
        /**
         * constructor takes a name and port
         * @param[in] name name of localEp
         * @param[in] port source port name on the vswitch
         */
        SourceEndPoint(const string& name, const string& port);
        /**
         * gets the name of the source end point
         * @return name of source end point
         */
        const string& getName() const { return name; };
        /**
         * gets the IP address of the source end point
         * @return IP address of source end point
         */
        const address &getSrcEndPoint() const { return srcEndPoint; };
        /**
         * set srcEndPoint address
         * @param[in] address_ IP address of the source end point
         */
        void setSrcEndPoint(const address& address_) { srcEndPoint = address_;};
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
        /**
         * set direction to one of gbp::DirectionEnumT values
         * @param[in] dir_ direction to be set
         */
        void setDirection(const unsigned char dir_) { dir = dir_;};
        /**
         * gets the map of filters keyed by filter name
         * @return a reference to a map of name to filters.
         */
        const unordered_map<string, filter_t>& getFilterSet() const {
            return filters;
        }

    private:
        string name;
        address srcEndPoint;
        string port;
        unsigned char dir;
        unordered_map<string, filter_t> filters;

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
            bool operator()(std::shared_ptr<SourceEndPoint> src1,
                    std::shared_ptr<SourceEndPoint> src2) const {
                if (src1->getPort().compare(src2->getPort()) == 0)
                    return true;
                else
                    return false;
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
         * constructor that takes a SessionState as reference.
         * @param s reference to a SessionState object
         */
        SessionState(const SessionState &s);
        /**
         * constructor that takes a URI that points to a Session object
         * @param uri URI to a Session object
         * @param name name of Session object
         */
        SessionState(const URI& uri, const string& name);
        /**
         * gets the URI, which points to a Session object
         * @return a URI
         */
        const URI& getUri() const { return uri;};
        /**
         * set the URI to the one passed in
         * @param uri_ URI to a Session object
         */
        void setUri(URI &uri_) { uri = uri_;};

        /**
         * add a destination end point to the internal map
         * @param uri uri pointing to the DstSummary object
         * @param dEp shared pointer to a DstEndPoint object.
         */
        void addDstEndPoint(const URI& uri, shared_ptr<DstEndPoint> dEp);

        /**
         * add a source end point to the internal map
         * @param srcEp shared pointer to a SourceEndPoint object.
         */
        void addSrcEndPoint(shared_ptr<SourceEndPoint> srcEp);

        /**
         * get the source end point set reference
         * @return a reference to the source end point set
         */
        const srcEpSet& getSrcEndPointSet();

        /**
         * gets the destination end point map reference
         * @return a reference to the destination end point map.
         */
        const unordered_map<URI, shared_ptr<DstEndPoint>>&
             getDstEndPointMap() const;

        /**
         * gets the name string for this object
         * @return the name attribute string.
         */
        const string& getName() { return name; };

    private:
        URI uri;
        string name;
        // mapping LocalEp to SourceEndPoint
        // unordered_map<URI, shared_ptr<SourceEndPoint>> srcEndPoints;

        srcEpSet srcEndPoints;
        // mapping DstSummary to DstEndPoint
        unordered_map<URI, shared_ptr<DstEndPoint>> dstEndPoints;
        unordered_map<string, filter_t> filters;
    };
}

#endif // SPANSESSIONSTATE_H
