/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

/*
 * @file tcp_util.c - TCP/IP Networking Utilities
 *
 * @brief The TCP_UTIL functions allow you to easily establish and communicate
 *    over TCP/IP network connections between client and server processes,
 *    possibly residing on different hosts.  The TCP_UTIL functions follow
 *    a telephone-like model of networking: clients "call" servers and
 *    servers "answer" clients.  Once a network connection is established
 *    between a client and a server, the two can "talk" to each other by
 *    reading from and writing to the connection:
 *
 *                  Client <----------------------> Server
 *
 *    tcp_listen(), tcp_answer(), and tcp_call() are used to establish a
 *    communications link between a client and a server.  The server process
 *    calls tcpListen() to create a network endpoint at which the server
 *    listens for connection requests from clients.  When one is received,
 *    the server process calls tcp_answer() to accept the client connection.
 *    A server may receive and accept connection requests from multiple
 *    clients; the operating system automatically creates a new connection
 *    (data endpoint) for each client.  The server can then multiplex the
 *    servicing of the clients or, if the operating system supports it,
 *    fork separate subprocesses to service each client.
 *
 *    Client processes call tcp_call() to submit connection requests to
 *    the server process (which may be running on another host).  Note
 *    that a process can be both a client and a server with respect to
 *    other processes.
 *
 *    tcp_read() and tcp_write() are used to send and receive data over a
 *    network connection.  Because there is no concept of record boundaries
 *    in a TCP/IP network stream, communicating processes must follow their
 *    own protocol in order to determine how long a message is.  In the
 *    example presented below, the protocol is very simple: every message
 *    is exactly 64 bytes long, even if the text of the message doesn't
 *    fill the transmitted "record".  More sophisticated protocols (e.g.,
 *    the XDR record marking standard) are available.
 *
 *    The following is a very simple server process that listens for and
 *    answers a client "call".  It then reads and displays 64-byte messages
 *    sent by the client.  If the client "hangs up", the server loops back
 *    and waits for another client.
 *
 *        #include  <stdio.h>           -- Standard I/O definitions.
 *        #include  "tcp_util.h"            -- TCP/IP networking utilities.
 *
 *        main (int argc, char *argv[])
 *        {
 *            char  buffer[128];
 *            tcp_end_point client, server;
 *
 *            tcpListen ("<name>", 99, &server);   -- Create listening endpoint.
 *
 *            for (;; ) {         -- Answer next client.
 *                tcp_answer (server, -1.0, &client);
 *                for (;; ) {         -- Service connected client.
 *                    if (tcp_read (client, -1.0, 64, buffer, NULL))  break;
 *                    printf ("Message from client: %s\n", buffer);
 *                }
 *                tcp_destroy (client);     -- Lost client.
 *            }
 *
 *        }
 *
 *    The following client process "calls" the server and, once the connection
 *    is established, sends it 16 messages:
 *
 *        #include  <stdio.h>           -- Standard I/O definitions.
 *        #include  "tcp_util.h"            -- TCP/IP networking utilities.
 *
 *        main (int argc, char *argv[])
 *        {
 *            char  buffer[128];
 *            int  i;
 *            tcp_end_point  server;
 *
 *            tcp_call ("<name>", 0, &server);  -- Call server.
 *
 *            for (i = 0;  i < 16;  i++) {    -- Send messages.
 *                sprintf (buffer, "Hello for the %dth time!", i);
 *                tcp_write (server, -1.0, 64, buffer, NULL);
 *            }
 *
 *            tcp_destroy (server);     -- Hang up.
 *
 *        }
 *
 *
 *@remarks I'm not crazy about the term "endpoint", but I wanted something other
 *    than "socket" that was common to TCP listening sockets and TCP and
 *    UDP data sockets.  I tried "port", but TCP server sockets all have
 *    their listening socket's port number.  I've seen "endpoint" used in
 *    a couple of networking books and in some articles, so there it is.
 *
 *
 *@remarks Public Procedures (for listening endpoints):
 *    tcp_answer() - answers a client connection request.
 *    tcp_listen() - creates a listening endpoint.
 *    tcp_request_pending() - checks if a client is trying to connect.
 *
 * @remarks Public Procedures (for data endpoints, * defined as macros):
 *    tcp_call() - establishes a client-side connection with a server.
 *    tcp_complete() - completes a no-wait call.
 *    tcp_is_readable() - checks if data is waiting to be read on a connection.
 *    tcp_is_up() - checks if a connection is up.
 *    tcp_is_writeable() - checks if data can be written to a connection.
 *    tcp_read() - reads data from a connection.
 *    tcp_readv() - reads data from a connection using the scatter/gather method.
 *  * tcpSetBuf() - changes the sizes of a connection's receive and send buffers.
 *    tcp_write() - writes data to a connection.
 *    tcp_writev() - writes data to a connection using the scater/gather method.
 *
 * @remarks Public Procedures (general):
 *    tcp_destroy() - closes an endpoint.
 *    tcp_fd() - returns the file descriptor for an endpoint's socket.
 *    tcp_name() - returns the name of an endpoint.
 *    tcpDevFd() - returns the descriptor for the device that this 
 *                 endpoint is associated
 *
 */

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#ifndef PATH_MAX
#    include  <sys/param.h>
#    define  PATH_MAX  MAXPATHLEN
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/uio.h>
#include <sys/un.h>
#ifdef sun
#	include <sys/filio.h>
#endif

#include "gendcls.h"
#include "meo-util.h"
#include "str1-util.h"
#include "debug.h"
#include "util.h"
#include "opt-util.h"
#include "tcp-util.h"
#include "get-util.h"
#include "net-util.h"
#include "tv-util.h"

#define MEM_DB_LEVEL  5

/* Delete an endpoint without
   overwriting the value of ERRNO. */
#define  CLEAN_UP(endpoint) \
    { int  status = errno;      \
        tcp_destroy (endpoint); \
        endpoint = NULL;        \
        errno = status;         \
    }


/*
 *    TCP Endpoint - contains information about a server's listening endpoint
 *        or about a client's or server's data endpoint.
 */
typedef  enum  tcp_end_point_type {
    TCP_NONE, TCP_LISTENING_POINT, TCP_DATA_POINT
}  tcp_end_point_type;

typedef  struct  _tcp_end_point {
  char  *name;              /* "<port>[@<host>]" */
  tcp_end_point_type type;  /* Listening or data endpoint? */
  int   family;             /* the family of socket: AF_LOCAL or AF_INET */
  int  fd;                  /* Listening or data socket. */
}  _tcp_end_point;

int  tcp_util_db_level = 4;     /* Global debug level switch - is determined by DEBUGLEVEL (debug.c) */
int  tcp_util_debug = 0;       /* Global debug switch (1/0 = yes/no). */

/*
*
* @brief tcp_answer () Answer a Client's Connection Request.
*
* @remarks
*    The tcp_answer() function waits for and answers a client's request for a
*    network connection.  When a request is accepted, the system automatically
*    creates a new socket (the "data" socket) through which the server can talk
*    to the client.
*
* @remarks invocation:
*        status = tcp_answer (listening_point, timeout, &data_point);
*
*    where
* @param0 <listening_point>  - I
*            is the listening endpoint handle returned by tcp_listen().
* @param1 <timeout>     - I
*            specifies the maximum amount of time (in seconds) that the
*            caller wishes to wait for a connection to be established.
*            A fractional time can be specified; e.g., 2.5 seconds.
*            A negative timeout (e.g., -1.0) causes an infinite wait;
*            a zero timeout (0.0) causes an immediate return if no
*            connection request is pending.
* @param2 <data_point>       - O
*            returns a handle for the data endpoint created by the acceptance
*            of a connection request from another net location (i.e., a client).
* @return <status>      - O
*            returns the status of answering a network connection request:
*            zero if there were no errors, EWOULDBLOCK if the timeout interval
*            expired before a connection was established, and ERRNO otherwise.
*
*/
int tcp_answer (tcp_end_point listening_point, double timeout, 
               tcp_end_point *data_point)
{  
    static char mod[] = "tcpAnser";
    char  *host_name, server_name[PATH_MAX+1];
    fd_set  read_mask;
    int  num_active, optval;
    socklen_t length;
    struct  sockaddr  client_addr;
    struct  timeval  delta_time, expiration_time;
    struct  linger   linger;
    struct  sockaddr_un local_client_addr;

    /* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
     * Wait for the next connection request from a client.
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
  
    /* If a timeout interval was specified, then compute the expiration time
       of the interval as the current time plus the interval. */
      if (timeout >= 0.0)
        expiration_time = tv_add(tv_tod(), tv_createf(timeout));
  
    /* Wait for a connection request from a client. */
      for (;;) {
        if (timeout >= 0.0)
            delta_time = tv_subtract(expiration_time, tv_tod());
        FD_ZERO (&read_mask);  FD_SET(listening_point->fd, &read_mask);
        num_active = select (listening_point->fd+1, &read_mask, NULL, NULL,
                            (timeout < 0.0) ? NULL : &delta_time);
        if (num_active >= 0)
            break;
        if (errno == EINTR)
            continue;
        DEBUG_OUT(tcp_util_db_level, 
                  ("%s Error waiting for connection request on %s.\nselect: ",
                   mod, listening_point->name));
        return (errno);
    }
  
    if (num_active == 0) {
        errno = EWOULDBLOCK;
        if (tcp_util_debug)
            DEBUG_OUT(tcp_util_db_level, 
                      ("%s: Timeout while waiting for connection request on %s.\n",
                       mod, listening_point->name));
        return (errno);
    }
    
    /* Create an endpoint structure for the pending connection request. */
    *data_point = (tcp_end_point) malloc (sizeof (_tcp_end_point));
    DEBUG_OUT(MEM_DB_LEVEL, ("%s: ++++malloc ptr=%p size=%ld\n", 
                             mod, *data_point, sizeof (_tcp_end_point)));
    if (*data_point == NULL) {
        DEBUG_OUT(0, ("%s: Error allocating endpoint structure for %s.\nmalloc: ",
                      mod, listening_point->name));
        return (errno);
    }
  
    /* initialize all the endpoint data */
    (*data_point)->name = NULL;
    (*data_point)->type = TCP_DATA_POINT;
  
    /* Accept the connection request. */
    do {                /* Retry interrupted system calls. */
        if (listening_point->family == AF_UNIX || listening_point->family == AF_LOCAL) {
            length = sizeof(local_client_addr);
            (*data_point)->fd = accept (listening_point->fd,
                                       (struct sockaddr *)&local_client_addr, 
                                       &length);
        }
        else {
            length = sizeof client_addr;
            (*data_point)->fd = accept (listening_point->fd,
                                       &client_addr, &length);
        }
    } while (((*data_point)->fd < 0) && (errno == EINTR));

    if ((*data_point)->fd < 0) {
#ifdef WIN32
        if (errno != WSAEWOULDBLOCK)
#else
            if (errno != EWOULDBLOCK)
#endif
                DEBUG_OUT(0, ("%s: Error accepting connection request on %s.\naccept: ",
                              mod, listening_point->name));
        CLEAN_UP (*data_point);
        return (errno);
    }

    if (listening_point->family == AF_UNIX || listening_point->family == AF_LOCAL) 
        return 0;

  
    /* Configure the socket so that the operating system periodically verifies
       that the connection is still alive by "pinging" the client. */
  
    optval = 1;            /* Enable keep-alive transmissions. */
    if (setsockopt ((*data_point)->fd, SOL_SOCKET, SO_KEEPALIVE,
                    (char *) &optval, sizeof optval) == -1) {
        DEBUG_OUT(0, ("%s: Error enabling keep-alive mode for %s's client connection.\nsetsocketopt: ",
                      mod, listening_point->name));
        CLEAN_UP (*data_point);
        return (errno);
    }
  
    /* 09272001 - dek - fixes the disconnect issue where the connection is closed 
     * before we can get the achnowledge out.
     */
    linger.l_onoff = 1;
    linger.l_linger = 5;  /* linger time in seconds */
    if (setsockopt ((*data_point)->fd, SOL_SOCKET, SO_LINGER,
                    (char *) &linger, sizeof (linger)) == -1) {
        DEBUG_OUT(0, 
                  ("%s: Error enabling linger options mode for %s's client connection.\nsetsocketopt: ",
                   mod, listening_point->name));
        CLEAN_UP (*data_point);
        return (errno);
    }
  
    /* Construct the connection's name. */
  
    host_name = (char *) skt_peer (listening_point->name, (*data_point)->fd);
    if (host_name == NULL)  host_name = "localhost";
    sprintf (server_name, "%d",
             skt_port (listening_point->name, (*data_point)->fd));
    (*data_point)->name = malloc (strlen (server_name) + 1 +
                                 strlen (host_name) + 1);
    DEBUG_OUT(MEM_DB_LEVEL, ("%s: ++++malloc ptr=%p size=%ld\n", mod, *data_point, 
                             strlen (server_name) + 1 + strlen (host_name) + 1));
    if ((*data_point)->name == NULL) {
        DEBUG_OUT(0, ("%s: Error duplicating server name: %s#%s\nmalloc: ",
                      mod, server_name, host_name));
        CLEAN_UP (*data_point);
        return (errno);
    }
    sprintf ((*data_point)->name, "%s#%s", server_name, host_name);
  
  
    /* Return the data connection to the caller. */
  
    DEBUG_OUT(tcp_util_db_level, 
              ("%s: Accepted connection %s, socket %d\n",
               mod, (*data_point)->name, (*data_point)->fd));
  
    return (0);
}

