/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef  UDP_UTIL_H		/* Has the file been INCLUDE'd already? */
#define  UDP_UTIL_H  yes

#ifdef __cplusplus
    extern  "C" {
#endif

#include  "skt-util.h"


typedef struct _udp_endpoint *udp_endpoint;

extern int udp_util_debug;		/* Global debug switch (1/0 = yes/no). */

extern int udp_create(const char *server_name, udp_endpoint parent,
                      udp_endpoint *endpoint);
extern int udp_destroy(udp_endpoint endpoint);
extern int udp_fd(udp_endpoint endpoint);
extern int udp_is_readable(udp_endpoint endpoint);
extern int udp_is_up(udp_endpoint endpoint);
extern int udp_is_writeable(udp_endpoint endpoint);
extern char *udp_name(udp_endpoint endpoint);
extern int udp_read(udp_endpoint endpoint, double timeout,
                    int max_bytes_to_read, char *buffer, int *num_bytes_read,
                    udp_endpoint *source);
#define udp_set_buf(endpoint, recv_size, send_size) \
    sktSetBuf (udp_name (endpoint), udp_fd (endpoint), recv_size, send_size)
extern int udp_write(udp_endpoint destination, double timeout,
                     int num_bytes_to_write, const char *buffer);

#ifdef __cplusplus
    }
#endif

#endif				/* If this file was not INCLUDE'd previously. */
