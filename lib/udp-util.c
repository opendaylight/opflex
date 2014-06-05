/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

/*
 *    This implements a high-level interface to UDP/IP
 *    network communications.  Under the UDP protocol, processes send
 *    "datagrams" to each other.  Unlike TCP, UDP has no concept of a
 *    "connection"; messages are individually routed to one or more
 *    destination endpoints through a single socket.  Also unlike TCP,
 *    UDP does not guarantee reliable transmission; messages may be
 *    lost or sent out of order.
 *
 *    The endpoints of a connection-less UDP
 *    "connection" are called, well, "endpoints".  A given program may
 *    create an anonymous UDP endpoint bound to a system-assigned network
 *    port:
 *
 *        udp_create (NULL, NULL, &endpoint);
 *
 *    or it may create a UDP endpoint bound to a predetermined network
 *    port (specified by name or port number):
 *
 *        udp_create ("<name>", NULL, &endpoint);
 *
 *    Client processes generally create anonymous UDP endpoints for the
 *    purpose of sending messages to a server process at a predetermined
 *    network port.  With an anonymous endpoint, a client must let the
 *    server(s) know its port number before the client can receive messages
 *    from the server(s).  The act of sending a datagram to the server(s)
 *    automatically supplies the server(s) with the client's port number
 *    and IP address.
 *
 *    By creating a UDP endpoint bound to a predetermined network port, a
 *    server is immediately ready to receive datagrams sent by clients to
 *    that port; the clients already know the port number, so there is no
 *    need for the server to send messages first.
 *
 *    To send a datagram from one endpoint to another, you must specify the
 *    network address of the destination endpoint.  Since the destination
 *    endpoint probably belongs to another process, possibly on a remote host,
 *    the UDP_UTIL package requires you to create a "proxy" endpoint for the
 *    destination endpoint:
 *
 *        udp_create ("<name>[@<host>]", source, &destination);
 *
 *    A proxy endpoint simply specifies the network address of the destination
 *    endpoint; the proxy endpoint is not bound to a network port and it has
 *    no operating system socket associated with it.  The proxy endpoint is
 *    internally linked with its source endpoint, so, when a datagram is sent
 *    to the proxy, udp_write() automatically sends the datagram through the
 *    source endpoint to the destination.  A source endpoint may have many
 *    proxy endpoints, but a given proxy endpoint is only linked to a single
 *    source.  (If you have multiple source endpoints, you can create multiple
 *    proxy endpoints for the same destination.)
 *
 *    When a datagram is read from an anonymous or predetermined endpoint,
 *    udp_read() returns the text of the datagram and a proxy endpoint for
 *    the sender of the datagram.  The proxy endpoint can be used to return
 *    a response to the sender:
 *
 *        char  message[64], response[32];
 *        udp_endpoint  me, you;
 *        ...
 *						-- Read message.
 *        udp_read (me, -1.0, sizeof message, message, NULL, &you);
 *						-- Send response.
 *        udp_write (you, -1.0, sizeof response, response);
 *
 *    Although there is no harm in doing so, there is no need to delete proxy
 *    endpoints; they are automatically garbage-collected when their source
 *    endpoint is deleted.
 *
 *    The following is a very simple server process that creates a UDP
 *    endpoint bound to a predetermined network port and then reads and
 *    displays messages received from clients:
 *
 *        #include  <stdio.h>			-- Standard I/O definitions.
 *        #include  "udp_util.h"			-- UDP utilities.
 *
 *        main (int argc, char *argv[])
 *        {
 *            char  buffer[128];
 *            udp_endpoint  client, server;
 *						-- Create UDP endpoint.
 *            udp_create ("<name>", NULL, &server);
 *
 *            for (;; ) {			-- Read and display messages.
 *                udp_read (server, -1.0, 128, buffer, NULL, &client);
 *                printf ("From %s: %s\n", udp_name (client), buffer);
 *            }
 *
 *        }
 *
 *    The following client process creates an anonymous UDP endpoint and
 *    sends 16 messages through that endpoint to the server:
 *
 *        #include  <stdio.h>			-- Standard I/O definitions.
 *        #include  "udp_util.h"			-- UDP utilities.
 *
 *        main (int argc, char *argv[])
 *        {
 *            char  buffer[128];
 *            int  i;
 *            udp_endpoint  client, server;
 *
 *            udp_create (NULL, NULL, &client);	-- Create client and target.
 *            udp_create ("<name>[@<host>]", client, &server);
 *
 *            for (i = 0;  i < 16;  i++) {	-- Send messages.
 *                sprintf (buffer, "Hello for the %dth time!", i);
 *                udp_write (server, -1.0, strlen (buffer) + 1, buffer);
 *            }
 *
 *            udp_destroy (client);		-- Deletes client and target.
 *
 *        }
 *
 *    Note that "client" is the anonymous endpoint and "server" is a proxy
 *    for the destination endpoint.
 *
 *Notes:
 *    These functions are reentrant under VxWorks (except for the global
 *    debug flag).
 *
 *Public Procedures (* defined as macros):
 *    udp_create() - creates a UDP endpoint.
 *    udp_destroy() - destroys a UDP endpoint.
 *    udp_fd() - returns the file descriptor for an endpoint's socket.
 *    udp_is_readable() - checks if a datagram is waiting to be read.
 *    udp_is_up() - checks if an endpoint is up.
 *    udp_is_writeable() - checks if a datagram can be written.
 *    udp_name() - returns the name of an endpoint.
 *    udp_read() - reads a datagram.
 *  * udp_set_buf() - changes the sizes of an endpoint's receive and send buffers.
 *    udp_write() - sends a datagram.
 *
 */

