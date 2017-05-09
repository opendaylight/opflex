/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for MockVppConnection
 *
 * Copyright (c) 2017 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */



#ifndef MOCKVPPCONNECTION_H
#define MOCKVPPCONNECTION_H

#include <VppConnection.h>

// VPP client C code
extern "C" {
#include "pneum.h"
};

namespace ovsagent {

        using u8 = uint8_t;
        using u16 = uint16_t;
        using u32 = uint32_t;
        using i8 = int8_t;
        using i16 = int16_t;
        using i32 = int32_t;


        class MockVppConnection : public VppConnection {


        public:

                /**
                 * Default constructor
                 */

                MockVppConnection();
                virtual ~MockVppConnection();

                int lockedWrite(char* msg, int size);

                int lockedConnect(std::string name, pneum_callback_t cb);

                void lockedDisconnect();

                /**
                 * TODO:
                 *
                 * @returns 0 success or -'ve fail
                 */
                int populateMessageIds();


                /**
                 * Return the API message ID based on message name
                 */
                u16 getMsgIndex(std::string name);

                /**
                 * Message termination check for multipart message reply
                 *
                 * For multipart messages, send the message ending with '_dump' from
                 * Opflex agent. Also send the following barrierSend. VPP will start
                 * sending multipart messages in response to dump message and at the
                 * end it will respond to barrierSend message.
                 */
                int barrierSend(u32 context);



        };

} //namespace ovsagent
#endif /* MOCKVPPCONNECTION_H */