/*
*
* @brief tcp_listen ()
*    Listen for Network Connection Requests from Clients.
*
*
* @brief
*    Function tcp_listen() creates a "listening" endpoint on which a network
*    server can listen for connection requests from clients.  The server
*    then calls tcp_answer() to "answer" incoming requests.
*
*
* @remark  Invocation:
*        status = tcp_listen (server_name, backlog, &listening_point);
*
*    where
*
* @param0 <server_name>      - I
*            is the server's name.  This is used for determining the port
*            associated with the server (via the system's name/port mappings).
*            You can side-step the system maps and explicitly specify a
*            particular port by passing in a decimal number encoded in ASCII
*            (e.g., "1234" for port 1234).
* @param1 <backlog>     - I
*            is the number of connection requests that can be outstanding
*            for the server.  UNIX systems typically allow a maximum of 5.
* @param2 <listening_point>  - O
*            returns a handle for the new listening endpoint.
* @return <status>      - O
*            returns the status of creating the endpoint: zero if there
*            were no errors and ERRNO otherwise.
*/
int tcp_listen (const char *server_name, int backlog, tcp_end_point *listening_point)
{
    static char mod[] = "tcp_listen";
    int optval, port_num;
    struct sockaddr_in  socket_name;

    if (server_name == NULL)
        server_name = "0";
    if (backlog < 0)
        backlog = 99;

    /* Create an endpoint structure. */
    *listening_point = (tcp_end_point) malloc (sizeof (_tcp_end_point));
    DEBUG_OUT(MEM_DB_LEVEL, ("%s: ++++malloc ptr=%p size=%ld\n", mod,
                             *listening_point, sizeof (_tcp_end_point)));
    if (*listening_point == NULL) {
        DEBUG_OUT(0, ("%s: Error allocating endpoint structure for %s.\nmalloc: ",
                      mod, server_name));
        return (errno);
    }

    (*listening_point)->type = TCP_LISTENING_POINT;
    (*listening_point)->fd = -1;
    (*listening_point)->family = AF_INET;

    (*listening_point)->name = strdup (server_name);
    if ((*listening_point)->name == NULL) {
        DEBUG_OUT(0, ("%s: Error duplicating server name: %s\nstrdup: ",
                      mod, server_name));
        CLEAN_UP (*listening_point);
        return (errno);
    }

    /* Lookup the port number bound to the server name. */
    port_num = net_port_of (server_name, "tcp");
    if (port_num == -1) {
        DEBUG_OUT(0, 
                  ("%s:  Error getting server entry for %s.\nnet_port_of: ",
                   mod, server_name));
        CLEAN_UP (*listening_point);
        return (errno);
    }

    /* Set up the network address for the connection. */
    memset (&socket_name, '\0', sizeof socket_name);
    socket_name.sin_family = AF_INET;
    socket_name.sin_port = htons (port_num);
    socket_name.sin_addr.s_addr = INADDR_ANY;

    /* Create a socket for the connection. */
    (*listening_point)->fd = socket (AF_INET, SOCK_STREAM, 0);
    if ((*listening_point)->fd < 0) {
        DEBUG_OUT(0, 
                  ("%s: Error creating listening socket for endpoint %s.\nsocket: ",
                   mod, server_name));
        CLEAN_UP (*listening_point);
        return (errno);
    }

    /* Configure the socket so it can be discarded quickly when no longer needed.
        If the SO_REUSEADDR option is not enabled and server A goes down without
        client B closing its half of the broken connection, server A can't come
        back up again.  Server A keeps getting an "address in use" error, even
        though A is the "owner" of the port and B can't really do anything with
        its broken connection!  The reuse-address option allows A to come right
        back up again and create a new listening socket. */
    optval = 1;            /* Enable address reuse. */
    if (setsockopt ((*listening_point)->fd, SOL_SOCKET, SO_REUSEADDR,
                    (char *) &optval, sizeof optval) == -1) {
        DEBUG_OUT(0, ("%s: Error setting %s endpoint's listening socket for re-use.\nsetsocketopt: ",
                      mod, server_name));
        CLEAN_UP (*listening_point);
        return (errno);
    }

    /* Bind the network address to the socket and enable it to listen for
        connection requests. */
    if (bind ((*listening_point)->fd,
              (struct sockaddr *) &socket_name, sizeof socket_name)) {
        DEBUG_OUT(0, ("%s: Error binding %s endpoint's socket name.\nbind: ",
                      mod, server_name));
        CLEAN_UP (*listening_point);
        return (errno);
    }

    if (port_num == 0) {
        port_num = skt_port (server_name, (*listening_point)->fd);
        DEBUG_OUT(MEM_DB_LEVEL, ("%s:  ----freeing ptr->%p\n",
                                 mod, (*listening_point)->name));
        free ((*listening_point)->name);
        (*listening_point)->name = strndup(NULL, 16);
        if ((*listening_point)->name == NULL) {
            DEBUG_OUT(0, ("%s: Error duplicating port name: %d\nstrndup: ",
                          mod, port_num));
            CLEAN_UP (*listening_point);
        }
        sprintf ((*listening_point)->name, "%d", port_num);
    }

    if (listen ((*listening_point)->fd, backlog)) {
        DEBUG_OUT(0, ("%s: Error enabling acceptance of connection requests on %s endpoint.\nlisten: ",
                      mod, (*listening_point)->name));
        CLEAN_UP (*listening_point);
        return (errno);
    }

    DEBUG_OUT(tcp_util_db_level, ("%s: Listening on %s, port %d, socket %d.\n",
                                  mod, (*listening_point)->name, port_num, 
                                  (*listening_point)->fd));

    return (0);
}

/*
 *
 * @brief tcp_request_pending ()
 *    Check a Listening Port for Pending Connection Requests.
 *
 * @brief
 *    The tcp_request_pending() function checks to see if any connection requests
 *    from potential clients are waiting to be answered.
 *
 *    Invocation:
 *        isPending = tcp_request_pending (listening_point);
 *
 *    where
 * @param0 <listening_point>  - I
 *            is the endpoint handle returned by tcp_listen().
 * @return <isPending>       - O
 *            returns true (a non-zero value) if connection requests are
 *            pending and false (zero) otherwise.
 *
*/

int tcp_request_pending (tcp_end_point  listening_point)
{
    static char mod[] = "tcp_request_pending";
    fd_set  read_mask;
    struct  timeval  timeout;

    /* Poll the listening socket for input. */
    FD_ZERO (&read_mask);  FD_SET (listening_point->fd, &read_mask);
    timeout.tv_sec = timeout.tv_usec = 0;  /* No wait. */
    while (select (listening_point->fd+1, &read_mask, NULL, NULL, &timeout) < 0) {
        if (errno == EINTR)  continue;     /* SELECT interrupted by signal - try again. */
             DEBUG_OUT(0,( "%s: Error polling endpoint %s, socket %d.\nselect: ",
                           mod, listening_point->name, listening_point->fd));
        return (0);
    }

    return (FD_ISSET (listening_point->fd, &read_mask)); /* Request pending? */
}