#define _BSD_SOURCE  1

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
#include <netdb.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <netinet/in.h>

#include "meo-util.h"
#include "net-util.h"
#include "skt-util.h"
#include "tv-util.h"
#include "debug.h"
#include "udp-util.h"


/*
 *    UDP Endpoints - contain information about local and remote UDP sockets.
 *        A "local" (to the application) endpoint represents a UDP socket on
 *        which the application can receive UDP datagrams.  A "remote" (to
 *        the application) endpoint has no socket and simply specifies the
 *        source address of a datagram received by the application or the
 *        destination address of a datagram being sent by the application.
 *        Socket-less remote endpoints are linked to their "parent" local
 *        endpoint; when the parent is deleted, the children are automatically
 *        deleted.
 */

typedef  struct  _udp_endpoint {
    char *name;			            /* "<port>[@<host>]" */
    struct sockaddr address;		/* Network address and port number. */
    int address_length;		        /* Length (in bytes) of address. */
    int fd;				            /* UDP socket. */
    struct _udp_endpoint *parent;	/* Local parent of remote endpoint. */
    struct _udp_endpoint *next;	/* Link to first child or next sibling. */
}  _udp_endpoint;


int  udp_util_debug = 0;		/* Global debug switch (1/0 = yes/no). */

/*
*    udp_create - Create a UDP Endpoint.
*    The udp_create() function creates any one of three types of UDP endpoints:
*
*        (1) a UDP socket bound to a system-chosen network port,
*
*        (2) a UDP socket bound to a predetermined network port
*            (identified by port name or number), and
*
*        (3) a socket-less UDP endpoint used to specify the target
*            of a datagram being sent.
*
*    The anonymous and predetermined endpoints (#'s 1 and 2 above) should
*    be closed by a call to udp_destroy() when they are no longer needed.
*    Proxy endpoints (#3 above) are linked upon creation to an existing
*    anonymous or predetermined endpoint (i.e., its "parent").  When a
*    parent endpoint is closed, all of its "children" (i.e., the associated
*    proxy endpoints) are automatically deleted.
*
*    Invocation (anonymous endpoint):
*        status = udp_create (NULL, NULL, &endpoint);
*
*    Invocation (predetermined endpoint):
*        status = udp_create (server_name, NULL, &endpoint);
*
*    Invocation (proxy endpoint):
*        status = udp_create (targetName, parent, &endpoint);
*
*    where
*        <server_name>	- I
*            is the server's name.  This is used for determining the port
*            associated with the server (via the system's name/port mappings).
*            You can side-step the system maps and explicitly specify a
*            particular port by passing in a decimal number encoded in ASCII
*            (e.g., "1234" for port 1234).
*        <targetName>	- I
*            is the target's name: "<server>[@<host>]".  The server can be
*            specified as a name or as a port number; see the "server_name"
*            argument description above.  The host, if given, can be
*            specified as a name or as a dotted Internet address.
*        <endpoint>	- O
*            returns a handle for the UDP endpoint.
*        <status>	- O
*            returns the status of creating the UDP endpoint, zero if
*            there were no errors and ERRNO otherwise.
*
*/
int  udp_create (const char *name, udp_endpoint parent, udp_endpoint *endpoint)
{
    static char mod[] = "udp_create";
    char  *s, host_name[MAXHOSTNAMELEN+1], server_name[MAXHOSTNAMELEN+1];
    int  length, port_number;
    struct  sockaddr_in  socket_name;
	
	/*
     *    Construct the endpoint's network address.
     */

	/* Parse the host and server names.  If the host name is not defined
     *	explicitly, it defaults to the local host. 
     */
    s = net_host_of(net_addr_of (NULL));
    if (s == NULL) {
        DEBUG_OUT(0, ("%s: Error getting local host name.\nnet_host_of: ", mod));
        return (errno);
    }
    strcpy (host_name, s);

    if (name == NULL) {				/* Let system choose port #? */
        strcpy (server_name, "0");
    } 
    else {					/* User-specified port #. */
        s = strchr (name, '@');
        if (s == NULL) {			/* "<server>" */
            strcpy (server_name, name);
        }
        else {				/* "<server>@<host>" */
            length = s - name;
            strncpy (server_name, name, length);
            server_name[length] = '\0';
            strcpy (host_name, ++s);
        }
    }

	/* Lookup the port number bound to the server name. */
    port_number = net_port_of (server_name, "udp");
    if (port_number == -1) {
        DEBUG_OUT(0, ("%s: Error getting server entry for %s.\nnet_port_of: ",
                      mod, server_name));
        return (errno);
    }

	/* Set up the network address for the endpoint. */
    memset (&socket_name, '\0', sizeof socket_name);
    socket_name.sin_family = AF_INET;
    socket_name.sin_port = htons (port_number);
    socket_name.sin_addr.s_addr = net_addr_of (host_name);
    if ((long) socket_name.sin_addr.s_addr == -1) {
        DEBUG_OUT(0, ("%s: Error getting host entry for %s.\nnet_addr_of: ",
                      mod, host_name));
        return (errno);
    }

	/*
     *	Create a UDP endpoint structure.
     */
    *endpoint = (udp_endpoint) malloc (sizeof (_udp_endpoint));
    if (*endpoint == NULL) {
        DEBUG_OUT(0, ("%s: Error allocating endpoint structure for %s.\nmalloc: ",
                      mod, server_name));
        return (errno);
    }

    (*endpoint)->name = NULL;
    *((struct sockaddr_in *) &(*endpoint)->address) = socket_name;
    (*endpoint)->address_length = sizeof socket_name;
    (*endpoint)->fd = -1;
    (*endpoint)->parent = parent;
    (*endpoint)->next = NULL;

	/*
     *	If a UDP socket is to be created, then create the socket and bind it to
     *	the specified port number.
     */
    if (parent == NULL) {
        /* Create a socket for the endpoint. */
        (*endpoint)->fd = socket (AF_INET, SOCK_DGRAM, 0);
        if ((*endpoint)->fd < 0) {
            DEBUG_OUT(0, ("%s: Error creating socket for %s.\nsocket: ",
                          mod, server_name));
            udp_destroy (*endpoint);  *endpoint = NULL;
            return (errno);
        }

		/* Bind the network address to the socket. */
        if (bind ((*endpoint)->fd,
                  &(*endpoint)->address, (*endpoint)->address_length)) {
            DEBUG_OUT(0, ("%s: Error binding %s's socket name.\nbind: ",
                          mod, server_name));
            udp_destroy (*endpoint);  *endpoint = NULL;
            return (errno);
        }

        sprintf (server_name, "%d", skt_port((*endpoint)->name,
                                           (*endpoint)->fd));
    }

	/*
     *	Otherwise, if a socket-less, proxy endpoint is being created, then
     *	simply link it to its parent endpoint.
     */
    else {
        (*endpoint)->next = parent->next;
        parent->next = *endpoint;
    }

	/*
     *  Construct the endpoint's name.
     */
      (*endpoint)->name = malloc (strlen (server_name) + 1 +
                                strlen (host_name) + 1);
    if ((*endpoint)->name == NULL) {
        DEBUG_OUT(0, ("%s: Error duplicating server name: %s%c%s\nmalloc: ",
                      mod, server_name,
                      (parent == NULL) ? '#' : '@', host_name));
        udp_destroy (*endpoint);  *endpoint = NULL;
        return (errno);
    }
    sprintf ((*endpoint)->name, "%s%c%s",
             server_name, (parent == NULL) ? '#' : '@', host_name);

    if (udp_util_debug)  printf ("%s: Created %s, socket %d.\n", mod,
                                 (*endpoint)->name, (*endpoint)->fd);

    return (0);	/* Successful completion. */
}

