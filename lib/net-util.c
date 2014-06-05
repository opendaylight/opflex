/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

/*
 *
 * @File: net_util.c Network Utilities.
 *    The NET_UTIL package is a collection of miscellaneous network functions
 *    primarily intended to isolate operating system dependencies in networking
 *    code.
 *
 *Public Procedures:
 *    net_addr_of() - translates a host name to the host's IP address.
 *    net_host_of() - translates an IP address to its host name.
 *    net_port_of() - translates a service name to its server port.
 *    net_server_of() - translates a server port to its service name.
 *
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>	
#ifdef sun
#	define  strtoul  strtol		/* STRTOUL(3) is not supported. */
#	include <sys/filio.h>
#endif

#include <string.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/types.h>
#define  CACHE_NAMES  1

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#if CACHE_NAMES
#    include  "hash-util.h"
#endif

#include <gendcls.h>
#include "get-util.h"
#include "net-util.h"
#define DOTADDRBUFSIZE 24

/*
 *    dotaddbufr () Translate Network number to dot format character string.
 *
 *     In 32-bit architecture (in Linux/x86), struct in_addr type has 
 *     only one element s_addr which is of type in_addr_t and this is defined 
 *     as unsigned long type (4 bytes). However in 64-bit architecture(in 
 *     Linux/x86_64), it is defined as unsigned int type (4bytes). Therefore 
 *     when you try to print out dot notated ip address (xxx.xxx.xxx.xxx) 
 *     using received data. It would not work:
 *
 *     printf(%s\n, inet_ntoa(saddr));
 *
 *     Instead, I wrote a small function which can be used instead of 
 *     inet_ntoa(): I called this dotaddr()
 *
 *    Invocation:
 *        char_dot_address = dotaddr (network_number_address);
 *
 *    where
 *        <paddr>	- I
 *            in network number form the address to be converted to dot format.
 *        <dotaddrbuf>	- O
 *            returns a pointer to the dotaddrbuf, please note that this is 
 *            over written each time this is called.
 *
 */
static char dotaddrbuf[DOTADDRBUFSIZE];

char  *dotaddr( struct in_addr *paddr)
{
    union {
        char     dot[4];
#ifdef x86_64
        unsigned int    uaddr;
#else
        unsigned long   uaddr;
#endif
    } addr;

    memset(dotaddrbuf, 0, DOTADDRBUFSIZE);
    addr.uaddr=paddr->s_addr;
    sprintf(dotaddrbuf, "%d.%d.%d.%d", addr.dot[0], addr.dot[1], addr.dot[2], addr.dot[3]);

    return dotaddrbuf;
}

/*
 *    net_addr_of () Translate Host Name to IP Address.
 *
 *    Function net_addr_of() looks up a host by name and returns its IP address.
 *
 *    Invocation:
 *        ip_address = net_addr_of (host_name);
 *
 *    where
 *        <host_name>	- I
 *            is the host's name.  If this argument is NULL, the IP address
 *            of the local host is returned.
 *        <ip_address>	- O
 *            returns the IP adddress of the host in network-byte order; zero
 *            is returned in the case of an error.
 *
 */

unsigned long net_addr_of (const char *host_name)
{
    static char mod[] = "net_addr_of";
    char  local_host[MAXHOSTNAMELEN+1];
    unsigned  long  ip_address;

	/* If no host name was specified, use the local machine's host name. */
    if (host_name == NULL) {
        if (gethostname (local_host, sizeof local_host)) {
            DEBUG_OUT(0, ("%s: Error getting local host name.\ngethostname: ",
                          mod));
            return (0);
        }
        host_name = local_host;
    }

	/* If the host name was specified using the internet dot notation, then
   	**	convert it to a binary address. 
   	*/
    ip_address = inet_addr (host_name);
    if (ip_address != (unsigned long) -1)  
		return (ip_address);

	/* Otherwise, an actual name was specified.  Look up the name in the
   	** operating system's host database. 
   	*/
  	{ 
		struct  hostent  *hostEntry;

    	if ((hostEntry = gethostbyname (host_name)) == NULL) {
        	DEBUG_OUT(0, ("(net_addr_of) Error getting host entry for \"%s\".\ngethostbyname: ",
                 host_name));
        	return (0);
    	}
		/* Try to avoid big-endian/little-endian problems
		** by grabbing address as an unsigned long value. 
		*/
    	if (hostEntry->h_length != sizeof (unsigned long)) {
        	DEBUG_OUT(0, ("(net_addr_of) %s's address length is %d bytes, not %ld bytes.\n",
                 host_name, hostEntry->h_length, sizeof (unsigned long)));
        	return (0);
    	}
    	ip_address = *((unsigned long *) hostEntry->h_addr);
  	}

    return (ip_address);
}

