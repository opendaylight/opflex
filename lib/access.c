/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

/*
 * @file access.c
 * 
 * @brief host/network access control utilities
 * 
 * Revision History:  (latest changes at top) 
 *    08312001 - dkehn - created.
 * 
 * 
 */
 
#include <gendcls.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "debug.h"
#include "str-util.h"
#include "util.h"

#define putip(dest,src) memcpy(dest,src,4) 

/* Delimiters for lists of daemons or clients. */
static char *sep = ", \t";

#define	FAIL		(-1)

int access_db_level = 3 ;

/****************************************************************************
 * CODE
 ***************************************************************************/
/* ==========================================================================
 * @brief matchname - 
 *        determine if host name matches IP address 
 *
 * @param0 <remotehost>                 - I
 *         char string of the a remotehost name
 * @param1 <addr>                       - I
 *         ip addr to compare against
 * @return <return>                     - I
 *         True if the remotehost is ip, else False
 *
 **/
bool matchname(char *remotehost,struct in_addr  addr)
{
    static char mod[] = "matchname";
    struct hostent *hp;
    int     i;
    
    if ((hp = gethostbyname(remotehost)) == 0) {
        DEBUG_OUT(0,("(%s) gethostbyname(%s): lookup failure.\n", mod, remotehost));
        return False;
    } 

    /*
     * Make sure that gethostbyname() returns the "correct" host name.
     * Unfortunately, gethostbyname("localhost") sometimes yields
     * "localhost.domain". Since the latter host name comes from the
     * local DNS, we just have to trust it (all bets are off if the local
     * DNS is perverted). We always check the address list, though.
     */
  
    if (strcasecmp(remotehost, hp->h_name)
        && strcasecmp(remotehost, "localhost")) {
        DEBUG_OUT(0,("(%s) host name/name mismatch: %s != %s\n",
                     mod, remotehost, hp->h_name));
        return False;
    }
	
    /* Look up the host address in the address list we just got. */
    for (i = 0; hp->h_addr_list[i]; i++) {
        if (memcmp(hp->h_addr_list[i], (caddr_t) & addr, sizeof(addr)) == 0)
            return True;
    }

    /*
     * The host name does not map to the original host address. Perhaps
     * someone has compromised a name server. More likely someone botched
     * it, but that could be dangerous, too.
     */
  
    DEBUG_OUT(0,("(%s) host name/address mismatch: %s != %s\n",
                 mod, inet_ntoa(addr), hp->h_name));
    return False;
}

/* =========================================================================
 *
 * @brief is_ipaddress()
 *        Returns true if a string could be a pure IP address.
 *
 * @param0 <str>                     - I
 *         ip address string
 * @return <pure_address>            - O
 *         True if ip address, else false.
 *
 **/
bool is_ipaddress(const char *str)
{
    bool pure_address = True;
    int i;
  
    for (i=0; pure_address && str[i]; i++)
        if (!(isdigit((int)str[i]) || str[i] == '.'))
            pure_address = False;
  
    /* Check that a pure number is not misinterpreted as an IP */
    pure_address = pure_address && (strchr(str, '.') != NULL);

    return pure_address;
}

/* ============================================================================
 * 
 * @brief interpret_addr()
 *        interpret an internet address or name into an IP address in 
 *        4 byte form
 *
 * @param0 <str>             - I
 *         host name either in dot format of DNS name
 * @return <res>             - O
 *         IP number format
 *
 **/
unsigned int interpret_addr(char *str)
{
    static char mod[] = "interpret_addr";
    struct hostent *hp;
    unsigned int  res;
  
    if (strcmp(str,"0.0.0.0") == 0) return(0);
    if (strcmp(str,"255.255.255.255") == 0) return(0xFFFFFFFF);

    /* if it's in the form of an IP address then get the lib to interpret it */
    if (is_ipaddress(str)) {
        res = inet_addr(str);
    } else {
        /* otherwise assume it's a network name of some sort and use 
           Get_Hostbyname */
        if ((hp = gethostbyname(str)) == 0) {
            DEBUG_OUT(access_db_level,("(%s) gethostbyname: Unknown host. %s\n",mod, str));
            return 0;
        }
        if(hp->h_addr == NULL) {
            DEBUG_OUT(3,("(%s)gethostbyname: host address is invalid for host %s\n",
                         mod, str));
            return 0;
        }
        putip((char *)&res,(char *)hp->h_addr);
    }

    if (res == (unsigned int)-1) return(0);

    return(res);
}