/*
 *    udp_destroy () Destroy a UDP Endpoint.
 *    The udp_destroy() function destroys a UDP endpoint.  If the endpoint
 *    is a socket bound to a network port, the socket is closed.  If the
 *    endpoint is a socket-less, proxy endpoint, the endpoint is simply
 *    unlinked from the socket endpoint (i.e., its "parent") with which
 *    it is associated.
 *
 *    Invocation:
 *        status = udp_destroy (endpoint);
 *
 *    where
 *        <endpoint>	- I
 *            is the endpoint handle returned by udp_create() or udp_read().
 *        <status>	- O
 *            returns the status of destroying the endpoint, zero if there
 *            were no errors and ERRNO otherwise.
 *
 */

int udp_destroy (udp_endpoint endpoint)
{
    static char mod[] = "udp_destroy";
    udp_endpoint  prev;

    if (endpoint == NULL)
        return (0);

    if (udp_util_debug)
        printf ("%s: Destroying %s, socket %d ...\n", mod,
                endpoint->name, endpoint->fd);

	/* If this is a socket endpoint, then delete all of the socket-less proxy
   	**	endpoints associated with the endpoint. 
   	*/
    if (endpoint->parent == NULL) {
        while (endpoint->next != NULL)
            udp_destroy (endpoint->next);
        close (endpoint->fd);
        endpoint->fd = -1;
    }

	/* Otherwise, if this is a socket-less proxy, then unlink the proxy endpoint
   	** 	from its socket endpoint (i.e., its "parent"). 
   	*/
    else {
        for (prev = endpoint->parent;  prev != NULL;  prev = prev->next)
            if (prev->next == endpoint)  break;
        if (prev != NULL)  prev->next = endpoint->next;
    }

	/* Deallocate the endpoint structure. */
    if (endpoint->name != NULL) {
        free (endpoint->name);  endpoint->name = NULL;
    }
    free (endpoint);

    return (0);

}

