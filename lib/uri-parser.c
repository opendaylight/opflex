/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "uri-parser.h"
#include "hash.h"
#include "util.h"
#include "dbug.h"


/*
 * Prototype.
 */
static __inline__ int _is_scheme_char(int);

/*
 * Check whether the character is permitted in scheme string
 */
static __inline__ int
_is_scheme_char(int c)
{
    return (!isalpha(c) && '+' != c && '-' != c && '.' != c) ? 0 : 1;
}

/*
 * See RFC 1738, 3986
 */
parsed_uri_t *parse_uri(const char *uri)
{
    static char *mod = "parse_uri";
    parsed_uri_t *puri = NULL;
    const char *tmpstr;
    const char *curstr;
    int len;
    int i;
    int userpass_flag;
    int bracket_flag;
    

    DBUG_ENTER(mod);
    DBUG_PRINT("DEBUG", ("uri=%s", uri));
    /* Allocate the parsed uri storage */
    puri = xzalloc(sizeof(parsed_uri_t));
    puri->uri = strdup(uri);
    curstr = uri;

    /* create the hash for this uri */
    puri->hash = hash_string(uri, 0);

    /*
     * <scheme>:<scheme-specific-part>
     * <scheme> := [a-z\+\-\.]+
     *             upper case = lower case for resiliency
     */
    /* Read scheme */
    tmpstr = strchr(curstr, ':');
    if ( NULL == tmpstr ) {
        /* Not found the character */
        DBUG_PRINT("ERROR", ("can't find the : in: %s", uri));
        parsed_uri_free(&puri);
        puri = NULL;
        goto rtn_return;
    }
    /* Get the scheme length */
    len = tmpstr - curstr;
    /* Check restrictions */
    for ( i = 0; i < len; i++ ) {
        if ( !_is_scheme_char(curstr[i]) ) {
            /* Invalid format */
            DBUG_PRINT("ERROR",("invalid format: %s", uri));
            parsed_uri_free(&puri);
            puri = NULL;
            goto rtn_return;
        }
    }
    /* Copy the scheme to the storage */
    puri->scheme = xzalloc(sizeof(char) * (len + 1));
    (void)strncpy(puri->scheme, curstr, len);
    puri->scheme[len] = '\0';
    /* Make the character to lower if it is upper case. */
    for ( i = 0; i < len; i++ ) {
        puri->scheme[i] = tolower(puri->scheme[i]);
    }
    /* Skip ':' */
    tmpstr++;
    curstr = tmpstr;

    /*
     * //<user>:<password>@<host>:<port>/<uri-path>
     * Any ":", "@" and "/" must be encoded.
     */
    /* Eat "//" */
    if ( '/' == *curstr ) {
        for ( i = 0; i < 2; i++ ) {
            if ( '/' != *curstr ) {
                parsed_uri_free(&puri);
                DBUG_PRINT("ERROR", ("Not encoded correctly: %s", uri));
                puri = NULL;
                goto rtn_return;
            }
            curstr++;
        }
    }

    /* Check if the user (and password) are specified. */
    userpass_flag = 0;
    tmpstr = curstr;
    while ( '\0' != *tmpstr ) {
        if ( '@' == *tmpstr ) {
            /* Username and password are specified */
            userpass_flag = 1;
            break;
        } else if ( '/' == *tmpstr ) {
            /* End of <host>:<port> specification */
            userpass_flag = 0;
            break;
        }
        tmpstr++;
    }

    /* User and password specification */
    tmpstr = curstr;
    if ( userpass_flag ) {
        /* Read username */
        while ( '\0' != *tmpstr && ':' != *tmpstr && '@' != *tmpstr ) {
            tmpstr++;
        }
        len = tmpstr - curstr;
        puri->username = xzalloc(sizeof(char) * (len + 1));
        (void)strncpy(puri->username, curstr, len);
        puri->username[len] = '\0';
        /* Proceed current pointer */
        curstr = tmpstr;
        if ( ':' == *curstr ) {
            /* Skip ':' */
            curstr++;
            /* Read password */
            tmpstr = curstr;
            while ( '\0' != *tmpstr && '@' != *tmpstr ) {
                tmpstr++;
            }
            len = tmpstr - curstr;
            puri->password = xzalloc(sizeof(char) * (len + 1));
            (void)strncpy(puri->password, curstr, len);
            puri->password[len] = '\0';
            curstr = tmpstr;
        }
        /* Skip '@' */
        if ( '@' != *curstr ) {
            parsed_uri_free(&puri);
            DBUG_PRINT("ERROR", ("Not encoded correctly: %s", uri));
            puri = NULL;
            goto rtn_return;
        }
        curstr++;
    }

    if ( '[' == *curstr ) {
        bracket_flag = 1;
    } else {
        bracket_flag = 0;
    }
    /* Proceed on by delimiters with reading host */
    tmpstr = curstr;
    while ( '\0' != *tmpstr ) {
        if ( bracket_flag && ']' == *tmpstr ) {
            /* End of IPv6 address. */
            tmpstr++;
            break;
        } else if ( !bracket_flag && (':' == *tmpstr || '/' == *tmpstr) ) {
            /* Port number is specified. */
            break;
        }
        tmpstr++;
    }
    len = tmpstr - curstr;
    puri->host = xzalloc(sizeof(char) * (len + 1));
    (void)strncpy(puri->host, curstr, len);
    puri->host[len] = '\0';
    curstr = tmpstr;

    /* Is port number specified? */
    if ( ':' == *curstr ) {
        curstr++;
        /* Read port number */
        tmpstr = curstr;
        while ( '\0' != *tmpstr && '/' != *tmpstr ) {
            tmpstr++;
        }
        len = tmpstr - curstr;
        puri->port = xzalloc(sizeof(char) * (len + 1));
        (void)strncpy(puri->port, curstr, len);
        puri->port[len] = '\0';
        curstr = tmpstr;
    }

    /* End of the string */
    if ( '\0' == *curstr ) {
        return puri;
    }

    /* Skip '/' */
    if ( '/' != *curstr ) {
        parsed_uri_free(&puri);
        DBUG_PRINT("ERROR", ("Not encoded correctly: %s", uri));
        puri = NULL;
        goto rtn_return;
    }
    curstr++;

    /* Parse path */
    tmpstr = curstr;
    while ( '\0' != *tmpstr && '#' != *tmpstr  && '?' != *tmpstr ) {
        tmpstr++;
    }
    len = tmpstr - curstr;
    puri->path = xzalloc(sizeof(char) * (len + 1));
    (void)strncpy(puri->path, curstr, len);
    puri->path[len] = '\0';
    curstr = tmpstr;

    /* Is query specified? */
    if ( '?' == *curstr ) {
        /* Skip '?' */
        curstr++;
        /* Read query */
        tmpstr = curstr;
        while ( '\0' != *tmpstr && '#' != *tmpstr ) {
            tmpstr++;
        }
        len = tmpstr - curstr;
        puri->query = xzalloc(sizeof(char) * (len + 1));
        (void)strncpy(puri->query, curstr, len);
        puri->query[len] = '\0';
        curstr = tmpstr;
    }

    /* Is fragment specified? */
    if ( '#' == *curstr ) {
        /* Skip '#' */
        curstr++;
        /* Read fragment */
        tmpstr = curstr;
        while ( '\0' != *tmpstr ) {
            tmpstr++;
        }
        len = tmpstr - curstr;
        puri->fragment = xzalloc(sizeof(char) * (len + 1));
        (void)strncpy(puri->fragment, curstr, len);
        puri->fragment[len] = '\0';
        curstr = tmpstr;
    }

 rtn_return:
    DBUG_PRINT("<", (""));
    return(puri);
}

/*
 * Free memory of parsed uri
 */
void parsed_uri_free(parsed_uri_p *puri)
{
    if ( NULL != *puri ) {
        if ( NULL != (*puri)->uri ) {
            free((*puri)->uri);
        }
        if ( NULL != (*puri)->scheme ) {
            free((*puri)->scheme);
        }
        if ( NULL != (*puri)->host ) {
            free((*puri)->host);
        }
        if ( NULL != (*puri)->port ) {
            free((*puri)->port);
        }
        if ( NULL != (*puri)->path ) {
            free((*puri)->path);
        }
        if ( NULL != (*puri)->query ) {
            free((*puri)->query);
        }
        if ( NULL != (*puri)->fragment ) {
            free((*puri)->fragment);
        }
        if ( NULL != (*puri)->username ) {
            free((*puri)->username);
        }
        if ( NULL != (*puri)->password ) {
            free((*puri)->password);
        }
        free(*puri);
    }
}