/*
 *    net_host_of ()Translate IP Address to Host Name.
 *
 *    Function net_host_of() looks up an IP address and returns the corresponding
 *    host name.
 *
 *    Invocation:
 *        host_name = net_host_of (ip_address);
 *
 *    where
 *        <ip_address>	- I
 *            is the IP adddress of the host.
 *        <host_name>	- O
 *            returns the host's name; NULL is returned in the event of an
 *            error.  The ASCII name string is stored local to net_host_of()
 *            and it should be used or duplicated before calling net_host_of()
 *            again.
 *
 */
char *net_host_of (unsigned long ip_address)
{
    static char mod[] ="net_host_of";
    char  dotted_name[MAXHOSTNAMELEN+1], *host_name;
    struct  in_addr  address;
#if CACHE_NAMES
    static hash_table cached_names = NULL;
#endif

	/* On the first call to net_host_of(), 
       create the table of cached host names. */
#if CACHE_NAMES
    if ((cached_names == NULL) && hash_create (64, 0, &cached_names)) {
        DEBUG_OUT(0, ("%s: Error creating host name cache.\nhash_create: ",
                      mod));
        return (NULL);
    }
#endif

	/* Convert the IP address to its dotted format, "a.b.c.d". */
    address.s_addr = ip_address;
    strcpy (dotted_name, dotaddr (&address));

	/* To avoid having to query the name server, look up the dotted IP address
   	**	in the host name cache. 
   	*/

#if CACHE_NAMES
    if (hash_search (cached_names, dotted_name, (void **) &host_name))
        return (host_name);
#endif

	/* Bummer!  The name server must be queried for the host's name. */

  { struct  hostent  *host;

    host = gethostbyaddr ((const char *) &address, sizeof address, AF_INET);
    if (host == NULL)
        host_name = strdup (dotted_name);		/* ASCII IP address. */
    else
        host_name = strdup (host->h_name);		/* Actual host name. */
  }


    if (host_name == NULL) {
        DEBUG_OUT(0,("%s: Error duplicating host name.\nstrdup: ", mod));
        return (NULL);
    }

/* Cache the new translation for future reference. */

#if CACHE_NAMES
    if (hash_add (cached_names, dotted_name, (void *) host_name)) {
        DEBUG_OUT(0, ("(net_host_of) Error caching host name.\nhash_add: "));
    }
#endif

    return (host_name);
}

/*
 *    net_port_of () Translate Service Name to Server Port.
 *    Function net_port_of() looks up a server's name in the network services
 *    database (the "/etc/services" file) and returns the server's port
 *    number.
 *
 *    NOTE:  net_port_of() makes use of the system getservbyname(3) function
 *    under operating system's that support that function.  Otherwise, it
 *    manually reads the "/etc/services" file in search of the server name.
 *    In this latter case, a file other than "/etc/services" can be used as
 *    the services database by storing the full pathname of the file in
 *    environment variable, "SERVICES_FILE".
 *
 *
 *    Invocation:
 *        port = net_port_of (server_name, protocol);
 *
 *    where
 *        <server_name>	- I
 *            is the name of the service being looked up.
 *        <protocol>	- I
 *            specifies a network protocol; e.g., "tcp" or "udp".  A given
 *            service name may appear under different protocols in the
 *            services file.  If PROTOCOL is specified, then net_port_of()
 *            returns the server's port for that protocol.  If PROTOCOL is
 *            NULL, then net_port_of() returns the first port for the service
 *            name, regardless of the protocol.
 *        <port>		- O
 *            returns the requested service's port number in host byte order;
 *            -1 is returned in the event of an error (e.g., the service was
 *            not found).
 */