/*
 *
 * @brief tcp_call () - Request a Network Connection to a Server.
 *
 * @brief Function tcp_call() is used by a client task to "call" a server task
 *    and request a network connection to the server.  If its no-wait
 *    argument is false (zero), tcp_call() waits until the connection is
 *    established (or refused) before returning to the invoking function:
 *
 *        #include  "tcp_util.h"            -- TCP/IP networking utilities.
 *        tcp_end_point  connection;
 *        ...
 *        if (tcp_call ("<server>[@<host>]", 0, &connection)) {
 *            ... error establishing connection ...
 *        } else {
 *            ... connection is established ...
 *        }
 *
 *    If the no-wait argument is true (non-zero), tcp_call() initiates the
 *    connection attempt and immediately returns; the application must then
 *    invoke tcp_complete() to complete the connection attempt:
 *
 *        if (tcp_call ("<server>[@<host>]", 1, &connection)) {
 *            ... error initiating connection attempt ...
 *        } else {
 *            ... do something else ...
 *            if (tcp_complete (connection, -1.0, 1)) {
 *                ... error establishing connection ...
 *            } else {
 *                ... connection is established ...
 *            }
 *        }
 *
 *    No-wait mode is useful in applications which monitor multiple I/O sources
 *    using select(2).  After tcp_call() returns, a to-be-established connection's
 *    file descriptor can be retrieved with tcp_fd() and placed in select(2)'s
 *    write mask.  When select(2) detects that the connection is writeable,
 *    the application can call tcp_complete() to complete the connection.
 *
 *    IMPLEMENTATION NOTE:  Connecting to a server is usually accomplished
 *    by creating an unbound socket and calling connect(), a system call, to
 *    establish the connection.  In order to implement the timeout capability,
 *    the socket is configured for non-blocking I/O before connect() is called.
 *    If the connection cannot be established right away, connect() returns
 *    immediately and select() is called to wait for the timeout interval to
 *    expire or for the socket to become writeable (indicating the connection
 *    is complete).  Once the connection is established, the socket is
 *    reconfigured for blocking I/O.
 *
 *    The basic idea for implementing a timeout capability was outlined in
 *    a posting from W. Richard Stevens (*the* network guru) I retrieved
 *    from one of the WWW news archives.  VxWorks has a non-portable
 *    connectWithTimeout() function which, to save a few "#ifdef"s, I don't
 *    use.
 *
 *
 *    Invocation:
 *  @remark      status = tcp_call (server_name, no_wait, &data_point);
 *
 *    where
 * @param0 <server_name>  - I
 *            is the server's name: "<server>[@<host>]".  The server can be
 *            specified as a name or as a port number.  The host, if given,
 *            can be specified as a name or as a dotted Internet address.
 * @param1 <no_wait>  - I
 *            specifies if tcp_call() should wait for a connection to be
 *            established.  If no_wait is false (zero), tcp_call() waits
 *            until a connection is established or refused before returning
 *            control to the caller.  If no_wait is true (a non-zero value),
 *            tcp_call() initiates a connection attempt and returns to the
 *            caller immediately; the caller is responsible for eventually
 *            calling tcp_complete() to complete the connection.
 * @param2 <data_point>   - O
 *            returns a handle for the endpoint.
 * @return <status>  - O
 *            returns the status of establishing the network connection:
 *            zero if there were no errors and ERRNO otherwise.
 *
 */
int tcp_call (char *server_name_in, int no_wait, tcp_end_point *data_point)
{
    static char mod[] = "tcp_call";
    char  *s, host_name[MAXHOSTNAMELEN+1], server_name[MAXHOSTNAMELEN+1];
    int  length, optval, port_num;
    struct  sockaddr_in  socket_name;

    /*******************************************************************************
        Determine the host and server information needed to make the connection.
    *******************************************************************************/
    /* Parse the host and server names.  If the host name is not defined
        explicitly, it defaults to the local host. */
    s = net_host_of (net_addr_of (NULL));
    if (s == NULL) {
        DEBUG_OUT(0, ("%s: Error getting local host name.\nnet_host_of: ",
                       mod));
        return (errno);
    }
    strcpy (host_name, s);

    s = strchr (server_name_in, '@');
    if (s == NULL) {                /* "<server>" */
        strcpy (server_name, server_name_in);
    } else {                    /* "<server>@<host>" */
        length = s - server_name_in;
        strncpy (server_name, server_name_in, length);
        server_name[length] = '\0';
        strcpy (host_name, ++s);
    }

    /* Create an endpoint structure. */
    *data_point = (tcp_end_point) malloc (sizeof (_tcp_end_point));
    DEBUG_OUT(MEM_DB_LEVEL, ("%s: ++++malloc ptr=%p size=%ld\n", mod,
                             *data_point, sizeof (_tcp_end_point)));
    if (*data_point == NULL) {
        DEBUG_OUT(0, ("%s: Error allocating connection structure for %s.\nmalloc: ",
                      mod, server_name_in));
        return (errno);
    }

    (*data_point)->type = TCP_DATA_POINT;
    (*data_point)->fd = -1;
    (*data_point)->family = AF_INET;
    (*data_point)->name = malloc (strlen (server_name) + 1 +
                                 strlen (host_name) + 1);
    DEBUG_OUT(MEM_DB_LEVEL, ("%s: ++++malloc ptr=%p size=%ld\n", mod,
      (*data_point)->name, strlen (server_name) + 1 + strlen (host_name) + 1));
    if ((*data_point)->name == NULL) {
        DEBUG_OUT(0, ("%s: Error duplicating server name: %s@%s\nmalloc: ",
                      mod, server_name, host_name));
        CLEAN_UP (*data_point);
        return (errno);
    }
    memset((*data_point)->name, 0, (strlen (server_name) + 1 + strlen (host_name) + 1));
    sprintf ((*data_point)->name, "%s@%s", server_name, host_name);

    /* Lookup the port number bound to the server name. */
    port_num = net_port_of (server_name, "tcp");
    if (port_num == -1) {
        DEBUG_OUT(0, ("%s: Error getting server entry for %s.\nnet_port_of: ",
                      mod, server_name_in));
        CLEAN_UP (*data_point);
        return (errno);
    }

    /* Set up the network address for the connection. */
    memset (&socket_name, '\0', sizeof socket_name);
    socket_name.sin_family = AF_INET;
    socket_name.sin_port = htons (port_num);

    socket_name.sin_addr.s_addr = net_addr_of (host_name);
    if ((long) socket_name.sin_addr.s_addr == -1) {
        DEBUG_OUT(0, ("%s: Error getting host entry for %s.\nnet_addr_of: ",
                      mod, host_name));
        CLEAN_UP (*data_point);
        return (errno);
    }

    /*  Establish a connection with the server. */
    /* Create a socket for the connection. */
    (*data_point)->fd = socket (AF_INET, SOCK_STREAM, 0);
    if ((*data_point)->fd < 0) {
        DEBUG_OUT(0, ("%s: Error creating socket for %s.\nsocket: ",
                      mod, (*data_point)->name));
        CLEAN_UP (*data_point);
        return (errno);
    }

    /* Configure the socket so it can be discarded quickly when no longer needed.
        (See the description of the SO_REUSEADDR option under NET_ANSWER, above.) */

    optval = 1;            /* Enable address reuse. */
    if (setsockopt ((*data_point)->fd, SOL_SOCKET, SO_REUSEADDR,
                    (char *) &optval, sizeof optval) == -1) {
        DEBUG_OUT(0, ("%s: Error setting %s's socket for re-use.\nsetsocketopt: ",
                      mod, (*data_point)->name));
        CLEAN_UP (*data_point);
        return (errno);
    }

    /* If caller does not wish to wait for the connection to be established, then
        configure the socket for non-blocking I/O.  This causes the connect(2) call
        to only initiate the attempt to connect; tcp_complete() must be called to
        complete the connection. */
    optval = 1;        /* A value of 1 enables non-blocking I/O. */
    if (no_wait && (ioctl ((*data_point)->fd, FIONBIO, &optval) == -1)) {
        DEBUG_OUT(0, ("%s: Error configuring %s's socket for non-blocking I/O.\nioctl: ",
                      mod, (*data_point)->name));
        CLEAN_UP (*data_point);
        return (errno);
    }

    /* Attempt to establish the connection. */
    if (connect ((*data_point)->fd,
                 (struct sockaddr *) &socket_name, sizeof socket_name) &&
        (!no_wait || (errno != EINPROGRESS))) {
        DEBUG_OUT(0, ("%s: Error attempting to connect to %s.\nconnect: ",
                      mod, (*data_point)->name));
        CLEAN_UP (*data_point);
        return (errno);
    }

    /* If caller does not wish to wait for the connection to be established,
        then return immediately; the caller is responsible for subsequently
        calling tcp_complete(). */
    if (no_wait)  
        return (0);

    /* The connection has been established.  Configure the socket so that the
        operating system periodically verifies that the connection is still alive
        by "pinging" the server. */

    optval = 1;            /* Enable keep-alive transmissions. */
    if (setsockopt ((*data_point)->fd, SOL_SOCKET, SO_KEEPALIVE,
                    (char *) &optval, sizeof optval) == -1) {
        DEBUG_OUT(0, ("%s: Error enabling keep-alive mode for connection to %s.\nsetsockopt: ",
                      mod, (*data_point)->name));
        CLEAN_UP (*data_point);
        return (errno);
    }

    DEBUG_OUT(3, ("%s:  Connected to %s, port %d, socket %d.\n",
                  mod, (*data_point)->name,
                  skt_port ((*data_point)->name, (*data_point)->fd),
                  (*data_point)->fd));

    return (0);    /* Successful completion. */
}

/*
 *
 * @brief tcp_listen_ocal ()
 *    Listen for AF_UNIX type  Connection Requests from Clients.
 *
 * @brief
 *    Function tcp_listen() creates a "listening" endpoint on which a network
 *    server can listen for connection requests from clients.  The server
 *    then calls tcp_answer() to "answer" incoming requests.
 *
 * @remark  Invocation:
 *        status = tcp_listen (server_name, backlog, &listening_point);
 *
 *    where
 * @param0 <server_name>      - I
 *            is the server's name.  This is used for determining the port
 *            associated with the server (via the system's name/port mappings).
 *            You can side-step the system maps and explicitly specify a
 *            particular port by passing in a decimal number encoded in ASCII
 *            (e.g., "1234" for port 1234).
 * @param1 <backlog>     - I
 *            is the number of connection requests that can be outstanding
 *            for the server.  UNIX systems typically allow a maximum of 5.
 * @param2 <listening_point>  - O
 *            returns a handle for the new listening endpoint.
 * @return <status>      - O
 *            returns the status of creating the endpoint: zero if there
 *            were no errors and ERRNO otherwise.
 *
 */
