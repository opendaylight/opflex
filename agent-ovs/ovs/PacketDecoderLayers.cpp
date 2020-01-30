/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation file for PacketDecoderLayers
 *
 * Copyright (c) 2019 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */
#include "PacketDecoderLayers.h"

namespace opflexagent {

typedef PacketDecoderLayerFieldType PDF;

int EthernetLayer::configure() {
    addField("dstMac", 48, 0, PDF::FLDTYPE_MAC, 0, 0, -1, 1, 3);
    addField("srcMac", 48, 48, PDF::FLDTYPE_MAC, 0, 0, -1, 2, 2);
    addField("eType", 16, 96, PDF::FLDTYPE_BYTES, 1, 0, -1, 3, 4);
    return 0;
}

void EthernetLayer::getFormatString(boost::format &fmtStr) {
    //Format string to print the layer goes here
    fmtStr = boost::format(" MAC=%1%:%2%:%3%");
}

int QtagLayer::configure() {
    addField("pcp", 3, 0, PDF::FLDTYPE_BITFIELD, 0, 0, -1, 0, 0);
    addField("dei", 1, 3, PDF::FLDTYPE_BITFIELD, 0, 0, -1, 0, 0);
    addField("vid", 12, 4, PDF::FLDTYPE_BITFIELD, 0, 0, -1, 1, 0);
    addField("eType", 16, 16, PDF::FLDTYPE_BYTES, 1, 0, -1, 0, 4);
    return 0;
}

void QtagLayer::getFormatString(boost::format &fmtStr) {
    //Format string to print the layer goes here
    fmtStr = boost::format(" QTAG=%1%");
}

int IPv4Layer::configure() {
    addField("version", 4, 0, PDF::FLDTYPE_BITFIELD, 0, 0, -1, 0, 0);
    addField("headerLength", 4, 4, PDF::FLDTYPE_BITFIELD, 0, 0, 0, 0, 0);
    addField("dscp", 6, 8, PDF::FLDTYPE_BITFIELD, 0, 0, -1, 4, 0);
    addField("ecn", 2, 14, PDF::FLDTYPE_BITFIELD, 0, 0, -1, 0, 0);
    addField("totalLength", 16, 16, PDF::FLDTYPE_BYTES, 0, 0, -1, 3, 0);
    addField("id", 16, 32, PDF::FLDTYPE_BYTES, 0, 0, -1, 6, 0);
    addField("flags", 3, 48, PDF::FLDTYPE_BITFIELD, 0, 0, -1, 7, 0);
    addField("fragOffset", 13, 51, PDF::FLDTYPE_BITFIELD, 0, 0, -1, 8, 0);
    addField("ttl", 8, 64, PDF::FLDTYPE_BYTES, 0, 0, -1, 5, 0);
    addField("ipProto", 8, 72, PDF::FLDTYPE_BYTES, 1, 0, -1, 9, 7);
    addField("checkSum", 16, 80, PDF::FLDTYPE_BYTES, 0, 0, -1, 0, 0);
    addField("srcAddress", 32, 96, PDF::FLDTYPE_IPv4ADDR, 0, 0, -1, 1, 5);
    addField("dstAddress", 32, 128, PDF::FLDTYPE_IPv4ADDR, 0, 0, -1, 2, 6);
    return 0;
}

void IPv4Layer::getOptionLength(ParseInfo &p) {
    p.pendingOptionLength = ((p.scratchpad[0]*4) - byteLength);
}

void IPv4Layer::getFormatString(boost::format &fmtStr) {
    //Format string to print the layer goes here
    fmtStr = boost::format(" SRC=%1% DST=%2% LEN=%3% DSCP=%4% TTL=%5% ID=%6% FLAGS=%7% FRAG=%8% PROTO=%9%");
}

int GeneveLayer::configure() {
    addField("version", 2, 0, PDF::FLDTYPE_BITFIELD, 0, 0, -1, 0, 0);
    addField("optionsLength", 6, 2, PDF::FLDTYPE_BITFIELD, 0, 0, 0, 0, 0);
    addField("oamPacket", 1, 8, PDF::FLDTYPE_BITFIELD, 0, 0, -1, 0, 0);
    addField("criticalOption", 1, 9, PDF::FLDTYPE_BITFIELD, 0, 0, -1, 0, 0);
    addField("rsvd", 6, 10, PDF::FLDTYPE_BITFIELD, 0, 0, -1, 0, 0);
    addField("eType", 16, 16, PDF::FLDTYPE_BYTES, 1, 0, -1, 0, 0);
    addField("vni", 24, 32, PDF::FLDTYPE_BYTES, 0, 0, -1, 1, 0);
    addField("rsvd1", 8, 56, PDF::FLDTYPE_BYTES, 0, 0, -1, 0, 0);
    return 0;
}

void GeneveLayer::getOptionLength(ParseInfo &p) {
    p.pendingOptionLength = p.scratchpad[0]*4;
}

void GeneveLayer::getFormatString(boost::format &fmtStr) {
    //Format string to print the layer goes here
    fmtStr = boost::format(" BR=%1%");
}

int GeneveOptLayer::configure() {
    addField("class", 16, 0, PDF::FLDTYPE_BYTES, 0, 0, -1, 0, 0);
    addField("type", 8, 16, PDF::FLDTYPE_BYTES, 0, 0, -1, 0, 0);
    addField("flags", 3, 24, PDF::FLDTYPE_BITFIELD, 0, 0, -1, 0, 0);
    addField("length", 5, 27, PDF::FLDTYPE_BITFIELD, 0, 1, -1, 0, 0);
    addField("data", 0, 32, PDF::FLDTYPE_VARBYTES, 0, 0, -1, 0, 0);
    return 0;
}

uint32_t GeneveOptLayer::getVariableDataLength(uint32_t hdrLength) {
    if (hdrLength >= 4 ) {
        return (hdrLength -4);
    } else {
        return 0;
    }
}

uint32_t GeneveOptLayer::getVariableHeaderLength(uint32_t fldVal) {
    return (fldVal*4 + 4);
}

void GeneveOptLayer::getFormatString(boost::format &fmtStr) {
    //Format string to print the layer goes here
    fmtStr = boost::format("");
}

int ARPLayer::configure() {
    addField("hwType", 16, 0, PDF::FLDTYPE_BYTES, 0, 0, -1, 0, 0);
    addField("eType", 16, 16, PDF::FLDTYPE_BYTES, 0, 0, -1, 0, 0);
    addField("hwSize", 8, 32, PDF::FLDTYPE_BYTES, 0, 0, -1, 0, 0);
    addField("protSize", 8, 40, PDF::FLDTYPE_BYTES, 0, 0, -1, 0, 0);
    addField("opCode", 16, 48, PDF::FLDTYPE_BYTES, 0, 0, -1, 3, 0);
    addField("senderMac", 48, 64, PDF::FLDTYPE_MAC, 0, 0, -1, 0, 0);
    addField("senderv4", 32, 112, PDF::FLDTYPE_IPv4ADDR, 0, 0, -1, 1, 0);
    addField("tgtMac", 48, 144, PDF::FLDTYPE_MAC, 0, 0, -1, 0, 0);
    addField("tgtv4", 32, 192, PDF::FLDTYPE_IPv4ADDR, 0, 0, -1, 2, 0);
    return 0;
}

void ARPLayer::getFormatString(boost::format &fmtStr) {
    //Format string to print the layer goes here
    fmtStr = boost::format(" ARP_SPA=%1% ARP_TPA=%2% ARP_OP=%3%");
}

int ICMPLayer::configure() {
    addField("type", 8, 0, PDF::FLDTYPE_BYTES, 0, 0, -1, 1, 0);
    addField("code", 8, 8, PDF::FLDTYPE_BYTES, 0, 0, -1, 2, 0);
    addField("checksum", 16, 16, PDF::FLDTYPE_BYTES, 0, 0, -1, 0, 0);
    addField("identifier", 16, 32, PDF::FLDTYPE_BYTES, 0, 0, -1, 3, 0);
    addField("sequenceNum", 16, 48, PDF::FLDTYPE_BYTES, 0, 0, -1, 4, 0);
    return 0;
}

void ICMPLayer::getFormatString(boost::format &fmtStr) {
    //Format string to print the layer goes here
    fmtStr = boost::format(" TYPE=%1% CODE=%2% ID=%3% SEQ=%4%");
}

int TCPLayer::configure() {
    addField("srcPort", 16, 0, PDF::FLDTYPE_BYTES, 0, 0, -1, 1, 8);
    addField("dstPort", 16, 16, PDF::FLDTYPE_BYTES, 0, 0, -1, 2, 9);
    addField("seqNum", 32, 32, PDF::FLDTYPE_BYTES, 0, 0, -1, 3, 0);
    addField("ackNum", 32, 64, PDF::FLDTYPE_BYTES, 0, 0, -1, 4, 0);
    addField("length", 4, 96, PDF::FLDTYPE_BITFIELD, 0, 0, 0, 5, 0);
    addField("rsvd", 3, 100, PDF::FLDTYPE_BITFIELD, 0, 0, -1, 0, 0);
    addField("NS", 1, 103, PDF::FLDTYPE_BITFIELD, 0, 0, -1, 6, 0);
    addField("CWR", 1, 104, PDF::FLDTYPE_BITFIELD, 0, 0, -1, 6, 0);
    addField("ECE", 1, 105, PDF::FLDTYPE_BITFIELD, 0, 0, -1, 6, 0);
    addField("URG", 1, 106, PDF::FLDTYPE_BITFIELD, 0, 0, -1, 6, 0);
    addField("ACK", 1, 107, PDF::FLDTYPE_BITFIELD, 0, 0, -1, 6, 0);
    addField("PSH", 1, 108, PDF::FLDTYPE_BITFIELD, 0, 0, -1, 6, 0);
    addField("RST", 1, 109, PDF::FLDTYPE_BITFIELD, 0, 0, -1, 6, 0);
    addField("SYN", 1, 110, PDF::FLDTYPE_BITFIELD, 0, 0, -1, 6, 0);
    addField("FIN", 1, 111, PDF::FLDTYPE_BITFIELD, 0, 0, -1, 6, 0);
    addField("wdwSize", 16, 112, PDF::FLDTYPE_BYTES, 0, 0, -1, 7, 0);
    addField("ckSum", 16, 128, PDF::FLDTYPE_BYTES, 0, 0, -1, 0, 0);
    addField("urgentPtr", 16, 144, PDF::FLDTYPE_BYTES, 0, 0, -1, 8, 0);
    return 0;
}

void TCPLayer::getOptionLength(ParseInfo &p) {
    p.pendingOptionLength = p.scratchpad[0]*4 - byteLength;
}

void TCPLayer::getFormatString(boost::format &fmtStr) {
    //Format string to print the layer goes here
    fmtStr = boost::format(" SPT=%1% DPT=%2% SEQ=%3% ACK=%4% LEN=%5% WINDOWS=%7% %6% URGP=%8%");
}

int TCPOptLayer::configure() {
    addField("kind", 8, 0, PDF::FLDTYPE_BYTES, 0, 0, 1, 0, 0);
    addField("length", 8, 8, PDF::FLDTYPE_OPTBYTES, 0, 1, -1, 0, 0);
    addField("data", 0, 16, PDF::FLDTYPE_VARBYTES, 0, 0, -1, 0, 0);
    return 0;
}

uint32_t TCPOptLayer::getVariableDataLength(uint32_t hdrLength) {
    return ((hdrLength < 2)? 0:(hdrLength - 2));
}

uint32_t TCPOptLayer::getVariableHeaderLength(uint32_t fldVal) {
    return ((fldVal==0)?1:fldVal);
}

bool TCPOptLayer::hasOptBytes(ParseInfo &p) {
    return ((p.scratchpad[1] != 0) && (p.scratchpad[1] != 1));
}

void TCPOptLayer::getFormatString(boost::format &fmtStr) {
    //Format string to print the layer goes here
    fmtStr = boost::format("");
}

int UDPLayer::configure() {
    addField("srcPort", 16, 0, PDF::FLDTYPE_BYTES, 0, 0, -1, 1, 8);
    addField("dstPort", 16, 16, PDF::FLDTYPE_BYTES, 0, 0, -1, 2, 9);
    addField("length", 16, 32, PDF::FLDTYPE_BYTES, 0, 0, -1, 3, 0);
    addField("ckSum", 16, 48, PDF::FLDTYPE_BYTES, 0, 0, -1, 0, 0);
    return 0;
}

void UDPLayer::getFormatString(boost::format &fmtStr) {
    //Format string to print the layer goes here
    fmtStr = boost::format(" SPT=%1% DPT=%2% LEN=%3%");
}

int IPv6Layer::configure() {
    addField("version", 4, 0, PDF::FLDTYPE_BITFIELD, 0, 0, -1, 0, 0);
    addField("trafficClass", 8, 4, PDF::FLDTYPE_BITFIELD, 0, 0, -1, 4, 0);
    addField("flowLabel", 20, 12, PDF::FLDTYPE_BITFIELD, 0, 0, -1, 6, 0);
    addField("payloadLength", 16, 32, PDF::FLDTYPE_BYTES, 0, 0, -1, 3, 0);
    addField("nextHeader", 8, 48, PDF::FLDTYPE_BYTES, 1, 0, -1, 7, 7);
    addField("hopLimit", 8, 56, PDF::FLDTYPE_BYTES, 0, 0, -1, 5, 0);
    addField("srcAddress", 128, 64, PDF::FLDTYPE_IPv6ADDR, 0, 0, -1, 1, 5);
    addField("dstAddress", 128, 192, PDF::FLDTYPE_IPv6ADDR, 0, 0, -1, 2, 6);
    return 0;
}

void IPv6Layer::getFormatString(boost::format &fmtStr) {
//Format string to print the layer goes here
    fmtStr = boost::format(" SRC=%1% DST=%2% LEN=%3% TC=%4% HL=%5% FL=%6% PROTO=%7%");
}

}
