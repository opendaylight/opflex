/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#define _GNU_SOURCE
#include <stdio.h> /* for aprintf() */

/* this code is very far from portable, but it's all throw-away code
 * that will be completely replaced once we have our state machine
 * and parser
 */

#include <noiro/policy-agent/comms-internal.h>
#include <noiro/debug_with_levels.h>

/* FIXME: this is just the most brutal eXtreme Programming */
int send_identity(peer_t * peer, char const * domain,
        char const * name) {

    DBUG_ENTER(__func__);

    uv_buf_t buf = {
          .len = asprintf(&buf.base,
                  "{\r\n"
                  "  \"jsonrpc\": \"1.0\",\r\n"
                  "  \"method\": \"send_identity\",\r\n"
                  "  \"params\": [{\r\n"
                  "    \"domain\": \"%s\",\r\n"
                  "    \"name\": \"%s\",\r\n"
                  "    \"my-role\": [\"policy-agent\"],\r\n"
                  "    \"peers\": [],\r\n"
                  "  }],\r\n"
                  "  \"id\": 1\r\n"
                  "}\r\n\r\n", domain, name),
    };

    if (!buf.base) {
        DBUG_RETURN(UV_ENOMEM);
    }

    int rc;
    if ((rc = uv_write(&peer->write_req,
                (uv_stream_t*) &peer->handle,
                &buf,
                1,
                on_write))) {
        DBUG_PRINT(kDBG_E, ("uv_write: %s", uv_err_name(rc)));
        DBUG_RETURN(rc);
    }

    DBUG_RETURN(0);
}

/* FIXME: this is just the most brutal eXtreme Programming */
int send_identity_response(peer_t * peer, char const * domain,
        char const * name) {

    DBUG_ENTER(__func__);

    uv_buf_t buf = {
          .len = asprintf(&buf.base,
                  "{\r\n"
                  "  \"jsonrpc\": \"1.0\",\r\n"
                  "  \"result\": {\r\n"
                  "    \"domain\": \"%s\",\r\n"
                  "    \"name\": \"%s\",\r\n"
                  "    \"my-role\": [\"policy-agent\"],\r\n"
                  "    \"peers\": []\r\n"
                  "  },\r\n"
                  "  \"error\": null,\r\n"
                  "  \"id\": 1\r\n"
                  "}\r\n\r\n", domain, name),
    };

    if (!buf.base) {
        DBUG_RETURN(UV_ENOMEM);
    }

    int rc;
    if ((rc = uv_write(&peer->write_req,
                (uv_stream_t*) &peer->handle,
                &buf,
                1,
                on_write))) {
        DBUG_PRINT(kDBG_E, ("uv_write: %s", uv_err_name(rc)));
        DBUG_RETURN(rc);
    }

    DBUG_RETURN(0);
}

#include <string.h>
char * bsd_strnstr(const char *s, const char *find, size_t slen)
{
    char c, sc;
    size_t len;

    if ((c = *find++) != '\0') {
        len = strlen(find);
        do {
            do {
                if (slen-- < 1 || (sc = *s++) == '\0')
                    return (NULL);
            } while (sc != c);
            if (len > slen)
                return (NULL);
        } while (strncmp(s, find, len) != 0);
        s--;
    }
    return ((char *)s);
}

void do_the_needful_for_identity(peer_t * peer, ssize_t nread, char * buf) {
    if(bsd_strnstr(buf, "send_identity", nread-1)) { /* may be off by one */
        send_identity_response(peer, "noironetworks.com", "pocus");
    } else {
        if(bsd_strnstr(buf, "result", nread-1)) { /* may be off by one */
            peer->got_resp = true;
        }
    }
}