int tcp_listen_local (const char *server_name, int backlog,
                      tcp_end_point *listening_point)
{ 
    static char mod[] = "tcp_listen_local";
    struct sockaddr_un socket_name;
    int len = 0;
    int s;

    if (server_name == NULL)  /* Must have a name for this to work */
        return EACCES;

    if (backlog < 0)
        backlog = 99;

    /* Create an endpoint structure. */
    *listening_point = (tcp_end_point) malloc (sizeof (_tcp_end_point));
    if (*listening_point == NULL) {
        DEBUG_OUT(0, ("(%s) Error allocating endpoint structure for %s.\nmalloc: ",
                      mod, server_name));
        return (errno);
    }
  
    (*listening_point)->type = TCP_LISTENING_POINT;
    (*listening_point)->fd = -1;
    (*listening_point)->family = AF_UNIX;
    (*listening_point)->name = strdup (server_name);
    if ((*listening_point)->name == NULL) {
        DEBUG_OUT(0, ("(%s) Error duplicating server name: %s\nstrdup: ", mod, server_name));
        CLEAN_UP (*listening_point);
        return (errno);
    }
  
    /* Set up the network address for the connection. */
      memset (&socket_name, '\0', sizeof socket_name);
  
    /* Create a socket for the connection. */  
    s = socket (AF_UNIX, SOCK_STREAM, 0);
    if (s == -1) {
        DEBUG_OUT(0, ("(%s) Error creating listening socket for endpoint %s.\nsocket: ",
                      mod, server_name));
        CLEAN_UP (*listening_point);
        return (errno);
    }
    
    /* Bind the network address to the socket and enable it to listen for
       connection requests. */
    socket_name.sun_family = AF_UNIX;
    strcpy(socket_name.sun_path, server_name);
    unlink(socket_name.sun_path);   /* so we don't get a EINVAL error */
    len = sizeof(socket_name);

    printf("socketaddr_un.sun_path = %s socketaddr_un.sun_family = %d\n", 
           socket_name.sun_path, socket_name.sun_family);

    if (bind (s, (struct sockaddr *) &socket_name, len) == -1) {
        DEBUG_OUT(0, ("(%s) Error binding %s endpoint's socket name.\nbind: ",
                      mod, server_name));
        CLEAN_UP (*listening_point);
        return (errno);
    }
  
    if (listen (s, backlog)) {
        DEBUG_OUT(0, ("(%s) Error enabling acceptance of connection requests on %s endpoint.\nlisten: ",
                      mod, (*listening_point)->name));
        CLEAN_UP (*listening_point);
        return (errno);
    }
  
    DEBUG_OUT(tcp_util_db_level, ("(%s) Listening on %s\n",mod, (*listening_point)->name));

    (*listening_point)->fd = s;
    return (0);
}

/*
*
* @brief tcp_call_local ()
*    Request a lcoal Connection to a Server.
*
*    @brief Function tcp_call() is used by a client task to "call" a server task
*    and request a network connection to the server.  If its no-wait
*    argument is false (zero), tcp_call() waits until the connection is
*    established (or refused) before returning to the invoking function:
*
*        #include  "tcp_util.h"            -- TCP/IP networking utilities.
*        tcp_end_point  connection;
*        ...
*        if (tcp_call ("<server>[@<host>]", 0, &connection)) {
*            ... error establishing connection ...
*        } else {
*            ... connection is established ...
*        }
*
*    If the no-wait argument is true (non-zero), tcp_call() initiates the
*    connection attempt and immediately returns; the application must then
*    invoke tcp_complete() to complete the connection attempt:
*
*        if (tcp_call ("<server>[@<host>]", 1, &connection)) {
*            ... error initiating connection attempt ...
*        } else {
*            ... do something else ...
*            if (tcp_complete (connection, -1.0, 1)) {
*                ... error establishing connection ...
*            } else {
*                ... connection is established ...
*            }
*        }
*
*    No-wait mode is useful in applications which monitor multiple I/O sources
*    using select(2).  After tcp_call() returns, a to-be-established connection's
*    file descriptor can be retrieved with tcp_fd() and placed in select(2)'s
*    write mask.  When select(2) detects that the connection is writeable,
*    the application can call tcp_complete() to complete the connection.
*
*    IMPLEMENTATION NOTE:  Connecting to a server is usually accomplished
*    by creating an unbound socket and calling connect(), a system call, to
*    establish the connection.  In order to implement the timeout capability,
*    the socket is configured for non-blocking I/O before connect() is called.
*    If the connection cannot be established right away, connect() returns
*    immediately and select() is called to wait for the timeout interval to
*    expire or for the socket to become writeable (indicating the connection
*    is complete).  Once the connection is established, the socket is
*    reconfigured for blocking I/O.
*
*    The basic idea for implementing a timeout capability was outlined in
*    a posting from W. Richard Stevens (*the* network guru) I retrieved
*    from one of the WWW news archives.  VxWorks has a non-portable
*    connectWithTimeout() function which, to save a few "#ifdef"s, I don't
*    use.
*
*
*    Invocation:
* @remark      status = tcp_call (server_name, no_wait, &data_point);
*
*    where
* @param0 <server_name>  - I
*            is the server's name: "<server>[@<host>]".  The server can be
*            specified as a name or as a port number.  The host, if given,
*            can be specified as a name or as a dotted Internet address.
* @param1 <no_wait>  - I
*            specifies if tcp_call() should wait for a connection to be
*            established.  If no_wait is false (zero), tcp_call() waits
*            until a connection is established or refused before returning
*            control to the caller.  If no_wait is true (a non-zero value),
*            tcp_call() initiates a connection attempt and returns to the
*            caller immediately; the caller is responsible for eventually
*            calling tcp_complete() to complete the connection.
* @param2 <data_point>   - O
*            returns a handle for the endpoint.
* @return <status>  - O
*            returns the status of establishing the network connection:
*            zero if there were no errors and ERRNO otherwise.
*
*/
int tcp_call_local (char *server_name, int no_wait, tcp_end_point *data_point) 
{    
    static char mod[] = "tcp_call_local";
    struct  sockaddr_un  socket_name;
  
    /*
     * Determine the host and server information needed to make the connection.
     */
    /* Create an endpoint structure. */  
    *data_point = (tcp_end_point) malloc (sizeof (_tcp_end_point));
    DEBUG_OUT(MEM_DB_LEVEL, ("(tcp_call) ++++malloc ptr=%p size=%ld\n", *data_point, sizeof (_tcp_end_point)));
    if (*data_point == NULL) {
        DEBUG_OUT(0, ("(tcp_call) Error allocating connection structure for %s.\nmalloc: ",
                      server_name));
        return (errno);
    }
  
    (*data_point)->type = TCP_DATA_POINT;
    (*data_point)->fd = -1;
    (*data_point)->family = AF_UNIX;
  
    (*data_point)->name = malloc (strlen (server_name) + 1);

    DEBUG_OUT(MEM_DB_LEVEL, ("(%s) ++++malloc ptr=%p size-%ld\n", 
                             mod, (*data_point)->name, strlen (server_name) + 1));
    if ((*data_point)->name == NULL) {
        DEBUG_OUT(0, ("(%s) Error duplicating server name: %s\nmalloc: ",
                      mod, server_name));
        CLEAN_UP (*data_point);
        return (errno);
    }
  
    memset((*data_point)->name, 0, strlen (server_name) + 1);
    memcpy((*data_point)->name, server_name, strlen(server_name));

    /* Set up the network address for the connection. */
    memset (&socket_name, '\0', sizeof(socket_name));
    socket_name.sun_family = AF_UNIX;
    strcpy(socket_name.sun_path, server_name);
  
    /*
     *        Establish a connection with the server.
     */
    /* Create a socket for the connection. */
      (*data_point)->fd = socket (AF_UNIX, SOCK_STREAM, 0);
    if ((*data_point)->fd < 0) {
        DEBUG_OUT(0, ("(%s) Error creating socket for %s.\nsocket: ",
                      mod, (*data_point)->name));
        CLEAN_UP (*data_point);
        return (errno);
    }  
  
    /* Attempt to establish the connection. */
    if (connect ((*data_point)->fd, (struct sockaddr *) &socket_name, sizeof(socket_name))) {
        DEBUG_OUT(0, ("(%s) Error attempting to connect to %s.\nconnect: %s",
                      mod, (*data_point)->name, strerror(errno)));
        CLEAN_UP (*data_point);
        return (errno);
    }
  
    DEBUG_OUT(3, ("(%s) Connected to %s\n", mod, (*data_point)->name));
    return (0);    /* Successful completion. */
}

/*
 *
 * @brief tcp_complete ()
 *    Complete a Call to a Server.
 *
 * @brief Function tcp_complete() waits for an asynchronous, network connection
 *    attempt to complete.  Invoking tcp_call() in no-wait mode initiates
 *    an attempt to connect to a network server.  At some later time, the
 *    application must call tcp_complete() to complete the connection attempt
 *    (if it is fated to complete).  See tcp_call() for an example of using
 *    tcp_complete().
 *
 *    The default behavior of tcp_complete() is to NOT destroy the endpoint
 *    created by tcp_call(), regardless of any errors.  Since the endpoint
 *    SHOULD be destroyed in the event of an error, the destroy_on_error flag
 *    provides a short-hand means of waiting for a connection without
 *    explicitly destroying the endpoint if the connection attempt fails:
 *
 *        #include  "tcp_util.h"            -- TCP/IP networking utilities.
 *        tcp_end_point  connection;
 *        ...
 *        if (tcp_call ("<server>[@<host>]", 1, &connection) ||
 *            tcp_complete (connection, 30.0, 1)) {
 *            ... timeout or error establishing connection;
 *                connection is destroyed ...
 *        } else {
 *            ... connection is established ...
 *        }
 *
 *    Some applications, however, may not wish to destroy the connection right
 *    away.  For example, the following code fragment periodically displays an
 *    in-progress message until a connection is established or refused:
 *
 *        if (tcp_call ("<server>[@<host>]", 1, &connection)) {
 *            ... error initiating connection attempt ...
 *        } else {
 *            for (;; ) {
 *                printf ("Waiting for %s ...\n", tcp_name (connection));
 *                if (!tcp_complete (connection, 5.0, 0)) {
 *                    ... connection is established ...
 *                    break;
 *                } else if (errno != EWOULDBLOCK) {
 *                    ... connection attempt failed ...
 *                    tcp_destroy (connection);  connection = NULL;
 *                    break;
 *                }
 *            }
 *        }
 *
 *
 *    Invocation:
 *        status = tcp_complete (data_point, timeout, destroy_on_error);
 *
 *    where
 * @param0 <data_point>       - I
 *            is the endpoint handle returned by tcp_call().
 * @param1 <timeout>     - I
 *            specifies the maximum amount of time (in seconds) that the
 *            caller wishes to wait for the call to complete.  A fractional
 *            time can be specified; e.g., 2.5 seconds.  A negative timeout
 *            (e.g., -1.0) causes an infinite wait; a zero timeout (0.0)
 *            causes an immediate return if the connection is not yet
 *            established.
 * @param2 <destroy_on_error>  - I
 *            specifies if tcp_complete() should destroy the endpoint in the
 *            event of an error.  If this argument is true (a non-zero value),
 *            tcp_complete() calls tcp_destroy() to destroy the endpoint.  If
 *            this argument is false (zero), the calling routine itself is
 *            responsible for destroying the endpoint.
 * @return <status>      - O
 *            returns the status of completing the network connection:
 *            zero if there were no errors, EWOULDBLOCK if the timeout
 *            interval expired before the connection was established,
 *            and ERRNO otherwise.
 *
 */
