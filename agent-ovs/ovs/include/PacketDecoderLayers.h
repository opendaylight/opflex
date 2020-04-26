/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for PacketDecoderLayers
 *
 * Copyright (c) 2019 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */
#ifndef __PACKETDECODERLAYERS_H__
#define __PACKETDECODERLAYERS_H__

#include "PacketDecoder.h"

namespace opflexagent {
/**
 * Layer implementing Ethernet
 */
class EthernetLayer: public PacketDecoderLayer {
public:
    EthernetLayer():PacketDecoderLayer("Datalink", 25944, "Ethernet", 14, "EProto", "none", 1, 1, 2, 0, 0, 3){};
    virtual ~EthernetLayer() {};
    virtual int configure();
    virtual void getFormatString(boost::format &fmtStr);
};

/**
 * Layer implementing Qtag
 */
class QtagLayer: public PacketDecoderLayer {
public:
    QtagLayer():PacketDecoderLayer("EProto", 33024, "Qtag", 4, "EProto", "none", 2, 2, 2, 0, 0, 1){};
    virtual ~QtagLayer() {};
    virtual int configure();
    virtual void getFormatString(boost::format &fmtStr);
};

/**
 * Layer implementing IPv4
 */
class IPv4Layer: public PacketDecoderLayer {
public:
    IPv4Layer():PacketDecoderLayer("EProto", 2048, "IPv4", 20, "IPProto", "IPOpt", 2, 4, 4, 3, 3, 9){};
    virtual ~IPv4Layer() {};
    virtual int configure();
    virtual void getOptionLength(ParseInfo &p);
    virtual void getFormatString(boost::format &fmtStr);
};

/**
 * Layer implementing Geneve
 */
class GeneveLayer: public PacketDecoderLayer {
public:
    GeneveLayer():PacketDecoderLayer("IPProto", 6081, "Geneve", 8, "Datalink", "GeneveOpt", 4, 6, 1, 5, 5, 0){};
    /** Destructor */
    virtual ~GeneveLayer() {};
    virtual int configure();
    virtual void getOptionLength(ParseInfo &p);
    virtual void getFormatString(boost::format &fmtStr);
};

/**
 * Layer implementing GeneveOpt
 */
class GeneveOptLayer: public PacketDecoderLayer {
public:
    GeneveOptLayer():PacketDecoderLayer("GeneveOpt", 0, "GeneveOpt", 0, "none", "none", 5, 5, 1, 0, 0, 0){};
    virtual ~GeneveOptLayer() {};
    virtual int configure();
    virtual uint32_t getVariableDataLength(uint32_t hdrLength);
    virtual uint32_t getVariableHeaderLength(uint32_t fldVal);
    virtual void getFormatString(boost::format &fmtStr);
    virtual std::shared_ptr<PacketDecoderLayerVariant>
            getVariant(ParseInfo &p);
};

/**
 * Variant implementing TableId
 */
class GeneveOptTableIdLayerVariant: public PacketDecoderLayerVariant {
public:
    GeneveOptTableIdLayerVariant():PacketDecoderLayerVariant("GeneveOpt", "TableId", 5, 1){};
    virtual ~GeneveOptTableIdLayerVariant() {};
    virtual int configure();
    virtual void getFormatString(boost::format &fmtStr);
    virtual void reParse(ParseInfo &p);
};

/**
 * Layer implementing ARP
 */
class ARPLayer: public PacketDecoderLayer {
public:
    ARPLayer():PacketDecoderLayer("EProto", 2054, "ARP", 28, "none", "none", 2, 7, 1, 0, 0, 3){};
    virtual ~ARPLayer() {};
    virtual int configure();
    virtual void getFormatString(boost::format &fmtStr);
};

/**
 * Layer implementing ICMP
 */
class ICMPLayer: public PacketDecoderLayer {
public:
    ICMPLayer():PacketDecoderLayer("IPProto", 1, "ICMP", 8, "none", "none", 4, 8, 1, 0, 0, 4){};
    virtual ~ICMPLayer() {};
    virtual int configure();
    virtual void getFormatString(boost::format &fmtStr);
};

/**
 * Layer implementing TCP
 */
class TCPLayer: public PacketDecoderLayer {
public:
    TCPLayer():PacketDecoderLayer("IPProto", 6, "TCP", 20, "TCPOpt", "TCPOpt", 4, 10, 7, 6, 9, 8){};
    virtual ~TCPLayer() {};
    virtual int configure();
    virtual void getOptionLength(ParseInfo &p);
    virtual void getFormatString(boost::format &fmtStr);
};

/**
 * Layer implementing TCPOpt
 */
class TCPOptLayer: public PacketDecoderLayer {
public:
    TCPOptLayer():PacketDecoderLayer("TCPOpt", 0, "TCPOpt", 0, "none", "none", 6, 9, 7, 0, 0, 0){};
    virtual ~TCPOptLayer() {};
    virtual int configure();
    virtual uint32_t getVariableDataLength(uint32_t hdrLength);
    virtual uint32_t getVariableHeaderLength(uint32_t fldVal);
    virtual bool hasOptBytes(ParseInfo &p);
    virtual void getFormatString(boost::format &fmtStr);
};

/**
 * Layer implementing UDP
 */
class UDPLayer: public PacketDecoderLayer {
public:
    UDPLayer():PacketDecoderLayer("IPProto", 17, "UDP", 8, "none", "none", 4, 11, 7, 0, 0, 3){};
    virtual ~UDPLayer() {};
    virtual int configure();
    virtual void getFormatString(boost::format &fmtStr);
};

/**
 * Layer implementing IPv6
 */
class IPv6Layer: public PacketDecoderLayer {
public:
    IPv6Layer():PacketDecoderLayer("EProto", 34525, "IPv6", 40, "IPProto", "none", 2, 12, 4, 0, 0, 7){};
    virtual ~IPv6Layer() {};
    virtual int configure();
    virtual void getFormatString(boost::format &fmtStr);
};

}
#endif /*__PACKETDECODERLAYERS_H__*/
