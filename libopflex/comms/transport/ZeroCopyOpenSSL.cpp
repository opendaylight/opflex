/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */


    class ZeroCopyOpenSSLEngine : public CommunicationPeer::TransportEngine {
      public:
        static TransportEngine create() {
            TransportEngine t = {
                NULL,
                alloc_cb,
                on_read,
            };

            return t;
        }
      private:
        static void alloc_cb(uv_handle_t * h, size_t size, uv_buf_t* buf);
        static void on_read(uv_stream_t * h, ssize_t nread, uv_buf_t const * buf);

        SSL * ssl_;
        BIO * intBio_;
    };

