/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation file for PacketDecoder
 *
 * Copyright (c) 2019 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */
#include "PacketDecoderLayers.h"
#include <boost/asio/ip/address.hpp>
#include <opflex/modb/MAC.h>
#include <opflexagent/logging.h>
#include <iostream>

namespace opflexagent {

using namespace std;

bool operator== (const PacketTuple &lhs, const PacketTuple &rhs) {
    bool all_fields_match = (lhs.fields.size() == rhs.fields.size());
    if (!all_fields_match)
        return false;
    all_fields_match &= (lhs.fields == rhs.fields);
    return all_fields_match;
}

void PacketDecoderLayerField::transformLog(uint32_t value, stringstream &ostr, ParseInfo &p) {
    if(!shouldLog()){
        return;
    }
    //Convert key types to layer names
    if(getIsNextKey()) {
        string layerName;
        if(p.pktDecoder &&
           p.pktDecoder->getLayerNameByTypeKey(
               p.nextLayerTypeId, p.nextKey, layerName)) {
            ostr << layerName;
        } else {
            ostr << value << "(unrecognized)";
        }
        return;
    }
    //Print fieldnames for bits
    if(bitLength == 1) {
        if(value ==1) {
            ostr << fieldName << " ";
        }
        return;
    }
    //Check for a string representation
    auto it = kvOutMap.find(value);
    if(it != kvOutMap.end()) {
        ostr << it->second;
    } else {
        ostr << value;
    }
}

int PacketDecoderLayerField::decode(const unsigned char *buf, std::size_t length, ParseInfo &p) {
    stringstream ostr;
    int err = 0;
    unsigned char *data_ptr = const_cast<unsigned char *>(buf) + (bitOffset/8);
    switch(fieldType) {
        case FLDTYPE_BITFIELD:
        {
            uint32_t value = 0;
            unsigned char *ptr = (unsigned char *)&value;
            uint32_t valBytes = 0;
            uint32_t remBits = 0;
            if(bitLength > 8-(bitOffset%8)) {
                remBits = (bitLength-8+(bitOffset%8));
                valBytes += (remBits)/8;
                if(remBits%8) {
                    valBytes++;
                }
            } else {
                remBits = 8-bitLength-(bitOffset%8);
            }
            unsigned char maskLow = ((1<<(8-(bitOffset%8)))-1);
            ptr[3-valBytes] = *data_ptr & maskLow;
            for( uint32_t i=1; i < valBytes; i++) {
                data_ptr++;
                ptr[3-valBytes+i] = *data_ptr;
            }
            ptr[3] &= ~((1<<(remBits%8)) -1);
            value = ntohl(value);
            if(remBits) {
                value = value >> remBits;
            }
            transformLog(value, ostr, p);
            if(getIsLength()) {
                p.inferredLength = value;
            }
            int scratchOffset;
            if(shouldSave(scratchOffset)) {
                p.scratchpad[scratchOffset] = value;
            }
            break;
        }
        case FLDTYPE_BYTES:
        case FLDTYPE_OPTBYTES:
        {
            uint8_t byteLen = bitLength/8;
            if(fieldType == FLDTYPE_OPTBYTES) {
                if(p.hasOptBytes == 0) {
                    if(getIsLength()) {
                        p.inferredLength = 0;
                    }
                    break;
                }
            }
            if (byteLen <= 4) {
                uint32_t value = 0;
                unsigned char *ptr = (unsigned char *)&value;
                for(uint8_t i=0; i<byteLen; i++) {
                    ptr[4-byteLen+i] = *data_ptr;
                    data_ptr++;
                }
                value = ntohl(value);
                if(getIsLength()) {
                    p.inferredLength = value;
                }
                if(getIsNextKey()) {
                    p.nextKey = value;
                }
                transformLog(value, ostr, p);
                if(shouldSave(scratchOffset)) {
                    p.scratchpad[scratchOffset] = value;
                }
            } else if(byteLen <= 8) {
                uint64_t value = 0;
                unsigned char *ptr = (unsigned char *)&value;
                memcpy(ptr, data_ptr, byteLen);
                if(shouldLog()) {
                    for (uint8_t i=0; i<byteLen; i++) {
                        ostr << hex << *ptr;
                        ptr++;
                    }
                }
            }
            break;
        }
        case FLDTYPE_IPv4ADDR:
        {
            uint32_t value = 0;
            unsigned char *ptr = (unsigned char *)&value;
            for(int i=0; i<=3; i++) {
                ptr[i] = *data_ptr;
                data_ptr++;
            }
            value = htonl(value);
            boost::asio::ip::address_v4 addr(value);
            if(shouldLog()) {
                ostr << addr;
            }
            break;
        }
        case FLDTYPE_IPv6ADDR:
        {
            boost::asio::ip::address_v6::bytes_type bytes;
            for(int i=0; i<16 ; i++) {
                bytes[i] = *data_ptr;
                data_ptr++;
            }
            boost::asio::ip::address_v6 addr(bytes);
            if(shouldLog()) {
                ostr << addr;
            }
            break;
        }
        case FLDTYPE_MAC:
        {   uint8_t mac_bytes[6];
            memcpy(mac_bytes,data_ptr,6);
            opflex::modb::MAC mac(mac_bytes);
            if(shouldLog()) {
                ostr << mac;
            }
            break;
        }
        case FLDTYPE_VARBYTES:
        {
            //Atleast with Geneve header which is TLV based,
            //option length cannot exceed 128 bytes
            unsigned char bytes[124];
            uint32_t var_length=0;
            var_length = p.inferredDataLength;
            if(var_length > (length - bitOffset/8)) {
                err = -1;
                return err;
            }
            memcpy(bytes,data_ptr,var_length);
            if(shouldLog()) {
                for(uint32_t i=0; i < var_length; i++) {
                    ostr << hex << bytes[i] << " ";
                }
            }
            p.inferredDataLength = 0;
            break;
        }
        default:
        case FLDTYPE_NONE:
        {
            break;
        }
    }
    if(shouldLog()) {
        p.formattedFields[outSeq-1] += ostr.str();
    }
    if(isTupleField()) {
        std::string tupleStr = ostr.str();
        p.packetTuple.setField((unsigned)(tupleSeq-1), tupleStr);
    }
    return err;
}

int PacketDecoderLayer::decode(const unsigned char *buf, std::size_t length, ParseInfo &p) {
    int err = 0;
    if (length < byteLength) {
        LOG(ERROR) << "Remaining length is less than header length";
        return -1;
    }
    p.formattedFields.clear();
    p.formattedFields.resize(numOutArgs);
    if(!isOptionLayer()) {
        p.nextLayerTypeId = getNextTypeId();
    }
    for( auto &fld : pktFields) {
        //For some option headers, length field is optional.
        //Check whether we have a valid length field first
        if(fld.getIsLength()) {
            p.hasOptBytes = hasOptBytes(p);
        }
        err = fld.decode(buf, length, p);
        if(err) {
            return err;
        }
        //We have read the length field of the option header.
        //Compute how many data bytes are expected as well as the length of
        //the entire header
        if(fld.getIsLength()) {
            p.inferredLength = getVariableHeaderLength(p.inferredLength);
            p.inferredDataLength = getVariableDataLength(p.inferredLength);
        }
        if((byteLength==0) && (p.inferredLength > length)) {
            LOG(ERROR) << "Incorrect option header length";
            return -1;
        }
    }
    if(haveOptions()) {
        p.optionLayerTypeId = getOptionLayerTypeId();
        getOptionLength(p);
    }
    if(byteLength != 0) {
        p.parsedLength += byteLength;
    } else {
        p.parsedLength += p.inferredLength;
        p.inferredLength = 0;
    }
    if(isOptionLayer()) {
        if(p.pendingOptionLength >= p.parsedLength) {
            p.pendingOptionLength -= p.parsedLength;
        } else {
            err = -1;
        }
    }

    boost::format fmtStr;
    getFormatString(fmtStr);
    for(unsigned i=1; i<=numOutArgs; i++) {
        fmtStr%p.formattedFields[i-1];
    }
    try {
        p.parsedString += fmtStr.str();
    } catch(boost::io::too_few_args& exc) {
        LOG(ERROR)<< exc.what();
    }
    return err;
}

void PacketDecoder::registerLayer(shared_ptr<PacketDecoderLayer>& decoderLayer) {
    if(!decoderLayer) {
        return;
    }
    layerTypeMap.insert(make_pair(decoderLayer->getTypeName(),decoderLayer->getTypeId()));
    layerNameMap.insert(make_pair(decoderLayer->getName(),decoderLayer->getId()));
    if(decoderLayer->getNextTypeId() != 0) {
        layerTypeMap.insert(make_pair(decoderLayer->getNextTypeName(),decoderLayer->getNextTypeId()));
    }
    if(decoderLayer->haveOptions()) {
        layerNameMap.insert(make_pair(decoderLayer->getOptionLayerName(),decoderLayer->getOptionLayerId()));
        layerTypeMap.insert(make_pair(decoderLayer->getOptionLayerName(),decoderLayer->getOptionLayerTypeId()));
    }
    layerIdMap.insert(make_pair(decoderLayer->getId(),decoderLayer));
    decoderMapRegistry[decoderLayer->getTypeId()].insert(
            make_pair(decoderLayer->getKey(),decoderLayer));
    LOG(DEBUG) << "Registered Layer " << decoderLayer->getName() << " ("
            << decoderLayer->getTypeId() << "," << decoderLayer->getKey() << ")";
}

int PacketDecoder::decode(const unsigned char *buf, std::size_t length, ParseInfo &p) {
    uint32_t layerId = baseLayerId;
    int ret=0;
    bool failed = false;
    shared_ptr<PacketDecoderLayer> pktDecoderLayer = getLayerById(layerId);
    while(length !=0) {
        if(!pktDecoderLayer) {
            failed = true;
            break;
        }
        ret = pktDecoderLayer->decode(buf, length, p);
        if(ret) {
            failed = true;
            break;
        }
        if(p.pendingOptionLength) {
            if(!getLayerByTypeKey(p.optionLayerTypeId, 0, pktDecoderLayer)){
               failed = true;
               break;
            }
        } else {
            if(p.nextLayerTypeId == 0) {
                break;
            }
            if(!getLayerByTypeKey(p.nextLayerTypeId, p.nextKey, pktDecoderLayer)){
               break;
            }
        }
        length -= p.parsedLength;
        buf += p.parsedLength;
        p.parsedLength=0;
    }
    if(failed) {
        if(ret != 0) {
            return ret;
        } else {
            return -1;
        }
    }
    return 0;
}

int PacketDecoder::configure() {
    shared_ptr<PacketDecoderLayer> sptrEthernet(new EthernetLayer());
    sptrEthernet->configure();
    registerLayer(sptrEthernet);
    shared_ptr<PacketDecoderLayer> sptrQtag(new QtagLayer());
    sptrQtag->configure();
    registerLayer(sptrQtag);
    shared_ptr<PacketDecoderLayer> sptrIPv4(new IPv4Layer());
    sptrIPv4->configure();
    registerLayer(sptrIPv4);
    shared_ptr<PacketDecoderLayer> sptrGeneve(new GeneveLayer());
    sptrGeneve->configure();
    registerLayer(sptrGeneve);
    shared_ptr<PacketDecoderLayer> sptrGeneveOpt(new GeneveOptLayer());
    sptrGeneveOpt->configure();
    registerLayer(sptrGeneveOpt);
    shared_ptr<PacketDecoderLayer> sptrArp(new ARPLayer());
    sptrArp->configure();
    registerLayer(sptrArp);
    shared_ptr<PacketDecoderLayer> sptrICMP(new ICMPLayer());
    sptrICMP->configure();
    registerLayer(sptrICMP);
    shared_ptr<PacketDecoderLayer> sptrTCP(new TCPLayer());
    sptrTCP->configure();
    registerLayer(sptrTCP);
    shared_ptr<PacketDecoderLayer> sptrTCPOpt(new TCPOptLayer());
    sptrTCPOpt->configure();
    registerLayer(sptrTCPOpt);
    shared_ptr<PacketDecoderLayer> sptrUDP(new UDPLayer());
    sptrUDP->configure();
    registerLayer(sptrUDP);
    shared_ptr<PacketDecoderLayer> sptrIPv6(new IPv6Layer());
    sptrIPv6->configure();
    registerLayer(sptrIPv6);
    /*Set the base layer id*/
    baseLayerId = sptrGeneve->getId();
    return 0;
}

}
