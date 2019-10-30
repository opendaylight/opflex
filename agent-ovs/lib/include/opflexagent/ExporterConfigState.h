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
#ifndef OPFLEX_EXPORTERCONFIGSTATE
#define OPFLEX_EXPORTERCONFIGSTATE

#include <opflex/modb/URI.h>
#include <boost/asio.hpp>
namespace opflexagent
{

using namespace std;
using namespace boost::asio::ip;
using namespace opflex::modb;
/*typedef struct {
       
        string name;
      
        address ip;
       
        uint16_t portStart;
      
        uint16_t portEnd;
    } filter_t;*/

class ExporterConfigState
{

private:
    address dstAddr;
    address srcAddr;
    uint32_t activeFlowTimeOut;
    uint32_t idleFlowTimeOut;
    uint32_t samplingRate;
    URI uri;
    string name;
    string dstPort;
public:
     /**
         * get the name of this ExporterConfig
         * @return string name of ExporterConfig
         */
        const string& getName() const { return name; };
         /**
         * get the address of the destination of ExporterConfig
         * @return address  of the destination ExporterConfig
         */
         
         const address& getdstAddress() const { return dstAddr; };

          /**
         * get the address of the source of ExporterConfig
         * @return address  of the source ExporterConfig
         */
         
         const address& getsrcAddress() const { return srcAddr; };
          /**
         * gets the URI, which points to a  ExporterConfig object
         * @return a URI
         */
        const URI& getUri() const { return uri;};
        /**
         * set the URI to the one passed in
         * @param uri_ URI to a  ExporterConfig object
         */
        void setUri(URI &uri_) { uri = uri_;};

};
}
#endif //OPFLEX_EXPORTERCONFIGSTATE