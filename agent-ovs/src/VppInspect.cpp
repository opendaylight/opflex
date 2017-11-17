/*
 * Copyright (c) 2017 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <cassert>
#include <vector>
#include <string>
#include <boost/algorithm/string.hpp>

#include "logging.h"
#include "VppInspect.h"
#include "vom/inspect.hpp"

namespace ovsagent
{

VppInspect::VppInspect(const std::string &sock_name):
    mSockName(sock_name),
    mInspect()
{
    int rc;
    uv_loop_init(&mServerLoop);
    mServerLoop.data = this;

    rc = uv_async_init(&mServerLoop, &mAsync, VppInspect::on_cleanup);

    rc = uv_thread_create(&mServerThread, run, this);
    if (rc < 0)
    {
        LOG(ERROR) << "inspect - thread create error:" << uv_strerror(rc);
    }
}

VppInspect::~VppInspect()
{
    uv_async_send(&mAsync);
    uv_thread_join(&mServerThread);
    uv_loop_close(&mServerLoop);

    LOG(INFO) << "inspect - close";
}

void VppInspect::on_cleanup(uv_async_t* handle)
{
    VppInspect *ins = static_cast<VppInspect*>(handle->loop->data);

    uv_stop(&ins->mServerLoop);
}

void VppInspect::run(void* ctx)
{
    VppInspect *ins = static_cast<VppInspect*>(ctx);
    uv_pipe_t server;
    int rv;

    /* remove the request file if it exists already */
    unlink(ins->mSockName.c_str());

    uv_pipe_init(&ins->mServerLoop, &server, 0);

    LOG(INFO) << "inspect - open:" << ins->mSockName;

    if ((rv = uv_pipe_bind(&server, ins->mSockName.c_str())))
    {
        LOG(ERROR) << "inspect - Bind error:" << uv_err_name(rv);
        return;
    }
    if ((rv = uv_listen((uv_stream_t*) &server, 1, on_connection)))
    {
        LOG(ERROR) << "inspect - Listen error:" << uv_err_name(rv);
        return;
    }

    uv_run(&ins->mServerLoop, UV_RUN_DEFAULT);
    uv_close((uv_handle_t*)&server, NULL);
}


VppInspect::write_req_t::~write_req_t()
{
    free(buf.base);
}

VppInspect::write_req_t::write_req_t(std::ostringstream &output)
{
    buf.len = output.str().length();
    buf.base = (char*) malloc(buf.len);
    memcpy(buf.base,
           output.str().c_str(),
           buf.len);
}

void VppInspect::on_alloc_buffer(uv_handle_t *handle,
                                 size_t size,
                                 uv_buf_t *buf)
{
    buf->base = (char*) malloc(size);
    buf->len = size;
}

void VppInspect::on_write(uv_write_t *req,
                          int status)
{
    write_req_t *wr = (write_req_t*) req;

    if (status < 0)
    {
        LOG(ERROR) << "inspect - Write error:" << uv_err_name(status);
    }

    delete wr;
}

void VppInspect::do_write(uv_stream_t *client,
                          std::ostringstream &output)
{
    write_req_t *req = new write_req_t(output);

    uv_write((uv_write_t*) req, client, &req->buf, 1, on_write);
}

void VppInspect::on_read(uv_stream_t *client,
                         ssize_t nread,
                         const uv_buf_t *buf)
{
    VppInspect *ins = static_cast<VppInspect*>(client->loop->data);

    if (nread > 0)
    {
        std::ostringstream output;
        std::string message(buf->base);
        message = message.substr(0, nread);
        boost::trim(message);

        if (message.length())
        {
            ins->mInspect.handle_input(message, output);
        }
        output << "# ";

        do_write((uv_stream_t*) client, output);
    }
    else if (nread < 0)
    {
        if (nread != UV_EOF)
        {
            LOG(ERROR) << "inspect - Read error:" << uv_err_name(nread);
        }
        uv_close((uv_handle_t*) client, NULL);
    }

    free(buf->base);
}

void VppInspect::on_connection(uv_stream_t* server,
                               int status)
{
    VppInspect *ins = static_cast<VppInspect*>(server->loop->data);

    if (status == -1)
    {
        // error!
        return;
    }

    uv_pipe_t *client = (uv_pipe_t*) malloc(sizeof(uv_pipe_t));
    uv_pipe_init(&ins->mServerLoop, client, 0);

    if (uv_accept(server, (uv_stream_t*) client) == 0)
    {
        std::ostringstream output;

        output << "Welcome: VPP inspect" << std::endl;
        output << "# ";

        do_write((uv_stream_t*) client, output);

        uv_read_start((uv_stream_t*) client,
                      VppInspect::on_alloc_buffer,
                      VppInspect::on_read);
    }
    else
    {
        uv_close((uv_handle_t*) client, NULL);
    }
}

} // namespace ovsagent