/* =====================================================================
 * masked_match - match address against netnumber/netmask 
 */
static int masked_match(char *tok, char *slash, char *s)
{
    static char mod[] = "masked_match";
    unsigned int net, mask, addr;
  
    if ((addr = interpret_addr(s)) == INADDR_NONE)
        return (False);
    *slash = 0;
    net = interpret_addr(tok);
    *slash = '/';
    if (net == INADDR_NONE || 
        (mask = interpret_addr(slash + 1)) == INADDR_NONE) {
        DEBUG_OUT(0,("(%s) access: bad net/mask access control: %s\n", mod, tok));
        return (False);
    }
    return ((addr & mask) == net);
}

/* =======================================================================
 *
 *  string_match - match string against token 
 * Return True if a token has the magic value "ALL". Return
 * FAIL if the token is "FAIL". If the token starts with a "."
 * (domain name), return True if it matches the last fields of
 * the string. If the token has the magic value "LOCAL",
 * return True if the string does not contain a "."
 * character. If the token ends on a "." (network number),
 * return True if it matches the first fields of the
 * string. If the token begins with a "@" (netgroup name),
 * return True if the string is a (host) member of the
 * netgroup. Return True if the token fully matches the
 * string. If the token is a netnumber/netmask pair, return
 * True if the address is a member of the specified subnet.  
 *  
 **/
static int string_match(char *tok,char *s, char *invalid_char)
{
    static char mod[] = "string_match";
    size_t     tok_len;
    size_t     str_len;
    char   *cut;
  
    *invalid_char = '\0';
  
    if (tok[0] == '.') {			/* domain: match last fields */
        if ((str_len = strlen(s)) > (tok_len = strlen(tok))
            && strcasecmp(tok, s + str_len - tok_len) == 0)
            return (True);
    } else if (tok[0] == '@') { /* netgroup: look it up */
#ifdef	HAVE_NETGROUP
        static char *mydomain = NULL;
        char *hostname = NULL;
        bool netgroup_ok = False;
    
        if (!mydomain) yp_get_default_domain(&mydomain);
    
        if (!mydomain) {
            DEBUG_OUT(0,("(%s) Unable to get default yp domain.\n", mod));
            return False;
        }
        if (!(hostname = strdup(s))) {
            DEBUG_OUT(1,("out of memory for strdup!\n"));
            return False;
        }
    
        netgroup_ok = innetgr(tok + 1, hostname, (char *) 0, mydomain);
    
        DEBUG_OUT(THIS_BDB_LEVEL, ("(%s) looking for %s of domain %s in netgroup %s gave %s\n", 
                                   mod,
                                   hostname,
                                   mydomain, 
                                   tok+1,
                                   boolSTR(netgroup_ok)));
    
        free(hostname);
    
        if (netgroup_ok) return(True);
#else
        DEBUG_OUT(0,("(%s) access: netgroup support is not configured\n", mod));
        return (False);
#endif
    } else if (strcasecmp(tok, "ALL") == 0) {	/* all: match any */
        return (True);
    } else if (strcasecmp(tok, "FAIL") == 0) {	/* fail: match any */
        return (FAIL);
    } else if (strcasecmp(tok, "LOCAL") == 0) {	/* local: no dots */
        if (strchr(s, '.') == 0 && strcasecmp(s, "unknown") != 0)
            return (True);
    } else if (!strcasecmp(tok, s)) {   /* match host name or address */
        return (True);
    } else if (tok[(tok_len = strlen(tok)) - 1] == '.') {	/* network */
        if (strncmp(tok, s, tok_len) == 0)
            return (True);
    } else if ((cut = strchr(tok, '/')) != 0) {	/* netnumber/netmask */
        if (isdigit((int)s[0]) && masked_match(tok, cut, s))
            return (True);
    } else if (strchr(tok, '*') != 0) {
        *invalid_char = '*';
    } else if (strchr(tok, '?') != 0) {
        *invalid_char = '?';
    }
    return (False);
}


/* =======================================================================
 *  client_match - match host name and address against token 
 */
