/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Copyright (c) 2017 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef __VPP_INSPECT_H__
#define __VPP_INSPECT_H__

#include <map>
#include <vector>
#include <string>
#include <sstream>
#include <uv.h>

#include "vom/inspect.hpp"

namespace ovsagent {
    /**
     * A means to inspect the state VPP has built, in total, and per-client
     * To use do:
     *   socat - UNIX-CONNECT:/path/to/sock/in/opflex.conf
     * and follow the instructions
     */
    class VppInspect
    {
    public:
        /**
         * Constructor
         */
        VppInspect(const std::string &sockname);

        /**
         * Destructor to tidyup socket resources
         */
        ~VppInspect();

    private:
        /**
         * Call operator for running in the thread
         */
        static void run(void* ctx);

        /**
         * A write request
         */
        struct write_req_t
        {
            write_req_t(std::ostringstream &output);
            ~write_req_t();

            uv_write_t req;
            uv_buf_t buf;
        };

        /**
         * Write a ostream to the client
         */
        static void do_write(uv_stream_t *client, std::ostringstream &output);

        /**
         * Called on creation of a new connection
         */
        static void on_connection(uv_stream_t* server,
                                  int status);

        /**
         * Call when data is written
         */
        static void on_write(uv_write_t *req, int status);

        /**
         * Called when data is read
         */
        static void on_read(uv_stream_t *client,
                            ssize_t nread,
                            const uv_buf_t *buf);

        /**
         * Called to allocate buffer space for data to be read
         */
        static void on_alloc_buffer(uv_handle_t *handle,
                                    size_t size,
                                    uv_buf_t *buf);

        /**
         * Called to cleanup the thread and socket during destruction
         */
        static void on_cleanup(uv_async_t* handle);

        /**
         * Async handle so we can wakeup the loop
         */
        uv_async_t mAsync;

        /**
         * The libuv loop
         */
        uv_loop_t mServerLoop;

        /**
         * The libuv thread context in which we run the loop
         */
        uv_thread_t mServerThread;

        /**
         * The inspect unix domain socket name, from the config file
         */
        std::string mSockName;

        /**
         * VPP inspect object to handle data read from the socket
         */
        VOM::inspect mInspect;
    };
};

#endif
