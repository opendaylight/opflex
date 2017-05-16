/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for VppApi
 *
 * Copyright (c) 2017 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */



#ifndef VPP_CONNECTION_H
#define VPP_CONNECTION_H

using namespace std;

namespace ovsagent {

  class VppConnection {

  public:

    VppConnection();

    VppConnection(string);

    /**
     * Connection status via a control_ping
     *
     * @return connection_status
     */
    auto isConnected() -> bool;

    /**
     * Send API message to VPP on shared memory connection
     *
     * @param msg - char* to VPP API message struct of form vl_api_xxxxx_t
     * @param size - length in bytes of msg
     *
     * @return return code, 0 = good, <0 = bad
     */
    auto lockedWrite(char* msg, int size) -> int;

    /**
     * Read an API message from VPP's shared memory interface
     *
     * @param msg - char** - side effect VPP API message struct of form
     *              vl_api_xxxxx_reply_t or vl_api_xxxx_t
     * @param reply_len - int* side effect length of VPP API message in bytes
     *
     * @return return code, 0 = good, <0 = bad
     */
    auto lockedRead(char** reply, int* reply_len, int timeout ) -> int;

    /**
     * Return number of messages in VPP's API table.
     *
     * VPP dynamically loads nodes and their API's at runtime, so
     * # messages varies based on plugins, version etc.
     *
     * @return int # of messages in API message table
     */
    auto lockedGetMsgTableSize() -> int;

    auto lockedGetMsgIndex(string name) -> u16;

    auto lockedConnect(string name, pneum_callback_t cb) -> int;

    auto lockedDisconnect() -> void;

    auto lockedGetClientIndex() -> u32;

  };
} //namespace ovsagent
#endif /* VPP_CONNECTION_H */