static int client_match(char *tok,char *item)
{
    char **client = (char **)item;
    int     match;
    char invalid_char = '\0';
  
    /*
     * Try to match the address first. If that fails, try to match the host
     * name if available.
     */
  
    if ((match = string_match(tok, client[1], &invalid_char)) == 0) {
        if(invalid_char)
            DEBUG_OUT(0,("(client_match) address match failing due to invalid character '%c' found in \
token '%s' in an allow/deny hosts line.\n", invalid_char, tok ));
    
        if (client[0][0] != 0)
            match = string_match(tok, client[0], &invalid_char);
    
        if(invalid_char)
            DEBUG_OUT(0,("(client_match) address match failing due to invalid character '%c' found in \
token '%s' in an allow/deny hosts line.\n", invalid_char, tok ));
    }
  
    return (match);
}

/* =================================================================================
 * list_match - match an item against a list of tokens with exceptions 
 * (All modifications are marked with the initials "jkf") 
 */
static int list_match(char *list,char *item, int (*match_fn)(char *, char *))
{
    char   *tok;
    char   *listcopy;
    int     match = False;
  
    listcopy = (list == 0) ? (char *)0 : strdup(list);
  
    /*
     * Process tokens one at a time. We have exhausted all possible matches
     * when we reach an "EXCEPT" token or the end of the list. If we do find
     * a match, look for an "EXCEPT" list and recurse to determine whether
     * the match is affected by any exceptions.
     */
  
    for (tok = strtok(listcopy, sep); tok ; tok = strtok(NULL, sep)) {
        if (strcasecmp(tok, "EXCEPT") == 0)	/* EXCEPT: give up */
            break;
        if ((match = (*match_fn) (tok, item)))	/* True or FAIL */
            break;
    }
    /* Process exceptions to True or FAIL matches. */
  
    if (match != False) {
        while ((tok = strtok((char *) 0, sep)) && strcasecmp(tok, "EXCEPT"))
            /* VOID */ ;
        if (tok == 0 || list_match((char *) 0, item, match_fn) == False) {
            if (listcopy != 0) free(listcopy); /* jkf */
            return (match);
        }
    }
  
    if (listcopy != 0) free(listcopy); /* jkf */
    return (False);
}


/* ================================================================================
 * @brief allow_access()
 *
 * @param0 <deny_list>                       - I
 *         List of network/addr that should not be allowed, i.e. from lp_hostdeny() in
 *         loadparm.c
 * @param1 <allow_list>                      - I
 *
 * @return true if access should be allowed 
 **/
bool allow_access(char *deny_list,char *allow_list,
                  char *cname,char *caddr)
{
    char *client[2];
  
    client[0] = cname;
    client[1] = caddr;  

    DEBUG_OUT(access_db_level, ("allow_access:%ld) deny_list=%s allow_list=%s cname=%s caddr=%s\n", 
                                pthread_self(), deny_list, allow_list, cname, caddr));

    /* if it is loopback then always allow unless specifically denied */
    if (strcmp(caddr, "127.0.0.1") == 0) {
        if (deny_list && 
            list_match(deny_list,(char *)client,client_match)) {
            return False;
        }
        return True;
    }
  
    /* if theres no deny list and no allow list then allow access */
    if ((!deny_list || *deny_list == 0) && 
        (!allow_list || *allow_list == 0)) {
        return(True);  
    }
  
    /* if there is an allow list but no deny list then allow only hosts
       on the allow list */
    if (!deny_list || *deny_list == 0) {
        if (list_match(allow_list,(char *)client,client_match))
            return True;
        else 
            return False;
        /*    return(list_match(allow_list,(char *)client,client_match)); */
    }
  
    printf("----------------------------\n");
    /* if theres a deny list but no allow list then allow
       all hosts not on the deny list */
    if (!allow_list || *allow_list == 0)
        return(!list_match(deny_list,(char *)client,client_match));
  
    /* if there are both type of list then allow all hosts on the
       allow list */
    if (list_match(allow_list,(char *)client,client_match))
        return (True);
  
    /* if there are both type of list and it's not on the allow then
       allow it if its not on the deny */
    if (list_match(deny_list,(char *)client,client_match))
        return (False);
  
    return (True);
}
/* ============================================================================
 *
 * @brief client_name()
 *        returns the IP address of the client as a string
 *
 * @param0 <fd>                  - I
 *         socket fd
 * @return <name_buf>            - O
 *         IP string
 *
 **/