int tcp_complete (tcp_end_point data_point, double timeout, int destroy_on_error)
{
    static char mod[] = "tcp_complete";
    fd_set  writeMask;
    int  num_active, optval;
    socklen_t length;
    struct timeval delta_time, expiration_time;

    /* If a timeout interval was specified, then compute the expiration time
        of the interval as the current time plus the interval. */
    if (timeout >= 0.0)
        expiration_time = tv_add (tv_tod (), tv_createf (timeout));


    /* Wait for the call to complete, which is indicated by the connection
        being writeable.  */
    for (;; ) {
        if (timeout >= 0.0)  delta_time = tv_subtract (expiration_time, tv_tod ());
        FD_ZERO (&writeMask);  FD_SET (data_point->fd, &writeMask);
        num_active = select (data_point->fd+1, NULL, &writeMask, NULL,
                            (timeout < 0.0) ? NULL : &delta_time);
        if (num_active > 0)  break;
        if (num_active == 0)  errno = EWOULDBLOCK;
        if (errno == EINTR)  continue;
        DEBUG_OUT(0, ("%s: Error waiting to connect to %s.\nselect: ",
                      mod,data_point->name));
        if (destroy_on_error)
            CLEAN_UP (data_point);
        return (errno);
    }

    /* Check the connection's error status. */
    length = sizeof optval;
    if (getsockopt (data_point->fd, SOL_SOCKET, SO_ERROR,
                    (char *) &optval, &length) == -1) {
        DEBUG_OUT(0, ("%s: Error checking error status of connection to %s.\ngetsockopt: ",
                      mod, data_point->name));
        if (destroy_on_error)
            CLEAN_UP (data_point);
        return (errno);
    }

    if (optval) {
        errno = optval;
        DEBUG_OUT(0, ("%s: Error connecting to %s.\nconnect: ",
                      mod, data_point->name));
        if (destroy_on_error)
            CLEAN_UP (data_point);
        return (errno);
    }

    /* The connection has been established.  Configure the socket so that the
     * operating system periodically verifies that the connection is still alive
     * by "pinging" the server. 
     */
    optval = 1;            /* Enable keep-alive transmissions. */
    if (setsockopt (data_point->fd, SOL_SOCKET, SO_KEEPALIVE,
                    (char *) &optval, sizeof optval) == -1) {
        DEBUG_OUT(0, ("%s: Error enabling keep-alive mode for connection to %s.\nsetsockopt: ",
                      mod, data_point->name));
        if (destroy_on_error)
            CLEAN_UP (data_point);
        return (errno);
    }

    /* Reconfigure the socket for blocking I/O. */
    optval = 0;            /* A value of 0 sets blocking I/O. */
    if (ioctl (data_point->fd, FIONBIO, &optval) == -1) {
        DEBUG_OUT(0, ("%s: Error reconfiguring %s's socket for blocking I/O.\nioctl: ",
                      mod, data_point->name));
        if (destroy_on_error)
            CLEAN_UP (data_point);
        return (errno);
    }

    DEBUG_OUT(3, ("%s: Connected to %s, port %d, socket %d.\n",
                  mod, data_point->name, 
                  skt_port(data_point->name, data_point->fd),
                  data_point->fd));
    return (0);            /* Successful completion. */
}

/*
 *
 * @brief tcp_is_readable () - Check if Data is Waiting to be Read.
 *
 * @brief Purpose:
 *    The tcpIsReadable() function checks to see if data is waiting to
 *    be read from a connection.
 *
 *
 *    Invocation:
 *
 *        isReadable = tcpIsReadable (data_point);
 *
 *    where
 *
 * @param0 <data_point>   - I
 *            is the endpoint handle returned by tcp_answer() or tcp_call().
 * @retrun <isReadable>  - O
 *            returns true (a non-zero value) if data is available for
 *            reading and false (zero) otherwise.
 *
*/

int tcpIsReadable (tcp_end_point data_point)
{
    if (data_point == NULL)
        return (0);
    else
        return (skt_is_readable (data_point->name, data_point->fd));
}

/*
 * @brief tcp_is_up () -Check if a Connection is Up.
 *
 * @brief The tcp_is_up() function checks to see if a network connection is still up.
 *
 *    Invocation:
 *        isUp = tcp_is_up (data_point);
 *
 *    where
 * @param0 <data_point>   - I
 *            is the endpoint handle returned by tcp_answer() or tcp_call().
 * @return <isUp>        - O
 *            returns true (a non-zero value) if the network connection is
 *            up and false (zero) otherwise.
 *
 */
int tcp_is_up (tcp_end_point  data_point)
{
    if (data_point == NULL)
        return (0);
    else
        return (skt_is_up(data_point->name, data_point->fd));
}

/*
 * @breif tcp_is_writeable () Check if Data can be Written.
 *
 * @brief The tcp_is_writeable() function checks to see if data can be written
 *    to a connection.
 *
 *    Invocation:
 *        is_writeable = tcp_is_writeable (data_point);
 *
 *    where
 * @param0 <data_point>   - I
 *            is the endpoint handle returned by tcp_answer() or tcp_call().
 * @param1 <isWriteable> - O
 *            returns true (a non-zero value) if data connection is ready
 *            for writing and false (zero) otherwise.
 *
 */
int tcp_is_writeable (tcp_end_point data_point)
{
    if (data_point == NULL)
        return (0);
    else
        return (skt_is_writeable(data_point->name, data_point->fd));
}

/*
* @brief tcp_read () Read Data from a Network Connection.
*
* @brief Function tcp_read() reads data from a network connection.  Because of
*    the way network I/O works, a single record written to a connection
*    by one task may be read in multiple "chunks" by the task at the other
*    end of the connection.  For example, a 100-byte record written by a
*    client may be read by the server in two chunks, one of 43 bytes and
*    the other of 57 bytes.  tcp_read() takes this into account and, if
*    you ask it for 100 bytes, it will automatically perform however many
*    network reads are necessary to collect the 100 bytes.  You can also
*    ask tcp_read() to return the first chunk received, whatever its length;
*    see the num_bytes_to_read argument.
*
*    A timeout can be specified that limits how long tcp_read() waits for
*    the desired amount of data to be received.  When the timeout interval
*    expires, tcp_read() will return whatever input has been received - which
*    may be less than you requested - during that interval.
*
*
*    Invocation:
*
*        status = tcp_read (data_point, timeout, num_bytes_to_read,
*                          buffer, &num_bytes_read);
*
*    where
*
* @param0 <data_point>       - I
*            is the endpoint handle returned by tcp_answer() or tcp_call().
* @param1 <timeout>     - I
*            specifies the maximum amount of time (in seconds) that the
*            caller wishes to wait for the desired amount of input to be
*            read.  A fractional time can be specified; e.g., 2.5 seconds.
*            A negative timeout (e.g., -1.0) causes an infinite wait;
*            a zero timeout (0.0) allows a read only if input is
*            immediately available.
* @param2 <num_bytes_to_read>  - I
*            has two different meanings depending on its sign.  (1) If the
*            number of bytes to read is positive, tcp_read() will continue
*            to read input until it has accumulated the exact number of bytes
*            requested.  If the timeout interval expires before the requested
*            number of bytes has been read, then tcp_read() returns with an
*            EWOULDBLOCK status.  (2) If the number of bytes to read is
*            negative, tcp_read() returns after reading the first "chunk"
*            of input received; the number of bytes read from that first
*            "chunk" is limited by the absolute value of num_bytes_to_read.
*            A normal status (0) is returned if the first "chunk" of input
*            is received before the timeout interval expires; EWOULDBLOCK
*            is returned if no input is received within that interval.
* @param2 <buffer>      - O
*            receives the input data.  This buffer should be at least
*            num_bytes_to_read in size.
* @param3 <num_bytes_read>    - O
*            returns the actual number of bytes read.  If an infinite wait
*            was specified (TIMEOUT < 0.0), then this number should equal
*            (the absolute value of) num_bytes_to_read.  If a finite wait
*            was specified, the number of bytes read may be less than the
*            number requested.
* @return <status>      - O
*            returns the status of reading from the network connection:
*            zero if no errors occurred, EWOULDBLOCK if the timeout
*            interval expired before the requested amount of data was
*            input, and ERRNO otherwise.  (See the num_bytes_to_read
*            argument for a description of how that argument affects
*            the returned status code.)
*
*/
int tcp_read (tcp_end_point data_point, double timeout, int num_bytes_to_read,
             char  *buffer, int  *num_bytes_read)
{ 
    static char mod[] = "tcp_read";
    fd_set  read_mask;
    int  firstInputOnly, length, num_active;
    struct  timeval  delta_time, expiration_time;
  
    DEBUG_OUT(tcp_util_db_level, 
              ("(%s) socket=%d num_bytes_to_read=%d buffer=%p timeout=%02f\n", 
               mod, tcp_fd(data_point), num_bytes_to_read, buffer, timeout));

    if (data_point->fd < 0) {
        DEBUG_OUT(0, ("(%s) Bad socket descriptor: %d\n", mod, data_point->fd));
        errno = EBADF;
        return EBADF;      
    }
  
    /* If a timeout interval was specified, then compute the expiration time
       of the interval as the current time plus the interval. */
    if (timeout >= 0.0)
        expiration_time = tv_add (tv_tod (), tv_createf (timeout));
  
    /*
     *   While the timeout interval has not expired, attempt to read the
     *   requested amount of data from the network connection.
     */
      firstInputOnly = (num_bytes_to_read < 0);
    if (firstInputOnly)  num_bytes_to_read = abs (num_bytes_to_read);
    if (num_bytes_read != NULL)  *num_bytes_read = 0;
  
    while (num_bytes_to_read > 0) {
          /* Wait for the next chunk of input to arrive. */
          for (;; ) {
            if (timeout >= 0.0)
                delta_time = tv_subtract (expiration_time, tv_tod ());
            FD_ZERO (&read_mask);  FD_SET (data_point->fd, &read_mask);
            num_active = select (data_point->fd+1, &read_mask, NULL, NULL,
                                 (timeout < 0.0) ? NULL : &delta_time);
      
            if (num_active >= 0)  break;
            if (errno == EINTR)  continue;
            DEBUG_OUT(0, ("(%s) Error waiting for input on socket %d.\nselect: ",
                          mod, data_point->fd));
            return (errno);
        }
    
        if (num_active == 0) {
            errno = EWOULDBLOCK;
            DEBUG_OUT(2, ( "(%s) Timeout while waiting for input on socket %d.\n",
                           mod, data_point->fd) );
            return (errno);
        }
    
        /* Read the available input. */
        length = read (data_point->fd, buffer, num_bytes_to_read);
        if (length < 0) {
            DEBUG_OUT(0, ( "(%s) Error reading from connection %d.\nread: ",
                           mod, data_point->fd) );
            return (errno);
        } else if (length == 0) {
            DEBUG_OUT(0, ("(%s) Broken connection on socket %d.\nread: ",
                          mod, data_point->fd));
            errno = EPIPE;
            return (errno);
        }
    
        DEBUG_OUT(3, ("(%s) Read %d bytes from %s, socket %d.\n",
                      mod, length, data_point->name, data_point->fd));
    
        if (tcp_util_debug) MEO_DUMPX (stdout, "    ", 0, buffer, length);
    
        if (num_bytes_read != NULL)  *num_bytes_read += length;
        num_bytes_to_read -= length;
        buffer += length;
    
        if (firstInputOnly)
            break;
    }
    /* NOTE: dkehn only remove for debugging
      printf("(%s) Return num_bytes_to_read=%d:%d buffer=%p devPtrs=%p\n", 
    	      mod, num_bytes_to_read,*num_bytes_read, buffer, getdevPtrs(mod));
    */
    return (0);
}