/*
 *    udp_fd () Get a UDP Endpoint's Socket.
 *
 *    Function udp_fd() returns a UDP endpoint's socket.
 *
 *    Invocation:
 *        fd = udp_fd (endpoint);
 *
 *    where
 *        <endpoint>	- I
 *            is the endpoint handle returned by udp_create() or udp_read().
 *        <fd>		- O
 *            returns the UNIX file descriptor for the endpoint's socket.
 *
 */
int udp_fd (udp_endpoint endpoint)
{
    return ((endpoint == NULL) ? -1 : endpoint->fd);
}

/*
 *    udp_is_readable () Check if Data is Waiting to be Read.
 *
 *    The udp_is_readable() function checks to see if data is waiting to
 *    be read from a UDP endpoint.
 *
 *    Invocation:
 *        isReadable = udp_is_readable (endpoint);
 *
 *    where
 *        <endpoint>	- I
 *            is the endpoint handle returned by udp_create().
 *        <isReadable>	- O
 *            returns true (a non-zero value) if data is available for
 *            reading and false (zero) otherwise.
 *
 */
int udp_is_readable (udp_endpoint endpoint)
{
    if (endpoint == NULL)
        return (0);
    else if (endpoint->parent == NULL)
        return (skt_is_readable (endpoint->name, endpoint->fd));
    else
        return (skt_is_readable (endpoint->name, (endpoint->parent)->fd));

}