char *client_addr(int fd)
{
    struct sockaddr sa;
    struct sockaddr_in *sockin = (struct sockaddr_in *) (&sa);
    socklen_t length = sizeof(sa);
    static fstring addr_buf;
    static int last_fd = -1;

    if (fd == last_fd) 
        return addr_buf;

    last_fd = fd;

    fstrcpy(addr_buf,"0.0.0.0");
  
    if (fd == -1) {
        return addr_buf;
    }
  
    if (getpeername(fd, &sa, &length) < 0) {
        DEBUG_OUT(0,("(client_addr) getpeername failed. sockdesc=%d.  Error was %s\n", 
                     fd, strerror(errno) ));
        return addr_buf;
    }
    fstrcpy(addr_buf,inet_ntoa(sockin->sin_addr));
  
    return addr_buf;
}

/* ============================================================================
 *
 * @brief client_name()
 *        returns the DNS name of the client based upon the socket fd.
 *
 * @param0 <fd>                  - I
 *         socket fd
 * @return <name_buf>            - O
 *         DNS host name
 *
 **/
char *client_name(int fd)
{
    static char mod[] = "client_name";
    struct sockaddr sa;
    struct sockaddr_in *sockin = (struct sockaddr_in *) (&sa);
    socklen_t  length = sizeof(sa);
    static pstring name_buf;
    struct hostent *hp;
    static int last_fd=-1;
  
    if (last_fd == fd) 
        return name_buf;
  
    last_fd = fd;
  
    pstrcpy(name_buf,"UNKNOWN");
  
    if (fd == -1) {
        return name_buf;
    }
  
    if (getpeername(fd, &sa, &length) < 0) {
        DEBUG_OUT(0,("(%s) getpeername failed. Error was %s\n", 
                     mod, strerror(errno) ));
        return name_buf;
    }
  
    /* Look up the remote host name. */
    if ((hp = gethostbyaddr((char *) &sockin->sin_addr,
                            sizeof(sockin->sin_addr),
                            AF_INET)) == 0) {
        DEBUG_OUT(1,("(%s) Gethostbyaddr failed for %s\n",mod, client_addr(fd)));
        StrnCpy(name_buf,client_addr(fd),sizeof(name_buf) - 1);
    } 
    else {
        StrnCpy(name_buf,(char *)hp->h_name,sizeof(name_buf) - 1);
        if (!matchname(name_buf, sockin->sin_addr)) {
            DEBUG_OUT(0,("(%s) Matchname failed on %s %s\n",mod,name_buf,client_addr(fd)));
            pstrcpy(name_buf,"UNKNOWN");
        }
    }

    return name_buf;
}


/* =============================================================================
 *
 * @brief check_access()
 *        check_access will check a service based upon a socked fd,
 *        returns true if access should be allowed to a service for a socket 
 *
 * @param0 <sock>               - I
 *         socket fd
 * @param1 <allow_list>         - I
 *         the allow list as define in the loadparm.c or config file, see
 *         lp_hostallow()
 * @param1 <deny_list>          - I
 *         the deny list as define in the loadparm.c or config file, see
 *         lp_hostdeny()
 * @return <ret>                - O
 *          True if the sock is allowed else False
 *
 **/
bool check_access(int sock, char *allow_list, char *deny_list)
{
    bool ret = False;
  
    if (deny_list) deny_list = strdup(deny_list);
    if (allow_list) allow_list = strdup(allow_list);
  
    if ((!deny_list || *deny_list==0) && (!allow_list || *allow_list==0)) {
        ret = True;
    }
  
    if (!ret) {
        if (allow_access(deny_list,allow_list,
                         client_name(sock),client_addr(sock))) {
            DEBUG_OUT(2,("Allowed connection from %s (%s)\n",
                         client_name(sock),client_addr(sock)));
            ret = True;
        } 
        else {
            DEBUG_OUT(0,("Denied connection from %s (%s)\n",
                         client_name(sock),client_addr(sock)));
        }
    }
  
    if (deny_list)
        free(deny_list);
    if (allow_list)
        free(allow_list);
    return(ret);
}