/*
 * @brief tcp_readv ()
 *    Read Data from a Network Connection using the scatter/gather method.
 *
 * @brief Function tcp_read() reads data from a network connection.  Because of
 *    the way network I/O works, a single record written to a connection
 *    by one task may be read in multiple "chunks" by the task at the other
 *    end of the connection.  For example, a 100-byte record written by a
 *    client may be read by the server in two chunks, one of 43 bytes and
 *    the other of 57 bytes.  tcp_read() takes this into account and, if
 *    you ask it for 100 bytes, it will automatically perform however many
 *    network reads are necessary to collect the 100 bytes.  You can also
 *    ask tcp_read() to return the first chunk received, whatever its length;
 *    see the num_bytes_to_read argument.
 *
 *    A timeout can be specified that limits how long tcp_read() waits for
 *    the desired amount of data to be received.  When the timeout interval
 *    expires, tcp_read() will return whatever input has been received - which
 *    may be less than you requested - during that interval.
 *
 *    Invocation:
 *        status = tcp_read (data_point, timeout, num_bytes_to_read,
 *                          buffer, &num_bytes_read);
 *
 *    where
 * @param0 <data_point>       - I
 *            is the endpoint handle returned by tcp_answer() or tcp_call().
 * @param1 <timeout>     - I
 *            specifies the maximum amount of time (in seconds) that the
 *            caller wishes to wait for the desired amount of input to be
 *            read.  A fractional time can be specified; e.g., 2.5 seconds.
 *            A negative timeout (e.g., -1.0) causes an infinite wait;
 *            a zero timeout (0.0) allows a read only if input is
 *            immediately available.
 * @param2 <num_bytes_to_read>  - I
 *            has two different meanings depending on its sign.  (1) If the
 *            number of bytes to read is positive, tcp_read() will continue
 *            to read input until it has accumulated the exact number of bytes
 *            requested.  If the timeout interval expires before the requested
 *            number of bytes has been read, then tcp_read() returns with an
 *            EWOULDBLOCK status.  (2) If the number of bytes to read is
 *            negative, tcp_read() returns after reading the first "chunk"
 *            of input received; the number of bytes read from that first
 *            "chunk" is limited by the absolute value of num_bytes_to_read.
 *            A normal status (0) is returned if the first "chunk" of input
 *            is received before the timeout interval expires; EWOULDBLOCK
 *            is returned if no input is received within that interval.
 * @param2 <iov>      - I
 *            pointer to iovec structure.  There should be enough iovec to 
 *            support the number for bytes bening asked for.
 * @param3 <iovcount> - I
 *            number of iovecs
 * @param4 <num_bytes_read>    - O
 *            returns the actual number of bytes read.  If an infinite wait
 *            was specified (TIMEOUT < 0.0), then this number should equal
 *            (the absolute value of) num_bytes_to_read.  If a finite wait
 *            was specified, the number of bytes read may be less than the
 *            number requested.
 * @return <status>      - O
 *            returns the status of reading from the network connection:
 *            zero if no errors occurred, EWOULDBLOCK if the timeout
 *            interval expired before the requested amount of data was
 *            input, and ERRNO otherwise.  (See the num_bytes_to_read
 *            argument for a description of how that argument affects
 *            the returned status code.)
 *
 */
int tcp_readv (tcp_end_point data_point, double timeout,  struct iovec *iov, 
              int iovCount, int  *num_bytes_read)
{
    static char mod[] = "tcp_readv";
    fd_set  read_mask;
    struct iovec *vp;
    int vCnt;
    int  firstInputOnly, length, num_active;
    struct  timeval  delta_time, expiration_time;
    int num_bytes_to_read = 0, currentRead;
    int a,b;
    int sav_idx = 0;
    struct iovec savvec;
    bool sav_active = False;
    
    savvec.iov_base = NULL;
    savvec.iov_len = 0;
  
    DEBUG_OUT(tcp_util_db_level, 
              ("%s: socket=%d Called iov=%p iovCount=%d timeout=%02f\n", 
               mod, data_point->fd, iov, iovCount, timeout)); 
  
    if (DEBUGLEVEL > 3) {
        vp = iov;
        DEBUG_OUT(tcp_util_db_level, ("====iovec dump\n"));
        for (b=0; b < iovCount; b++) {
            DEBUG_OUT(tcp_util_db_level, 
                      ("%d iovec=%p: iov_base=%p iov_len=%ld\n", 
                       b, vp, vp->iov_base, vp->iov_len));
            vp++;
        }
    }

    if (data_point->fd < 0) {
        DEBUG_OUT(0, ("(%s) Bad socket descriptor: %d\n", mod, data_point->fd));
        errno = EBADF;
        return EBADF;      
    }

    for (a=0, vp=iov; a < iovCount; a++,vp++) 
        num_bytes_to_read += vp->iov_len;
  
    vp = iov;
  
    /* If a timeout interval was specified, then compute the expiration time
       of the interval as the current time plus the interval. */
    if (timeout >= 0.0)
        expiration_time = tv_add (tv_tod (), tv_createf (timeout));

    /*
     *   While the timeout interval has not expired, attempt to read the
     *   requested amount of data from the network connection.
     */
    firstInputOnly = (num_bytes_to_read < 0);
    if (firstInputOnly)  num_bytes_to_read = abs (num_bytes_to_read);
    if (num_bytes_read != NULL)  *num_bytes_read = 0;
    vCnt = iovCount;
  
    while (num_bytes_to_read > 0) {
        /* Wait for the next chunk of input to arrive. */
        for (;; ) {
            if (timeout >= 0.0)
                delta_time = tv_subtract (expiration_time, tv_tod ());
            FD_ZERO (&read_mask);  FD_SET (data_point->fd, &read_mask);
            num_active = select (data_point->fd+1, &read_mask, NULL, NULL,
                                 (timeout < 0.0) ? NULL : &delta_time);
            if (num_active >= 0)
                break;
            if (errno == EINTR)
                continue;
            DEBUG_OUT(0, ("%s: Error waiting for input on socket %d.\nselect: ",
                          mod, data_point->fd));
            return (errno);
        }

        if (num_active == 0) {
            errno = EWOULDBLOCK;
            DEBUG_OUT(0, ( "%s: Timeout while waiting for input on socket %d.\n",
                           mod, data_point->fd) );
            return (errno);
        }

        /* Read the available input. */
        length = readv (data_point->fd, vp, vCnt);
    
        /* readv can return a 0 length an it not be an error
         */
        DEBUG_OUT(tcp_util_db_level, 
                  ("*****%s: After readv length = %d sav_active=%d sav_idx=%d vp=%p vCnt=%d\n", 
                   mod, length, sav_active, sav_idx, vp, vCnt));
    
        if (length < 0) {
            DEBUG_OUT(0, 
                      ( "%s: Error reading from connection %d. [%d:%s]\nread:",
                           mod, data_point->fd, errno, strerror(errno)) );
      
            if (DEBUGLEVEL > tcp_util_db_level) {
                vp = iov;
                DEBUG_OUT(tcp_util_db_level, ("====AFTER ERROR iovec dump\n"));
                for (b=0; b < iovCount; b++) {
                    DEBUG_OUT(tcp_util_db_level, 
                              ("%d iovec=%p: iov_base=%p iov_len=%ld\n", 
                               b, vp, vp->iov_base, vp->iov_len));
                    vp++;
                }
            }
            return (errno);
        } 
        else if (length == 0) {
            DEBUG_OUT(0, ("%s: Zero read data in the pipe[%d] = %d\n ",
                          mod, data_point->fd, 
                          skt_is_readable(tcp_name(data_point), 
                                        tcp_fd(data_point))));
      
            if (DEBUGLEVEL > tcp_util_db_level) {
                vp = iov;
                for (b=0; b < iovCount; b++) {
                    DEBUG_OUT(tcp_util_db_level, ("%d iovec=%p: iov_base=%p iov_len=%ld\n", 
                                                  b, vp, vp->iov_base, vp->iov_len));
                    vp++;
                }
            }
            exit(0);
        }
    
        DEBUG_OUT(tcp_util_db_level, ("%s: Read %d bytes from %s, socket %d.\n",
                                      mod, length, data_point->name, data_point->fd));
        /* will try dum whats been read */
        if (tcp_util_debug) {
            struct iovec *lpV;
            int dCnt = 0;
      
            for (a=0,lpV=vp; a > 0; a--, lpV--) {
                dCnt += lpV->iov_len;
                if (dCnt <= length) 
                    MEO_DUMPX (stdout, "    ", 0, lpV->iov_base, lpV->iov_len);
            }
        }
    
        if (num_bytes_read != NULL)  
            *num_bytes_read += length;
        num_bytes_to_read -= length;
        //buffer += length;
    
        /* check if we got it all, If not we need to rearrange the 
         * iov such that we be good for the next read */
        if (num_bytes_to_read > 0) {
            /* if we have already modified one of the vecs 
             * replace the original one back and figure out 
             * how much we have sent.
             */
            if (sav_active) {
                (iov+sav_idx)->iov_base = savvec.iov_base;
                (iov+sav_idx)->iov_len = savvec.iov_len;
                sav_active = False;
            }
      
            currentRead=*num_bytes_read;
            vp=iov;
            sav_idx = 0;
            vCnt = iovCount;
            while (currentRead > 0) {
                if ((currentRead -= vp->iov_len) < 0) {
                    break;
                }
                vCnt--;
                vp++;
                sav_idx++;
            }	   
      
            /* save the original info */
            if (currentRead != 0) {
                sav_active = True;
                savvec.iov_base = vp->iov_base;
                savvec.iov_len  = vp->iov_len;
	
                vp->iov_base = (vp->iov_base + vp->iov_len) - abs(currentRead);
                vp->iov_len = abs(currentRead);
            }
        }
    
        if (firstInputOnly)
            break;
    }
  
    /* restore the one that we modified if we got any partials */
    if (sav_active == True) {
        (iov+sav_idx)->iov_base = savvec.iov_base;
        (iov+sav_idx)->iov_len = savvec.iov_len;
    }

    if (DEBUGLEVEL > tcp_util_db_level) {
        vp = iov;
        DEBUG_OUT(tcp_util_db_level, ("====iovec dump\n"));
        for (b=0; b < iovCount; b++) {
            DEBUG_OUT(tcp_util_db_level, ("%d iovec=%p: iov_base=%p iov_len=%ld\n", 
                                          b, vp, vp->iov_base, vp->iov_len));
            vp++;
        }
    }
    return (0);
}

