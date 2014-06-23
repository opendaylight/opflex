/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */


#ifndef CONFIG_FILE_H
#define CONFIG_FILE_H 1

#include <stdbool.h>


/*
 * option definition.
 */
struct option_ele {
    char *group;
    char *key;
    char *default_val;
};

bool conf_initialize(struct option_ele *mod_options);
bool conf_file_isloaded(void);
void conf_free(void);
bool conf_load(const char *cnf_fname);
void conf_dump(FILE *fd);
char *conf_get_value(char *section, char *key);

#endif /* config-file.h */
