/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */
#define USE_VLOG 1
#include <config.h>
#include <stdlib.h>
#include "util.h"
#include "cnf-parser.h"
#include "config-file.h"
#include "vlog.h"
#include "dbug.h"

/* 
 * This is the config file management.
 */
VLOG_DEFINE_THIS_MODULE(config_file);

/*
 * TODO dkehn@noironetworks.com - this need to be driver by the 
 * default or the cmdline. For now I'm hard coding it.
 */
static char config_fname[256] = "/usr/local/etc/pagentd.conf";

/*
 * Once you create a config file entry is must be dded here in
 * order to be supported 
*/
static struct option_ele global_options[] = {
    {"global", "sysconfig", "/usr/local/etc/policy-agent"},
    {"global", "pkgdatadir", "/usr/local/share/polict-agent"},
    {"global", "rundir", "/usr/local/var/run/policy-agent"},
    {"global", "logdir", "/usr/local/var/log/policy-agent"},
    {"global", "dbdir", "/usr/local/etc/policy-agent"},
    {"global", "bindir", "/usr/local/bin"},
    {"global", "debug_level", "OFF"},
    {NULL, NULL, NULL}
};

static struct option_ele *opt_default_lookup = NULL;
static int opt_default_lookup_count = 0;
static dictionary *my_configs = NULL;

/* 
 * Prots 
 */
static char *conf_getitem(const char * group, const char *key);
static char *get_default(const char *sp, const char *k);


/* 
 * get_default - based uopon a key return a default value from the 
 *       opt_default_lookup structure.
 */
static char *get_default(const char *sp, const char *k)
{
    struct option_ele *defp = opt_default_lookup;

    while (defp->key != NULL) {
        if (!strcasecmp(sp, defp->group) && !strcasecmp(k, defp->key)) {
            break;
        }
        defp++;
    }
    return(defp->default_val);
}

static char *conf_getitem(const char * group, const char *key) {
    char real_key[80] = "";
    char *retp = NULL;
    char *dp = NULL;

    if (conf_file_isloaded()) {
        if (!conf_load(config_fname)) {
            dp = get_default(group, key);
            if (dp = NULL) {
                VLOG_WARN("conf_getitem: Invalid key:[%s]", key);
                retp = NULL; 
            }
            else {
                sprintf(real_key, "%s:%s", group, key);
                retp = cnf_parser_getstring(my_configs, real_key, dp);
            }
        }
    }
    return (retp);
}

static int option_count(struct option_ele *op)
{
    int mod_opt_cnt;

    for (mod_opt_cnt = 0; op->group != NULL; op++, mod_opt_cnt++);
    return (mod_opt_cnt);
}

/* 
 * Global routines
 */

/* 
 * initialize the config defaults list 
 *
 * where:
 * @param0 <mod_options>       - I
 *         a modules default options that are added to the 
 *         centralize configureation options. If NULL it will
 *         create the opt_default_config with the globals only.
 */
bool conf_initialize(struct option_ele *mod_options)
{
    static char *mod = "conf_initialize";
    struct option_ele *op = mod_options;
    struct option_ele *gp;
    int cnt;

#ifndef USE_VLOG
    DBUG_PROCESS("config");
#endif
    ENTER(mod);
    /* if its not been initialized then create it with the globals */
    if (opt_default_lookup == NULL) {
        DBUG_PRINT("DEBUG", ("adding globals"));
        cnt = option_count(global_options);
        opt_default_lookup = xzalloc((cnt+1) * sizeof(struct option_ele));
        opt_default_lookup_count = cnt+1;
        bzero(opt_default_lookup, (cnt+1) * sizeof(struct option_ele));
        memcpy(opt_default_lookup, global_options, (cnt * sizeof(struct option_ele)));
    }
    if (mod_options != NULL) {
        DBUG_PRINT("DEBUG", ("adding %s", mod_options->group));
        cnt = option_count(mod_options);
        opt_default_lookup = xrealloc(opt_default_lookup, 
                      (sizeof(struct option_ele) *
                       (opt_default_lookup_count + cnt)));
        
        /* copy the new options into the default */
        for (gp = (opt_default_lookup+cnt-1); cnt > 0; cnt--, gp++, op++) {
            memcpy(gp, op, sizeof(struct option_ele));
            opt_default_lookup_count++;
        }
        /* last cell NULLs */
        gp->group = gp->key = gp->default_val = NULL;
    }
    LEAVE(mod);
    return(0);
}

const char *conf_get_value(char *section, char *key)
{
    static char *mod = "conf_get_value";
    char real_key[80] = "";
    char *dp = NULL;
    char *retp;

    ENTER(mod);
#ifdef USE_VLOG
    VLOG_DBG("(section=%s, key=%s)", section, key);
#else
    DBUG_PRINT("DEBUG", ("(section=%s, key=%s)", section, key));
#endif
    /* get the default, becuse we always need it for
       cnf_parser_getstring. */
    if ((dp = get_default(section, key)) == NULL) {
        DBUG_PRINT("WARN", ("Invalid key:[%s]", key));
        return (NULL);
    }

    sprintf(real_key, "%s:%s", section, key);

    if (my_configs) {
        retp = cnf_parser_getstring(my_configs, real_key, dp);
    } else {
        retp = dp;
    }
    LEAVE(mod);
    return(retp);
}

bool conf_file_isloaded(void)
{
    return ((my_configs == NULL) ? 0: 1);
}

bool conf_load(const char *cnf_fname)
{
    int retc = 0;

    VLOG_INFO("conf_load loading %s", cnf_fname);
    if ((my_configs = cnf_parser_load(cnf_fname))) {
        memset(config_fname, sizeof(config_fname), 0);
        strcpy(config_fname, cnf_fname);
        retc = 1;
    } else {
        VLOG_ERR("conf_load failed, can't load %s", cnf_fname);
        retc = 0;
    }
    return (retc);
}

void conf_dump(FILE *fd) {
    if (conf_file_isloaded()) {
        conf_load(config_fname);
    }
    cnf_parser_dump(my_configs, fd);
}

void conf_free(void)
{
    if (my_configs) {
        cnf_parser_freedict(my_configs);
    }
}