/*
 *
 *    udp_is_up () Check if a UDP Endpoint is Up.
 *    The udp_is_up() function checks to see if a UDP endpoint is still up.
 *
 *    Invocation:
 *        isUp = udp_is_up (endpoint);
 *
 *    where
 *        <endpoint>	- I
 *            is the endpoint handle returned by udp_create().
 *        <isUp>		- O
 *            returns true (a non-zero value) if the endpoint is up and
 *            false (zero) otherwise.
 *
 */

int  udp_is_up (udp_endpoint endpoint)
{
    if (endpoint == NULL)
        return (0);
    else if (endpoint->parent == NULL)
        return (skt_is_up (endpoint->name, endpoint->fd));
    else
        return (skt_is_up (endpoint->name, (endpoint->parent)->fd));
}

/*
 *    udp_is_writeable () Check if Data can be Written.
 *    The udp_is_writeable() function checks to see if data can be written
 *    to a UDP endpoint.
 *
 *    Invocation:
 *        isWriteable = udp_is_writeable (endpoint);
 *
 *    where
 *        <endpoint>	- I
 *            is the endpoint handle returned by udp_create().
 *        <isWriteable>	- O
 *            returns true (a non-zero value) if the endpoint is ready
 *            for writing and false (zero) otherwise.
 *
 */
int udp_is_writeable (udp_endpoint endpoint)
{
    if (endpoint == NULL)
        return (0);
    else if (endpoint->parent == NULL)
        return (skt_is_writeable (endpoint->name, endpoint->fd));
    else
        return (skt_is_writeable (endpoint->name, (endpoint->parent)->fd));

}

/*
 *    udp_name () Get a UDP Endpoint's Name.
 *
 *    Invocation:
 *        name = udp_name (endpoint);
 *
 *    where
 *        <endpoint>	- I
 *            is the endpoint handle returned by udp_create() or udp_read().
 *        <name>	- O
 *            returns the endpoint's name.  The name is stored in memory
 *            local to the UDP utilities and it should not be modified or
 *            freed by the caller.
 *
 */
char *udp_name (udp_endpoint endpoint)
{
    if (endpoint == NULL)
        return ("");
    if (endpoint->name == NULL)
        return ("");
    return (endpoint->name);
}

/*
 *    udp_read - reads the next message on a UDP endpoint.  A timeout
 *    can be specified that limits how long udp_read() waits for the message
 *    to be received.
 *
 *    Invocation:
 *        status = udp_read (endpoint, timeout, max_bytes_to_read,
 *                          buffer, &num_bytes_read, &source);
 *
 *    where
 *        <endpoint>		- I
 *            is the endpoint handle returned by udp_create().
 *        <timeout>		- I
 *            specifies the maximum amount of time (in seconds) that the caller
 *            wishes to wait for the next message to be received.  A fractional
 *            time can be specified; e.g., 2.5 seconds.  A negative timeout
 *            (e.g., -1.0) causes an infinite wait; a zero timeout (0.0) allows
 *            a read only if message is immediately available.
 *        <max_bytes_to_read>	- I
 *            specifies the maximum number of bytes to read; i.e., the size of
 *            the message buffer.
 *        <buffer>		- O
 *            receives the input data.  This buffer should be at least
 *            max_bytes_to_read in size.
 *        <num_bytes_read>		- O
 *            returns the actual number of bytes read.
 *        <source>		- O
 *            returns an endpoint handle for the source of the message.
 *        <status>		- O
 *            returns the status of reading from the endpoint: zero if no
 *            errors occurred, EWOULDBLOCK if the timeout interval expired
 *            before a message was received, and ERRNO otherwise.
 *
 */