/*
* @brief tcp_write () 
*    Write Data to a Network Connection.
*
* @remarks
*    Function tcp_write() writes data to a network connection.  Because
*    of the way network I/O works, attempting to output a given amount
*    of data to a network connection may require multiple system
*    WRITE(2)s.  For example, when called to output 100 bytes of data,
*    WRITE(2) may return after only outputting 64 bytes of data; the
*    application is responsible for re-calling WRITE(2) to output the
*    other 36 bytes.  tcp_write() takes this into account and, if you
*    ask it to output 100 bytes, it will call WRITE(2) as many times
*    as necessary to output the full 100 bytes of data to the connection.
*
*    A timeout interval can be specified that limits how long tcp_write()
*    waits to output the desired amount of data.  If the timeout interval
*    expires before all the data has been written, tcp_write() returns the
*    number of bytes actually output in the num_bytyes_written argument.
*
*
*    Invocation:
*        status = tcp_write (data_point, timeout, numBytesToWrite, buffer,
*                           &num_bytyes_written);
*
*    where
*        <data_point>       - I
*            is the endpoint handle returned by tcp_answer() or tcp_call().
*        <timeout>     - I
*            specifies the maximum amount of time (in seconds) that the
*            caller wishes to wait for the data to be output.  A fractional
*            time can be specified; e.g., 2.5 seconds.   A negative timeout
*            (e.g., -1.0) causes an infinite wait; tcp_write() will wait as
*            long as necessary to output all of the data.  A zero timeout
*            (0.0) specifies no wait: if the socket is not ready for writing,
*            tcp_write() returns immediately; if the socket is ready for
*            writing, tcp_write() returns after outputting whatever it can.
*        <numBytesToWrite> - I
*            is the number of bytes to write.  If the timeout argument
*            indicates an infinite wait, tcp_write() won't return until
*            it has output the entire buffer of data.  If the timeout
*            argument is greater than or equal to zero, tcp_write() will
*            output as much as it can in the specified time interval,
*            up to a maximum of numBytesToWrite.
*        <buffer>      - O
*            is the data to be output.
*        <num_bytyes_written> - O
*            returns the actual number of bytes written.  If an infinite wait
*            was specified (TIMEOUT < 0.0), then this number should equal
*            numBytesToWrite.  If a finite wait was specified, the number
*            of bytes written may be less than the number requested.
*        <status>      - O
*            returns the status of writing to the network connection:
*            zero if no errors occurred, EWOULDBLOCK if the timeout
*            interval expired before the entire buffer of data was
*            output, and ERRNO otherwise.
*
*/
int tcp_write (tcp_end_point  data_point, double timeout, int numBytesToWrite,
              const  char  *buffer, int  *num_bytyes_written)
{ 
    static char mod[] = "tcp_write";
    fd_set writeMask;
    int length, num_active;
    struct timeval delta_time, expiration_time;
  
    DEBUG_OUT(tcp_util_db_level, ("%s: socket=%dCalled buffer=%p bytes=%d\n",
                                  mod, tcp_fd(data_point), buffer,
                                  numBytesToWrite)); 
  
    if (data_point->fd < 0) {
        DEBUG_OUT(0, ("%s: Bad socket descriptor: %d\n", mod, data_point->fd));
        errno = EBADF;
        return EBADF;      
    }
 
    /* If a timeout interval was specified, then compute the expiration time
       of the interval as the current time plus the interval. */
    if (timeout >= 0.0)
        expiration_time = tv_add (tv_tod (), tv_createf (timeout));
  
     /*
      *  While the timeout interval has not expired, attempt to write the entire
      *  buffer of data to the network connection.
      */
    if (num_bytyes_written != NULL)
        *num_bytyes_written = 0;
  
    while (numBytesToWrite > 0) {
        /* Wait for the connection to be ready for writing. */
        for (;; ) {
            if (timeout >= 0.0)
                delta_time = tv_subtract (expiration_time, tv_tod ());
            FD_ZERO (&writeMask);  FD_SET (data_point->fd, &writeMask);
            num_active = select (data_point->fd+1, NULL, &writeMask, NULL,
                                 (timeout < 0.0) ? NULL : &delta_time);
            if (num_active >= 0)  break;
            if (errno == EINTR)  continue;
            DEBUG_OUT(0, ("%s: Error waiting to write data to %s.\nselect: ",
                          mod, data_point->name) );
            return (errno);
        }
    
        if (num_active == 0) {
            errno = EWOULDBLOCK;
            DEBUG_OUT(0, ("%s: Timeout while waiting to write data to %s.\n",
                          mod, data_point->name));
            return (errno);
        }

        /* Write the next chunk of data to the network. */
        length = write (data_point->fd, (char *) buffer, numBytesToWrite);
        if (length < 0) {
            DEBUG_OUT(0, ( "%s: Error writing to %s.\nwrite: ",
                           mod, data_point->name));
            return (errno);
        }

        DEBUG_OUT(tcp_util_db_level, ("%s: Wrote %d bytes to %s, socket %d.\n",
                                      mod, length, data_point->name, data_point->fd));
        if (tcp_util_debug)
            MEO_DUMPX (stdout, "    ", 0, buffer, length);

        if (num_bytyes_written != NULL)  *num_bytyes_written += length;
        numBytesToWrite -= length;
        buffer += length;
    }
    return (0);
}

