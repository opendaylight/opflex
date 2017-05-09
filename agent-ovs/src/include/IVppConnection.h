/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Interface for VppConnection
 *
 * Copyright (c) 2017 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */



#ifndef IVPPCONNECTION_H
#define IVPPCONNECTION_H

#include <string.h>
#include <map>
#include <vector>
#include <boost/asio/ip/address.hpp>
#include <boost/bimap.hpp>
#include <future>
#include <VppTypes.h>

namespace ovsagent {

class IVppConnection  {

public:

    /**
     * get current per message context
     */
    virtual u32 getContextId() =0;

    /**
     * increment and return per message ID
     */
    virtual u32 getNextContextId() =0;

    /**
     * message context (instance of a message ID) converted into host order
     */
    virtual u32 getContextIdParm() =0;

    /**
     * incremented message context converted into host order
     */
    virtual u32 getNextContextIdParm() =0;

    /**
     * Returns VPP message ID
     */
    virtual u16 getMessageIdByName(std::string) =0;

    /**
     * Returns VPP message name from its ID
     */
    virtual std::string getMessageNameById(u16) =0;

    /**
     * Returns message_crc for VPP getMessageId()
     */
    virtual std::string getMessageWithCRCByName(std::string) =0;

    /**
     * Static function for VPP API callback handling
     */
    virtual void messageReceiveHandler(char* data, int len) =0;

    /**
     * Set the name of the switch
     *
     * @param _name the switch name to use
     */
    virtual void setName(std::string name) =0;

    /**
     * Get the name of the switch
     *
     * @return name of the switch to use
     */
    virtual std::string getName() =0;

    /**
     * TODO:
     *
     * @returns 0 success or -'ve fail
     */
    virtual int populateMessageIds()=0;

    /**
     * Real connect function
     *
     * @param TODO
     * @return
     */
    virtual int connect (std::string name) =0;

    /**
     * Real send function
     *
     * @param msg
     * @param size
     * @param replyId
     * @return
     */
    virtual int send(char* msg, int size, u16 replyId) =0;

    /**
     * Enable Stream function
     *
     * @param TODO
     * @param
     */
    virtual void enableStream(u32 context, u16 streamId) =0;

    /**
     * uses messageMap of futures
     */
    virtual std::vector<char*> messagesReceived(u32 context) =0;

    /**
     * uses streamMap of futures
     */
    virtual std::vector<char*> readStream(u32 context, u16 streamId) =0;

    /**
     * Terminate the connection with VPP
     */
    virtual void disconnect() =0;

    virtual bool isConnected() =0;

    /**
     * Return the API message ID based on message name
     */
    virtual u16 getMsgIndex(std::string name) =0;

};

} //namespace ovsagent
#endif /* IVPPCONNECTION_H */
