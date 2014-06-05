/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */


#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

#include "ansi_setup.h"
#include "gendcls.h"
#include "debug.h"
#include "cnf-parser.h"
#include "pagentd_conf.h"


static BOOL cnf_is_loaded = False;
static dictionary *my_configs = NULL;

/*
 * option definition.
 */
struct option_ele {
    char *group;
    char *key;
    char *default_val;
};

static struct option_ele opt_default_lookup[] = {
    {"global", "syslog_only", "Yes"},
    {"global", "syslog", "1"},
    {"global", "debug_level", "0"},
    {"pagentd", "logfile", "/var/log/policy-agent/pagentd.log"},
    {"pagentd", "max_log_size", "5000"},
    {"pagentd", "daemonize", "yes"},
    {"pagentd", "pidfile", "/tmp/pagentrd.pid"},
    {"pagentd", "port", "8777"},
    {NULL, NULL, NULL}
};

/*
 * Local routines.
 */

/* 
 * get_default - based uopon a key return a default value from the 
 *       opt_default_lookup structure.
 */
static int get_default(const char *k, char **gp, char **dp) 
{
    static char mod[] = "get_default";
    struct option_ele *defp = (struct option_ele *)&opt_default_lookup;
    BOOL retc = 0;

    DBUG_ENTER(mod);
    while (defp->key != NULL) {
        if (!strcasecmp(k, defp->key)) {
            *dp = defp->default_val;
            *gp = defp->group;
            retc = 1;
            break;
        }
        defp++;
    }
    DBUG_RETURN(retc);
}

/*
 * cnf_load() - loades the configs into a dictionary
 */
BOOL pagentd_conf_load(const char *cnf_fname)
{
    static char mod[] = "cnf_load";
    int retc = 0;

    DBUG_ENTER(mod);
    if ((my_configs = cnf_parser_load(cnf_fname)) == NULL) {
        retc = 1;
    }        
    DBUG_RETURN(retc);
}

/*
 * pagentd_conf_isloader()
 */
BOOL pagentd_conf_isloaded(void)
{
    static char mod[] = "pagentd_cnf_loaded";
    int retc = 0;
    
    DBUG_ENTER(mod);
    retc = (my_configs == NULL) ? False: True;
    DBUG_RETURN(retc);
}

/*
 * pagent_conf_dump - dumps the loaded config file.
 *      You must have call the pagantd_conf_load 1st.
 */
void pagentd_conf_dump(void) 
{
    static char mod[] = "pagent_conf_dump";

    DBUG_ENTER(mod);
    if (pagentd_conf_isloaded()) {
        cnf_parser_dump(my_configs, stdout);
    }
    else {
        DEBUG_OUT(0, ("Config file not loaded"));
    }
    /* DBUG_RETURN(NULL); */
}
    
char *pagentd_conf_getitem(const char *key) {
    static char mod[] = "pagent_conf_getitem";
    char real_key[80] = "";
    char *retp = NULL;
    char *gp = NULL;
    char *dp = NULL;

    DBUG_ENTER(mod);
    if (pagentd_conf_isloaded()) {
        if (( !get_default(key, &gp, &dp))) {
            DEBUG_OUT(0, ("Invalid key:[%s]", key));
            retp = NULL; 
        }
        else {
            sprintf(real_key, "%s:%s", gp, key);
            retp = cnf_parser_getstring(my_configs, real_key, dp);
        }
    }
    DBUG_RETURN(retp);
}