/*
* @brief tcp_writev () 
*    Write Data to a Network Connection using the scatter/gather method.
*
* @remarks Purpose:
*    Function tcp_write() writes data to a network connection.  Because
*    of the way network I/O works, attempting to output a given amount
*    of data to a network connection may require multiple system
*    WRITE(2)s.  For example, when called to output 100 bytes of data,
*    WRITE(2) may return after only outputting 64 bytes of data; the
*    application is responsible for re-calling WRITE(2) to output the
*    other 36 bytes.  tcp_write() takes this into account and, if you
*    ask it to output 100 bytes, it will call WRITE(2) as many times
*    as necessary to output the full 100 bytes of data to the connection.
*
*    A timeout interval can be specified that limits how long tcp_write()
*    waits to output the desired amount of data.  If the timeout interval
*    expires before all the data has been written, tcp_write() returns the
*    number of bytes actually output in the num_bytyes_written argument.
*
*
*    Invocation:
*        status = tcp_writev (data_point, timeout, iov, iovcount, &num_bytyes_written);
*
*    where
* @param0 <data_point>       - I
*            is the endpoint handle returned by tcp_answer() or tcp_call().
* @param1 <timeout>     - I
*            specifies the maximum amount of time (in seconds) that the
*            caller wishes to wait for the data to be output.  A fractional
*            time can be specified; e.g., 2.5 seconds.   A negative timeout
*            (e.g., -1.0) causes an infinite wait; tcp_write() will wait as
*            long as necessary to output all of the data.  A zero timeout
*            (0.0) specifies no wait: if the socket is not ready for writing,
*            tcp_write() returns immediately; if the socket is ready for
*            writing, tcp_write() returns after outputting whatever it can.
* @param2 <iov> - I
*            is the vector of struct iovecs.  If the timeout argument
*            indicates an infinite wait, tcp_writev() won't return until
*            it has output the entire vector array of data.  If the timeout
*            argument is greater than or equal to zero, tcp_writev() will
*            output as much as it can in the specified time interval,
*            up to a maximum of iovcount.
* @param3  <iovcouont>      - I
*            the number of vectors represented by the iov array
* @param4 <num_bytyes_written> - O
*            returns the actual number of bytes written.  If an infinite wait
*            was specified (TIMEOUT < 0.0), then this number should equal
*            the iovcount * the iov[*].iov_lene.  If a finite wait was 
*            specified, the number of bytes written may be less than the 
*            number requested.
* @return <status>      - O
*            returns the status of writing to the network connection:
*            zero if no errors occurred, EWOULDBLOCK if the timeout
*            interval expired before the entire buffer of data was
*            output, and ERRNO otherwise.
*
*/
int tcp_writev (tcp_end_point data_point,  double timeout,  struct iovec *iov,
               int iovcount, int *num_bytyes_written)
{    
    static char mod[] = "tcp_writev";
    fd_set  writeMask;
    int  length, num_active;
    struct  timeval  delta_time, expiration_time;
    int numBytesToWrite = 0, currentWrite;
    int a,b;
    struct iovec *vp = NULL;
    struct iovec savvec;
    bool sav_active = False;
    int sav_idx = 0, vCnt;
    int iVecCnt = 0;

    savvec.iov_base = NULL;
    savvec.iov_len = 0;

    DEBUG_OUT(tcp_util_db_level, ("%s: socket=%d Called iov=%p iovcount=%d\n",
                                  mod, data_point->fd, iov, iovcount)); 

    if (data_point->fd < 0) {
        DEBUG_OUT(0, ("%s: Bad socket descriptor: %d\n", mod, data_point->fd));
        errno = EBADF;
        return EBADF;      
    }

    //  if (DEBUGLEVEL > tcp_util_db_level) {
    if (DEBUGLEVEL > 3) {
        vp = iov;
        DEBUG_OUT(tcp_util_db_level, ("====iovec dump\n"));
        for (b=0; b < iovcount; b++) {
            DEBUG_OUT(tcp_util_db_level,
                      ("%d iovec=%p: iov_base=%p iov_len=%ld\n",
                       b, vp, vp->iov_base, vp->iov_len));
            vp++;
        }
    }

    /* determine the number of bytes to write */
    for (a=0,vp=iov; a < iovcount; a++,vp++)
        numBytesToWrite += vp->iov_len;
  
    /* If a timeout interval was specified, then compute the expiration time
       of the interval as the current time plus the interval. */
    if (timeout >= 0.0)
        expiration_time = tv_add (tv_tod(), tv_createf(timeout));
  
    /*
     * While the timeout interval has not expired, attempt to write the entire
     * buffer of data to the network connection.
     */
    if (num_bytyes_written != NULL)
        *num_bytyes_written = 0;

    vp = iov;
    iVecCnt = iovcount;

    while (numBytesToWrite > 0) {
        /* Wait for the connection to be ready for writing. */
        for (;; ) {
            if (timeout >= 0.0)
                delta_time = tv_subtract(expiration_time, tv_tod());
            FD_ZERO (&writeMask);  FD_SET (data_point->fd, &writeMask);
            num_active = select (data_point->fd+1, NULL, &writeMask, NULL,
                                 (timeout < 0.0) ? NULL : &delta_time);
            if (num_active >= 0)  break;
            if (errno == EINTR)  continue;
            DEBUG_OUT(0, ("%s: Error waiting to write data to %s.\nselect: ",
                          mod, data_point->name) );
            return (errno);
        }
    
        if (num_active == 0) {
            errno = EWOULDBLOCK;
            DEBUG_OUT(0, ("%s: Timeout while waiting to write data to %s.\n",
                          mod, data_point->name));
            return (errno);
        }
    
        /* Write the next chunk of data to the network. */
        length = writev (data_point->fd, vp, iVecCnt);

        if (length < 0) {
            DEBUG_OUT(0, ( "%s: Error writing to %s.\nwrite: ",
                           mod, data_point->name));
            perror("ERROR:");
            return (errno);
        }
    
        DEBUG_OUT(tcp_util_db_level, 
                  ("%s: Wrote %d bytes to %s, socket %d. in %d iovec blocks\n",
                   mod, length, data_point->name, data_point->fd, iovcount));
        if (tcp_util_debug) {
            struct iovec *lpV;
            int dCnt = length;
      
            for (a=0,lpV=vp; a <iovcount; a++, lpV++) {
                dCnt -= lpV->iov_len;
                DEBUG_OUT(tcp_util_db_level, (" ----------- iovec block = %d len=%ld\n", 
                                              a,lpV->iov_len ));
                if (dCnt >= 0) {
                    MEO_DUMPX (stdout, "    ", 0, lpV->iov_base, lpV->iov_len);	    
                }
                else {
                    MEO_DUMPX (stdout, "    ", 0, lpV->iov_base, abs(dCnt));
                }
            }
        }

        if (num_bytyes_written != NULL)  
            *num_bytyes_written += length;

        numBytesToWrite -= length;
    
        /* check to see if we need to modify the vectors for another 
         * write to the net.
         */
        if (numBytesToWrite > 0) {
            /* if we are in here for something other 
             * than the 1st time restore the saved iov element */
            if (sav_active) {
                (iov+sav_idx)->iov_base = savvec.iov_base;
                (iov+sav_idx)->iov_len = savvec.iov_len;
                sav_active = False;
            }
      
            /* update the vector ptr in the place where 
             * it has stopped and retry it. 
             */
            currentWrite=*num_bytyes_written;
            vp=iov;
            sav_idx = 0;
            vCnt = iovcount;
            while (currentWrite > 0) {
                if ((currentWrite -= vp->iov_len) < 0) {
                    break;
                }
                vCnt--;
                vp++;
                sav_idx++;
            }	   
      
            /* save the original info */
            if (currentWrite != 0) {
                sav_active = True;
                savvec.iov_base = vp->iov_base;
                savvec.iov_len  = vp->iov_len;
	
                vp->iov_base = (vp->iov_base + vp->iov_len) - abs(currentWrite);
                vp->iov_len = abs(currentWrite);
            }
        }    
    }
    /* see if we need to change the vector back... */
    if (sav_active == True) {
        (iov+sav_idx)->iov_base = savvec.iov_base;
        (iov+sav_idx)->iov_len = savvec.iov_len;
    }
    return (0);
}

/*
*
*Procedure:
*
* @brief tcp_destroy ()
*    Close a Network Endpoint.
*
*
*Purpose:
*
* @remarks Function tcp_destroy() deletes listening endpoints created by tcp_listen()
*    and data endpoints created by tcp_answer() or tcp_call().
*
*
*    Invocation:
*
*        status = tcp_destroy (endpoint);
*
*    where
*
* @param0 <endpoint>    - I
*            is the endpoint handle returned by tcp_listen(), tcp_answer(),
*            or tcp_call().
* @return <status>  - O
*            returns the status of deleting the endpoint, zero if there were
*            no errors and ERRNO otherwise.
*
*/
int tcp_destroy (tcp_end_point endpoint)
{
    static char mod[] = "tcp_destroy";
    if (endpoint == NULL)
        return (0);
    if (endpoint->fd == -1)
        return(0);
  
    DEBUG_OUT(tcp_util_db_level, ("%s: Closing %s, socket=%d\n",
                                  mod, endpoint->name, endpoint->fd));
  
    /* Close the endpoint's socket. */
    if (endpoint->fd >= 0) {
        DEBUG_OUT(tcp_util_db_level,
                  ("%s: Issueing the Close on %s, socket %d.\n",
                   mod, endpoint->name, endpoint->fd));
        close (endpoint->fd);
        endpoint->fd = -1;
    }

    /* Deallocate the endpoint structure. */
    if (endpoint->name != NULL) {
        DEBUG_OUT(MEM_DB_LEVEL, ("%s: ----freeing ptr->%p\n",
                                 mod, (endpoint)->name));

        /* if its a LOCAL connection then destroy the file reference */
        if (endpoint->family == AF_UNIX && endpoint->type == TCP_LISTENING_POINT) {
            unlink(endpoint->name);
            remove(endpoint->name);
        }

        free (endpoint->name);
        endpoint->name = NULL;
    }

    DEBUG_OUT(MEM_DB_LEVEL, ("%s: ----freeing ptr->%p\n",
                             mod, endpoint));
    free (endpoint);
    return (0);
}

/*
 * @brief tcp_fd () Get an device's descriptor
 *
 * @remark Function tcp_fd() returns a listening or data endpoint's socket.
 *
 *    Invocation:
 *        fd = tcp_fd (endpoint);
 *
 *    where
 * @param0 <endpoint>    - I
 *            is the endpoint handle returned by tcp_listen(), tcp_answer(),
 *            or tcp_call().
 * @return <fd>      - O
 *            returns the UNIX file descriptor for the endpoint's socket.
 *
 */
int tcp_fd (tcp_end_point  endpoint)
{
    return ((endpoint == NULL) ? -1 : endpoint->fd);
}

/*
 * @brief tcp_name ()
 *    Get an Endpoint's Name.
 *
 * @remark Function tcp_name() returns a listening or data endpoint's name.
 *
 *    Invocation:
 *        name = tcp_name (endpoint);
 *
 *    where
 * @param0 <endpoint>    - I
 *            is the endpoint handle returned by tcp_listen(), tcp_answer(),
 *            or tcp_call().
 * @return <name>        - O
 *            returns the endpoint's name.  The name is stored in memory local
 *            to the TCP utilities and it should not be modified or freed
 *            by the caller.
 *
 */
inline char *tcp_name (tcp_end_point endpoint)
{
    if (endpoint == NULL)
        return ("");
    if (endpoint->name == NULL)
        return ("");
    return (endpoint->name);
}

/*
* @brief tcp_client_addr_str ()
*    Get an Endpoint's ipaddr in dot format.
*
*Purpose:
* @remark Function tcp_client_addr_str() returns the end point's ip address in dot
*          format.
*
*    Invocation:
*        name = tcp_client_addr_str (endpoint);
*
*    where
* @param0 <endpoint>    - I
*            is the endpoint handle returned by tcp_listen(), tcp_answer(),
*            or tcp_call().
* @param1 <buf>         - O
*            buffer to be used for returning the ipaddr.           
* @return <retc>        - O
*            0 address is in the string. else, no.
*/
int tcp_client_addr_str ( tcp_end_point endpoint, char *ipaddr) 
{
    static char mod[] = "tcp_client_addr_str";
    int fd;
    struct sockaddr sa;
    struct sockaddr_in *sockin = (struct sockaddr_in *) (&sa);
    socklen_t length = sizeof(sa);

    if (endpoint == NULL)  
        return EFAULT;
    if (ipaddr == NULL)
        return ENOMEM;

    fd = endpoint->fd;
    strcpy(ipaddr,"0.0.0.0");

    if (fd == -1) {
        return EBADFD;
    }

    if (getpeername(fd, &sa, &length) < 0) {
        DEBUG_OUT(0,("%s: getpeername failed. sockdesc=%d.  Error was %s\n",
                     mod, fd, strerror(errno) ));
        return errno;
    }

    strcpy(ipaddr,(char *)inet_ntoa(sockin->sin_addr));
    return 0;
}