int udp_read (udp_endpoint endpoint, double timeout, int max_bytes_to_read,
             char *buffer, int *num_bytes_read, udp_endpoint *source)
{
    static char mod[] = "udp_read";
    char *host_name;
    char source_name[PATH_MAX+1];
    fd_set read_mask;
    int fd, length, num_active;
    socklen_t address_length;
    struct sockaddr address;
    struct timeval delta_time, expiration_time;
    udp_endpoint ep, source_point;

    if (endpoint == NULL) {
        errno = EINVAL;
        DEBUG_OUT(0, ("%s: NULL endpoint handle: ", mod));
        return (errno);
    }

    fd = endpoint->fd;
    if (fd < 0) {
        errno = EINVAL;
        DEBUG_OUT(0, ("%s: %d file descriptor: ", mod, fd));
        return (errno);
    }

	/*
     *	If a timeout interval was specified, then wait until the expiration of
     *	the interval for a message to be received.
     */
    if (timeout >= 0.0) {
		/* Compute the expiration time as the current time plus the interval. */
		expiration_time = tv_add (tv_tod (), tv_createf (timeout));

		/* Wait for the next message to arrive. */
        for (;; ) {
            delta_time = tv_subtract (expiration_time, tv_tod ());
            FD_ZERO (&read_mask);  FD_SET (fd, &read_mask);
            num_active = select (fd+1, &read_mask, NULL, NULL, &delta_time);
            if (num_active >= 0)  break;
            if (errno == EINTR)  continue;
            DEBUG_OUT(0, ("%s: Error waiting for input on %s.\nselect: ",
                          mod, endpoint->name));
            return (errno);
        }

        if (num_active == 0) {
            errno = EWOULDBLOCK;
            DEBUG_OUT(0, ("%s: Timeout while waiting for input on %s.\n",
                          mod, endpoint->name));
            return (errno);
        }
    }

	/*
     *	Read the message.
     */
    address_length = sizeof address;
    length = recvfrom (fd, buffer, max_bytes_to_read, 0, &address, &address_length);
    if (length < 0) {
        DEBUG_OUT(0, ("%s: Error reading from %s.\nrecvfrom: ",
                      mod, endpoint->name));
        return (errno);
    }
    else if (length == 0) {
        errno = EPIPE;
        DEBUG_OUT(0, ("%s:Broken connection on %s.\nrecvfrom: ",
                      mod, endpoint->name));
        return (errno);
    }

    if (num_bytes_read != NULL)
        *num_bytes_read = length;

	/*
     *	Create a UDP endpoint for the source of the message.
     */

	/* Check to see if an endpoint already exists for this particular source. */
    for (ep = endpoint->next;  ep != NULL;  ep = ep->next) {
        if ((address_length == ep->address_length) &&
            (memcmp (&address, &ep->address, address_length) == 0))
            break;
    }

	/* If not, create a brand new endpoint.  If so, then use the existing one. */
    if (ep == NULL) {
        host_name = net_host_of (
                        ((struct sockaddr_in *) &address)->sin_addr.s_addr);
        sprintf (source_name, "%d@%s",
                 ntohs (((struct sockaddr_in *) &address)->sin_port),
                 host_name);
        if (udp_create (source_name, endpoint, &source_point)) {
            DEBUG_OUT(0, ("%s: Error creating source endpoint: %s\nudp_create: ",
                          mod, source_name));
            return (errno);
        }
    } 
    else {
        source_point = ep;
    }

    if (source != NULL)
        *source = source_point;

    if (udp_util_debug) {
        printf("%s: Read %d bytes from %s on %s, socket %d.\n",
                mod, length, source_point->name, endpoint->name, endpoint->fd);
        MEO_DUMPX(stdout, "    ", 0, buffer, length);
    }

    return (0);
}

