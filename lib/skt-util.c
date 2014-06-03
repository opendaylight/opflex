/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

/*
 *    Contains a number of common functions used by
 *    my other networking packages: TCP_UTIL, UDP_UTIL, ...
 *
 *
 *Public Procedures:
 *
 *    skt_is_readable() - checks if data is waiting to be read on a socket.
 *    skt_is_up() - checks if a socketis up.
 *    skt_is_writeable() - checks if data can be written to a socket.
 *    skt_peer() - returns the name of the host at the other end of a socket.
 *    skt_port() - returns the port number of a socket.
 *    skt_set_buf() - changes the sizes of a socket's receive and send buffers.
 *
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>

#include "gendcls.h" 
#include "meo-util.h"
#include "str-util.h"
#include "debug.h"
#include "util.h"
#include "opt-util.h"
#include "tcp-util.h"
#include "get-util.h"
#include "dae-util.h"
#include "net-util.h"

/*
 *    skt_is_readable () Check if Data is Waiting to be Read from a Socket.
 *
 *    The skt_is_readable() function checks to see if data is waiting to
 *    be read from a socket.
 *
 *    Invocation:
 *        isReadable = skt_is_readable (name, fd);
 *
 *    where
 *        <name>		- I
 *            is the name of the socket for debugging purposes; this argument
 *            can be NULL.
 *        <fd>		- I
 *            is the UNIX file descriptor for the socket.
 *        <isReadable>	- O
 *            returns true (a non-zero value) if data is available for
 *            reading and false (zero) otherwise.
 */
int skt_is_readable (const char *name, int  fd)
{
    static char mod[] = "skt_is_readable";
    fd_set  read_mask;
    long  length;
    struct timeval timeout;

    if (name == NULL)
        name = "?";

	/* Poll the data socket for input. */
    FD_ZERO (&read_mask);  FD_SET (fd, &read_mask);
    timeout.tv_sec = timeout.tv_usec = 0;	/* No wait. */
    while (select (fd+1, &read_mask, NULL, NULL, &timeout) < 0) {
        if (errno == EINTR)  continue;		/* SELECT interrupted by signal - try again. */
        DEBUG_OUT(0, ("%s: Error polling %s, socket %d.\nselect: ", mod,
                      name, fd));
        return (0);
    }
    /* No input pending? */
    if (!FD_ISSET (fd, &read_mask))
        return (0);

	/* Input is pending.  Find out how many bytes of data are 
	** actually available for input.  If SELECT(2) indicates 
	** pending input, but IOCTL(2) indicates zero bytes of 
	** pending input, the connection is broken. 
	*/
    if (ioctl (fd, FIONREAD, &length) == -1) {
        DEBUG_OUT(0,("%s: Error polling %s, socket %d.\nioctl: ", mod,
                     name, fd));
        return (0);
    }

    if (length > 0) {
        return (length);			/* Pending input. */
    } 
    else {
        errno = EPIPE;
        DEBUG_OUT(0, ("%s: Broken connection to %s, socket %d.\n", mod,
                      name, fd));
        return (0);				/* EOF. */
    }
}

/*
 *    skt_is_up () Check if a Connection is Up.
 *    The skt_is_up() function checks to see if a network connection is still up.
 *
 *    Invocation:
 *        isUp = skt_is_up (name, fd);
 *
 *    where
 *        <name>		- I
 *            is the name of the socket for debugging purposes; this argument
 *            can be NULL.
 *        <fd>		- I
 *            is the UNIX file descriptor for the socket.
 *        <isUp>		- O
 *            returns true (a non-zero value) if the network connection is
 *            up and false (zero) otherwise.
 *
 */
int skt_is_up (const  char  *name, int  fd)
{
    static char mod[] = "skt_is_up";
    fd_set read_mask;
    long length;
    struct timeval timeout;

    DEBUG_OUT(4, ("%s: Called with: name=%s fd=%d\n", 
                  mod, name, fd));

    if (name == NULL)
        name = "?";

	/* Poll the data socket for input. */
    FD_ZERO (&read_mask);  FD_SET (fd, &read_mask);
    timeout.tv_sec = timeout.tv_usec = 0;	/* No wait. */
    while (select (fd+1, &read_mask, NULL, NULL, &timeout) < 0) {
        if (errno == EINTR)  continue;		/* SELECT interrupted by signal - try again. */
        DEBUG_OUT(0, ("%s: Error polling %s, socket %d.\nselect: ",
                      mod, name, fd));
        return (0);				/* Connection is down. */
    }

    if (!FD_ISSET (fd, &read_mask))		/* No input pending? */
        return (1);				/* Connection is up. */

	/* Input is pending.  Find out how many bytes of data are actually available
       for input.  If SELECT(2) indicates pending input, but IOCTL(2) indicates
       zero bytes of pending input, the connection is broken. */

    if (ioctl (fd, FIONREAD, &length) == -1) {
        DEBUG_OUT(0,( "%s: Error polling %s, socket %d.\nioctl: ",
                      mod, name, fd));
        return (0);				/* Connection is down. */
    }

    if (length > 0) {				/* Pending input? */
        return (1);				/* Connection is up. */
    }
    else {
        errno = EPIPE;
        DEBUG_OUT(0, ("%s: Broken connection to %s, socket %d.\n",
                      mod, name, fd));
        return (0);				/* Connection is down. */
    }
}

