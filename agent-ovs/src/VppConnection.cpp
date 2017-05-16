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


#include "VppConnection.h"

using namespace std;

namespace ovsagent {

  class VppConnection {

  public:

    VppConnection::VppConnection();

    VppConnection::VppConnection(string);

    /**
     * Connection status via a control_ping
     *
     * @return connection_status
     */
    auto VppConnection::isConnected() -> bool;

    auto VppConnection::lockedWrite(char* msg, int size) -> int
    {
      std::lock_guard<std::mutex> lock(pneum_mutex);
      return pneum_write(msg, size);
    }

    auto VppConnection::lockedRead(char** reply, int* reply_len, int timeout ) -> int
    {
      std::lock_guard<std::mutex> lock(pneum_mutex);
      return pneum_read (reply, reply_len, timeout);
    }

    auto VppConnection::lockedGetMsgTableSize() -> int
    {
      std::lock_guard<std::mutex> lock(pneum_mutex);
      return pneum_msg_table_size();
    }

    auto VppConnection::lockedGetMsgIndex(string name) -> u16
    {
      std::lock_guard<std::mutex> lock(pneum_mutex);
      return pneum_get_msg_index((unsigned char *) name.c_str());
    }

    auto VppConnection::lockedConnect(string name, pneum_callback_t cb) -> int
    {
      return pneum_connect( (char*) name.c_str(), NULL, cb, 32 /* rx queue length */);
    }

    auto VppConnection::lockedDisconnect() -> void
    {
      std::lock_guard<std::mutex> lock(pneum_mutex);
      pneum_disconnect();
    }

    auto VppConnection::lockedGetClientIndex() -> u32
    {
      std::lock_guard<std::mutex> lock(pneum_mutex);
      return pneum_client_index();
    }

  };
} //namespace ovsagent
