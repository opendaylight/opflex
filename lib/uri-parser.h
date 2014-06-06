/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */


#ifndef _URI_PARSER_H
#define _URI_PARSER_H

#include "hash.h"

/*
 * URI storage
 */
typedef struct _parsed_uri {
    char *uri;                  /* the original */
    char *scheme;               /* mandatory */
    char *host;                 /* mandatory */
    char *port;                 /* optional */
    char *path;                 /* optional */
    char *query;                /* optional */
    char *fragment;             /* optional */
    char *username;             /* optional */
    char *password;             /* optional */
    uint32_t hash;                /* required */
} parsed_uri_t, *parsed_uri_p;

#ifdef __cplusplus
extern "C" {
#endif

extern bool parse_uri(parsed_uri_p *pp, const char *uri);
extern void parsed_uri_free(parsed_uri_p *puri);

#ifdef __cplusplus
}
#endif

#endif /* _URI_PARSER_H */