/*
 *    udp_write - writes a message to a destination UDP endpoint.
 *    A timeout interval can be specified that limits how long udp_write()
 *    waits to output the message.
 *
 *    Note that a message is written through a local source endpoint to the
 *    remote destination endpoint.  Only the destination endpoint is passed
 *    to udp_write(); udp_write() will use the destination's "parent" as the
 *    source endpoint.
 *
 *    Invocation:
 *        status = udp_write (destination, timeout, num_bytes_to_write, buffer);
 *
 *    where
 *        <destination>		- I
 *            is the destination endpoint handle returned by udp_create()
 *            or udp_read().
 *        <timeout>		- I
 *            specifies the maximum amount of time (in seconds) that the
 *            caller wishes to wait for the data to be output.  A fractional
 *            time can be specified; e.g., 2.5 seconds.   A negative timeout
 *            (e.g., -1.0) causes an infinite wait; udp_write() will wait as
 *            long as necessary to output all of the data.  A zero timeout
 *            (0.0) specifies no wait: if the socket is not ready for writing,
 *            udp_write() returns immediately.
 *        <num_bytes_to_write>	- I
 *            is the number of bytes to write.
 *        <buffer>		- O
 *            is the data to be output.
 *        <status>		- O
 *            returns the status of sending the message: zero if no errors
 *            occurred, EWOULDBLOCK if the timeout interval expired before
 *            the message could be sent, and ERRNO otherwise.
 *
 */
int  udp_write (udp_endpoint destination, double timeout, int num_bytes_to_write,
               const char *buffer)
{
    static char mod[] = "udp_write";
    fd_set write_mask;
    int fd, length, num_active;
    struct timeval delta_time, expiration_time;

    if (destination == NULL) {
        errno = EINVAL;
        DEBUG_OUT(0, ("%s: NULL destination handle: ", mod));
        return (errno);
    }

    fd = (destination->parent == NULL) ? destination->fd
                                       : (destination->parent)->fd;
    if (fd < 0) {
        errno = EINVAL;
        DEBUG_OUT(0,("%s: %d file descriptor: ", mod, fd));
        return (errno);
    }

	/*
     *	If a timeout interval was specified, then wait until the expiration
     *	of the interval for the endpoint's socket to be ready for writing.
     */
    if (timeout >= 0.0) {
        /* Compute the expiration time as the current time plus the interval. */
        expiration_time = tv_add(tv_tod (), tv_createf (timeout));

        /* Wait for the endpoint to be ready for writing. */
        for (;; ) {
            delta_time = tv_subtract (expiration_time, tv_tod ());
            FD_ZERO (&write_mask);  FD_SET (fd, &write_mask);
            num_active = select (fd+1, NULL, &write_mask, NULL, &delta_time);
            if (num_active >= 0)  break;
            if (errno == EINTR)  continue;
            DEBUG_OUT(0, ("%s: Error waiting to write to %s.\nselect: ",
                          mod, destination->name));
            return (errno);
        }

        if (num_active == 0) {
            errno = EWOULDBLOCK;
            DEBUG_OUT(0, ("%s: Timeout while waiting to write data to %s.\n",
                          mod, destination->name));
            return (errno);
        }
    }

	/*
     *	Send the message to the destination endpoint.
     */
    length = sendto (fd, (char *) buffer, num_bytes_to_write, 0,
                     &destination->address, destination->address_length);
    if (length < 0) {
        DEBUG_OUT(0, ("%s: Error sending %d-byte message to %s.\nsendto: ",
                      mod, num_bytes_to_write, 
                      (destination->name?destination->name:"unknown")));
        return (errno);
    }

    if (udp_util_debug) {
        printf ("%s: Wrote %d bytes to %s, socket %d.\n",
                mod, length, destination->name, fd);
        MEO_DUMPX (stdout, "    ", 0, buffer, length);
    }
    return (0);
}
