/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for net flow exporter Listener
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
using namespace opflex::modb;

/**
 * represent netflow exporter config object
 */
class ExporterConfigState
{

private:
    string dstAddr;
    string srcAddr;
    uint32_t activeFlowTimeOut;
    uint32_t idleFlowTimeOut;
    uint32_t samplingRate;
    uint16_t dstPort;
    URI uri;
    string name;
    uint8_t version;
    uint8_t dscp;

public:
    /**
     * constructor that takes a URI that points to a Session object
     * @param uri_ URI to a Session object
     * @param name_ name of Session object
     */
    ExporterConfigState(const URI& uri_, const string& name_) : dstAddr(""),
        srcAddr(""), activeFlowTimeOut(0), idleFlowTimeOut(0), samplingRate(0),
        dstPort(0),uri(uri_), name(name_), version(0), dscp(0)
     { };
    /**
     * get the name of this ExporterConfig
     * @return string name of ExporterConfig
     */
    const string& getName() const { return name; };
    /**
     * get the address of the destination of ExporterConfig
     * @return address  of the destination ExporterConfig
     */
    const string& getDstAddress() const { return dstAddr; };
    /**
     * set the address of the destination of ExporterConfig
     * @param[in] dstAddr_  destination address of ExporterConfig
     */
    void setDstAddress(const string& dstAddr_ )  { dstAddr = dstAddr_; };
    /**
     * get the address of the source of ExporterConfig
     * @return address  of the source ExporterConfig
     */
    const string& getSrcAddress() const { return srcAddr; };
    /**
     * set the address of the source of ExporterConfig
     * @param[in] srcAddr_  source address of the source ExporterConfig
     */
    void setSrcAddress(const string& srcAddr_ ) { srcAddr = srcAddr_; };
    /**
     * get the activeFlowTimeOut of  ExporterConfig
     * @return activeFlowTimeOut  of the  ExporterConfig
     */
    const uint32_t& getActiveFlowTimeOut() const { return activeFlowTimeOut; };
    /**
     * set the  activeFlowTimeOut of ExporterConfig
     * @param[in] activeFlowTimeOut_ activeFlowTimeOut of the ExporterConfig
     */
    void setActiveFlowTimeOut(const uint32_t& activeFlowTimeOut_ ) { activeFlowTimeOut = activeFlowTimeOut_; };
    /**
     * get the idleFlowTimeOut of  ExporterConfig
     * @return idleFlowTimeOut  of the  ExporterConfig
     */
    const uint32_t& getIdleFlowTimeOut() const { return idleFlowTimeOut; };
    /**
     * set the  idleFlowTimeOut of ExporterConfig
     * @param[in] idleFlowTimeOut_ idleFlowTimeOut  of the  ExporterConfig
     */
    void setIdleFlowTimeOut(const uint32_t& idleFlowTimeOut_ ) { idleFlowTimeOut = idleFlowTimeOut_; };
    /**
     * get the samplingRate of  ExporterConfig
     * @return samplingRate  of the  ExporterConfig
     */
    const uint32_t& getSamplingRate() const { return samplingRate; };
    /**
     * set the  samplingRate of ExporterConfig
     * @param[in] samplingRate_ samplingRate  of the  ExporterConfig
     */
    void setSamplingRate(const uint32_t& samplingRate_ ) { samplingRate = samplingRate_; };
    /**
     * get the dstPort of  ExporterConfig
     * @return dstPort  of the  ExporterConfig
     */
    const uint16_t& getDestinationPort() const { return dstPort; };
    /**
     * set the  dstPort of ExporterConfig
     * @param[in] dstPort_ dstPort  of the  ExporterConfig
     */
    void setDestinationPort(const uint16_t& dstPort_ ) { dstPort = dstPort_; };
    /**
     * get the version of  ExporterConfig
     * @return version  of the  ExporterConfig
     */
    const uint8_t& getVersion() const { return version; };
    /**
     * set the  version of ExporterConfig
     * @param[in] version_ version  of the  ExporterConfig
     */
    void setVersion(const uint8_t& version_ ) { version = version_; };
    /**
     * get the version of  ExporterConfig
     * @return version  of the  ExporterConfig
     */
    const uint8_t& getDscp() const { return dscp; };
    /**
     * set the  dscp of ExporterConfig
     * @param[in] dscp_ dscp  of the  ExporterConfig
     */
    void setDscp(const uint8_t& dscp_ ) { dscp = dscp_; };
    /**
     * gets the URI, which points to a  ExporterConfig object
     * @return a URI
     */
    const URI& getUri() const { return uri; };
    /**
     * set the URI to the one passed in
     * @param uri_ URI to a  ExporterConfig object
     */
    void setUri(URI &uri_) { uri = uri_; };
};
} // namespace opflexagent
#endif //OPFLEX_EXPORTERCONFIGSTATE