/*
 *    skt_is_writeable () Check if Data can be Written.
 *
 *    The skt_is_writeable() function checks to see if data can be written
 *    to a connection.
 *
 *    Invocation:
 *        isWriteable = skt_is_writeable (name, fd);
 *
 *    where
 *        <name>		- I
 *            is the name of the socket for debugging purposes; this argument
 *            can be NULL.
 *        <fd>		- I
 *            is the UNIX file descriptor for the socket.
 *        <isWriteable>	- O
 *            returns true (a non-zero value) if data connection is ready
 *            for writing and false (zero) otherwise.
 *
 */
int skt_is_writeable (const char *name, int fd)
{ 
    static char mod[] = "skt_is_writeable";
    fd_set  write_mask;
    struct  timeval  timeout;

    if (name == NULL)
        name = "?";

	/* Poll the data socket for output. */
    FD_ZERO (&write_mask);  FD_SET (fd, &write_mask);
    timeout.tv_sec = timeout.tv_usec = 0;	/* No wait. */
    while (select (fd+1, NULL, &write_mask, NULL, &timeout) < 0) {
        if (errno == EINTR)  continue;		/* SELECT interrupted by signal - try again. */
        DEBUG_OUT(0, ("%s: Error polling %s, socket %d.\nselect: ",
                      mod, name, fd));
        return (0);
    }
    /* Ready for output? */
    return (FD_ISSET (fd, &write_mask));
}

/*
 *    skt_peer () Get a Socket's Peer Name.
 *
 *    Function skt_peer() returns the name (i.e., the Internet address) of
 *    the host at the other end of a network socket connection.
 *
 *    Invocation:
 *        host = skt_peer (name, fd);
 *
 *    where
 *        <name>		- I
 *            is the name of the socket for debugging purposes; this argument
 *            can be NULL.
 *        <fd>		- I
 *            is the UNIX file descriptor for the socket.
 *        <host>	- O
 *            returns the Internet address of the connected host.  The address
 *            is formatted in ASCII using the standard Internet dot notation:
 *            "a.b.c.d".  NULL is returned in the event of an error.  The
 *            ASCII host string is stored local to skt_peer() and it should
 *            be used or duplicated before calling skt_peer() again.
 *
 */
char *skt_peer (const char *name, int fd)
{
    static char mod[] = "skt_peer";
    struct sockaddr peer_addr;
    struct sockaddr_in *ip_address;
    socklen_t length;

    if (name == NULL)
        name = "?";

	/* Get the IP address of the host on the other end of the network connection. */
    length = sizeof (peer_addr);
    if (getpeername (fd, &peer_addr, &length)) {
        DEBUG_OUT(0, ("%s: Error getting peer's host for %s, socket %d.\ngetpeername: ",
                      mod, name, fd));
        return (NULL);
    }
    ip_address = (struct sockaddr_in *) &peer_addr;

	/* Convert the peer's IP address to a host name. */
    return (net_host_of(ip_address->sin_addr.s_addr));
}

/*
*    skt_port () Get a Socket's Port Number.
*
*    Function skt_port() returns the number of the port to which a socket
*    (either listening or data) is bound.
*
*    Invocation:
*        number = skt_port (name, fd);
*
*    where
*        <name>		- I
*            is the name of the socket for debugging purposes; this argument
*            can be NULL.
*        <fd>		- I
*            is the UNIX file descriptor for the socket.
*        <number>	- O
*            returns the socket's port number; -1 is returned in the
*            event of an error.
*
*/
int skt_port (const char *name, int fd)
{
    socklen_t length;
    struct sockaddr_in socket_name;

	/* Get the socket's port number and convert it to host byte ordering. */
    length = sizeof socket_name;
    if (getsockname (fd, (struct sockaddr *) &socket_name, &length)) {
        DEBUG_OUT(0, ("(skt_port) Error getting port number for %s, socket %d.\ngetsockname: ",
                 name, fd));
        return (-1);
    }

    return (ntohs (socket_name.sin_port));
}

/*
 *    skt_set_buf () Change the Sizes of a Socket's Receive and Send Buffers.
 *
 *    Function skt_set_buf() changes the sizes of a socket's receive and/or send
 *    buffers.
 *
 *    Invocation:
 *        status = skt_set_buf (name, fd, receiveSize, send_size);
 *
 *    where
 *        <name>		- I
 *            is the name of the socket for debugging purposes; this argument
 *            can be NULL.
 *        <fd>		- I
 *            is the UNIX file descriptor for the socket.
 *        <receiveSize>	- I
 *            specifies the new size of the socket's receive system buffer.
 *            If this argument is less than zero, the receive buffer retains
 *            its current size.
 *        <send_size>	- I
 *            specifies the new size of the socket's send system buffer.  If
 *            this argument is less than zero, the send buffer retains its
 *            current size.
 *        <status>	- O
 *            returns the status of changing the buffers' sizes, zero if no
 *            error occurred and ERRNO otherwise.
 *
 */

int skt_set_buf (const char *name, int fd, int receive_size, int send_size)
{
    static char mod[] = "skt_set_buf";

	/* Change the size of the socket's receive system buffer. */
    if ((receive_size >= 0) &&
        setsockopt (fd, SOL_SOCKET, SO_RCVBUF, &receive_size, sizeof (int))) {
        DEBUG_OUT(0, ("%s: Error setting receive buffer size (%d) for %s, socket %d.\nsetsockopt: ",
                      mod, receive_size, name, fd));
        return (errno);
    }

	/* Change the size of the socket's send system buffer. */

    if ((send_size >= 0) &&
        setsockopt (fd, SOL_SOCKET, SO_SNDBUF, &send_size, sizeof (int))) {
        DEBUG_OUT(0, ("%s: Error setting send buffer size (%d) for %s, socket %d.\nsetsockopt: ",
                      mod, send_size, name, fd));
        return (errno);
    }
    return (0);
}
