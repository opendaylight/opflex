/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */


#ifndef _CNF_PARSER_H_
#define _CNF_PARSER_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cnf-dictionary.h"

int cnf_parser_getnsec(dictionary *d);
char *cnf_parser_getsecname(dictionary *d, int n);
void cnf_parser_dump_ini(dictionary *d, FILE *fd);
void cnf_parser_dumpsection_cnf(dictionary *d, char *s, FILE *fd);
void cnf_parser_dump(dictionary * d, FILE *fd);
int cnf_parser_getsecnkeys(dictionary *d, char *s);
char **cnf_parser_getseckeys(dictionary *d, char *s);
char *cnf_parser_getstring(dictionary *d, const char *key, char *def);
int cnf_parser_getint(dictionary *d, const char *key, int notfound);
double cnf_parser_getdouble(dictionary *d, const char *key, double notfound);
int cnf_parser_getboolean(dictionary *d, const char *key, int notfound);
int cnf_parser_set(dictionary *cnf, const char * entry, const char * val);
void cnf_parser_unset(dictionary *cnf, const char *entry);
int cnf_parser_find_entry(dictionary *cnf, const char *entry) ;
dictionary *cnf_parser_load(const char *cnfname);
void cnf_parser_freedict(dictionary *d);

#endif