int net_port_of (const char *server_name, const char *protocol)
{
    static char mod[] = "net_port_of";
    char  *s;
    int  port_num;
#ifdef NO_GETSERVBYNAME
    char  *fname, inbuf[128];
    char  *s_name, *s_port, *s_protocol;
    FILE  *infile;
#else
    struct  servent  *server_entry;
#endif

	/* If the server "name" is simply the desired port number in ASCII,
   	**	then convert and return the binary port number to the caller. 
   	*/
    port_num = strtol (server_name, &s, 0);
    if (*s == '\0')
        return (port_num);


#ifndef NO_GETSERVBYNAME

	/* Under operating systems that support getservbyname(3), call it. */
    if ((server_entry = getservbyname (server_name, NULL)) == NULL) {
        if (errno == 0)  errno = EINVAL;
        DEBUG_OUT(0, ("%s: Error getting server entry for %s.\ngetservbyname: ",
                      mod, server_name));
        return (-1);
    }

    return (ntohs (server_entry->s_port));
#else

	/*
	*    Open the network services file.  The name of the file is retrieved
	*    from environment variable "SERVICES_FILE".  If that variable has
	*    no translation, then the file name defaults to the "/etc/services"
	*    system file.
	*/

    fname = getenv ("SERVICES_FILE");
    if (fname == NULL)  fname = "/etc/services";

    infile = fopen (fname, "r");
    if (infile == NULL) {
        DEBUG_OUT(0, ("%s: Error opening \"%s\" file.\nfopen: ",
                      mod, fname));
        return (-1);
    }

	/*******************************************************************************
    **	Scan the network services file for the requested service name.
	*******************************************************************************/
    for (;;) {

        /* Read the next server entry from the file.  Blank lines and comments
        *	are skipped. 
        */

        /* Read next line from file. */
        if (fgets (inbuf, (sizeof inbuf), infile) == NULL) {
            if (ferror (infile))
                DEBUG_OUT(0, ("(net_port_of) Error reading %s.\nfgets: ", fname));
            fclose (infile);
            return (-1);
        }					/* Strip comments and form feeds. */
        if ((s = strchr (inbuf, '#')) != NULL)  *s = '\0';
        while ((s = strchr (inbuf, '\f')) != NULL)  *s = ' ';

        /* Extract the service name, port number, and protocol from the entry. */
        s_name = strtok (inbuf, " \t\n/");	/* Service name. */
        if (s_name == NULL)  continue;		/* Skip blank lines. */
        s_port = strtok (NULL, " \t\n/");	/* Port number. */
        if (s_port == NULL)  continue;		/* No port number? */
        s_protocol = strtok (NULL, " \t\n/");	/* Protocol. */
        if (s_protocol == NULL)  continue;	/* No protocol? */
        
        /* If the protocols match, then scan the name and the aliases for the
        **	desired service name. 
        */
        if ((protocol != NULL) && (strcmp (protocol, s_protocol) != 0))
            continue;				/* Protocols don't match? */

        while (s_name != NULL) {		/* Scan name and aliases. */
            if (strcmp (s_name, server_name) == 0)  break;
            s_name = strtok (NULL, " \t\n/");
        }

        if (s_name != NULL)
            break;		/* Desired service found? */
    }


	/* An entry for the desired service was found.  Close the services file and
   	**	return the service's port number to the calling routine. 
   	*/
    fclose (infile);
    return (atoi (s_port));

#endif    /* NO_GETSERVBYNAME */
}


#ifdef  TEST

/*
*
*    Program to test the NET_UTIL functions.
*
*    Under UNIX:
*        Compile, link, and run as follows:
*            % cc -DTEST net_util.c <libraries> -o net_test
*            % net_test <port>
*
*    Under VxWorks:
*        Compile and link as follows:
*            % cc -DTEST net_util.c <libraries> -o net_test.vx.o
*        Load and run the server with the following commands:
*            -> ld <net_test.vx.o
*            -> sp net_test, "<port>"
*
*    The test program is a basic FTP server that listens for clients
*    at the specified network port.  Try connecting to it from within
*    ftp(1):
*
*        % ftp
*        ftp> open <host> <port>
*        ... enter username and password ...
*        ftp> pwd
*        ... see current directory ...
*        ftp> ls
*        ... list current directory ...
*        ftp> close
*        ... connection to server is closed ...
*        ftp>
*/

main (int argc,char *argv[])
{
    vperror_print = 1;
    printf ("Address of %s = %lX\n", argv[1], net_addr_of(argv[1]));
    printf ("   Host of %s = %s\n",
            argv[1], net_host_of (strtoul (argv[1], NULL, 0)));
    printf ("   Port of %s = %d\n", argv[1], net_port_of (argv[1], NULL));
    exit (0);
}

#endif  /* TEST */
