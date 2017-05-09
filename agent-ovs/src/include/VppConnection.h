/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for VppConnection
 *
 * Copyright (c) 2017 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */



#ifndef VPPCONNECTION_H
#define VPPCONNECTION_H

#include <IVppConnection.h>

// VPP client C code
extern "C" {
#include "pneum.h"
};

namespace ovsagent {

class VppConnection : public IVppConnection  {

        /**
         * Client name which gets from opflex config
         */
        std::string name;

        /**
         * VPP's ID for this client connection
         */
        u32 clientIndex;

        u32 contextId;

        /**
         * Connection state tracking bool variable
         */
        // CONTROL_PING can be used for API connection "keepalive"
        bool connected = false;

    protected:
        typedef boost::bimap< u16, std::string> messageIdsBiMap; //LEFT by ID, RIGHT by NAME
        typedef messageIdsBiMap::value_type messageIdsPair;
        typedef messageIdsBiMap::left_map::const_iterator messageIdsIdIter;
        typedef messageIdsBiMap::right_map::const_iterator messageIdsNameIter;

        static messageIdsBiMap messageIds;
        static std::map< std::string, std::string> messageCRCByName; //Name, CRC


    public:

        /**
         * Default constructor
         */

        VppConnection();
        virtual ~VppConnection();
        /**
         * get current per message context
         */
        u32 getContextId();

        /**
         * increment and return per message ID
         */
        u32 getNextContextId();

        /**
         * message context (instance of a message ID) converted into host order
         */
        u32 getContextIdParm();

        /**
         * incremented message context converted into host order
         */
        u32 getNextContextIdParm();

        virtual int lockedWrite(char* msg, int size);

        u16 lockedGetMsgIndex(std::string name);

        virtual int lockedConnect(std::string name, pneum_callback_t cb);

        virtual void lockedDisconnect();

        /**
         * Returns VPP message ID
         */
        u16 getMessageIdByName(std::string);

        /**
         * Returns VPP message name from its ID
         */
        std::string getMessageNameById(u16);

        /**
         * Returns message_crc for VPP getMessageId()
         */
        std::string getMessageWithCRCByName(std::string);

        /**
         * Static function for VPP API callback handling
         */
        static void vppMessageReceiveHandler(char* data, int len);

        void messageReceiveHandler(char* data, int len);

        u32 lockedGetClientIndex();

        /**
         * Set the name of the switch
         *
         * @param _name the switch name to use
         */
        void setName(std::string _name);

        /**
         * Get the name of the switch
         *
         * @return name of the switch to use
         */
        std::string getName();

        /**
         * TODO:
         *
         * @returns 0 success or -'ve fail
         */
        virtual int populateMessageIds();

        /**
         * Real connect function
         *
         * @param TODO
         * @return
         */
        int connect (std::string name);

        /**
         * Real send function
         *
         * @param msg
         * @param size
         * @param replyId TODO
         * @return
         */
        int send(char* msg, int size, u16 replyId);

        /**
         * Enable Stream function
         *
         * @param TODO
         * @param
         */
        void enableStream(u32 context, u16 streamId);

        /**
         * Message termination check for multipart message reply
         *
         * For multipart messages, send the message ending with '_dump' from
         * Opflex agent. Also send the following barrierSend. VPP will start
         * sending multipart messages in response to dump message and at the
         * end it will respond to barrierSend message.
         */
        virtual int barrierSend(u32 context);

        /**
         * uses messageMap of futures
         */
        std::vector<char*> messagesReceived(u32 context);

        /**
         * uses streamMap of futures
         */
        std::vector<char*> readStream(u32 context, u16 streamId);

        /**
         * Terminate the connection with VPP
         */
        void disconnect();

        bool isConnected();

        /**
         * Return the API message ID based on message name
         */
        u16 getMsgIndex(std::string name);

        static const u32 VPP_SWIFINDEX_NONE = UINT32_MAX;

    };

} //namespace ovsagent
#endif /* VPPCONNECTION_H */
