/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */


#ifndef  TCP_UTIL_H		/* Has the file been INCLUDE'd already? */
#define  TCP_UTIL_H  yes

#ifdef __cplusplus
    extern  "C" {
#endif


#include <sys/uio.h>

#include "skt-util.h"


/*
*    Session states
*/
#define SESS_TCPCONN       0x0100       /* TCP connect established              */
#define SESS_GOOD          0x0200       /* IDENTIFY has been recv and no errors */
#define SESS_FAIL          0x0400       /* IDENTIFY failed                      */
#define SESS_DISSCONN      0x0800       /* DISCONNECT recv have not tcpDestroy-ed yet */
#define SESS_DEAD          0x0000       /* No session yet                       */
#define SESS_SUBREAD       0x0001       /* Sub-state for last command           */
#define SESS_SUBWRITE      0x0002       /* Sub-state for last command           */
#define SESS_SUBDISCONN    0x0004       /* Sub-state for last command           */
#define SESS_SUBPING       0x0008       /* Sub-state for last command           */          

/*
 *    TCP Network Endpoint (Client View) and Definitions.
 */
typedef struct _tcp_end_point *tcp_end_point;

/*
 *    Miscellaneous declarations.
 */
extern int tcp_util_db_level;	/* Global debug level */
extern int tcp_util_debug;		/* Global debug switch (1/0 = yes/no). */

/*
 *    Public functions.
 */
extern int tcp_answer(tcp_end_point listening_point, double timeout,
                      tcp_end_point *data_point);
extern int tcp_call(char *serverName, int noWait, tcp_end_point *data_point);
extern int tcp_call_local (char *serverName, int noWait,
                           tcp_end_point *data_point);
extern int tcp_complete (tcp_end_point data_point, double timeout,
                         int destroyOnError);
extern int tcp_destroy(tcp_end_point port);
extern int tcp_fd(tcp_end_point endpoint);
extern int tcp_is_readable(tcp_end_point data_point);
extern int tcp_is_up(tcp_end_point data_point);
extern int tcp_is_writeable(tcp_end_point data_point);
extern int tcp_listen(const char *port_name, int backlog,
                      tcp_end_point *listening_point);
extern int tcp_listen_local(const char *port_name, int backlog,
                           tcp_end_point *listening_point);
extern inline char *tcp_name(tcp_end_point endpoint);
extern int tcp_read (tcp_end_point data_point, double timeout, 
                     int num_bytes_to_read, char *buffer, int *num_bytes_read);
extern int tcp_request_pending(tcp_end_point listening_point);
#define tcpSetBuf(endpoint, receive_size, sendSize) \
    sktSetBuf (tcp_name (endpoint), tcp_fd(endpoint), receive_size, sendSize)
extern int tcp_write(tcp_end_point data_point, double timeout,
                     int num_bytes_to_write, const char *buffer,
                     int *num_bytes_written);
extern int tcp_writev(tcp_end_point data_point, double timeout,
                      struct iovec *iov, int iovcount,
                      int *num_bytes_written);
extern int tcp_readv(tcp_end_point data_point, double timeout,
                     struct iovec *iov, int iovcount, int *num_bytes_read);
extern int tcp_client_addr_str(tcp_end_point endpoint, char *ipaddr);

#ifdef __cplusplus
    }
#endif

#endif	 		/* If this file was not INCLUDE'd previously. */
