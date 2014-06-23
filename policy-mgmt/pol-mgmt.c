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
#include <string.h>
#include <stdbool.h>
#include <time.h>

#include "gendcls.h"
#include "coverage.h"
#include "config-file.h"
#include "fnm-util.h"
#include "meo-util.h"
#include "ovs-thread.h"
#include "seq-util.h"
#include "hash-util.h"
#include "tv-util.h"
#include "dirs.h"
#include "pol-mgmt.h"
#include "eventq.h"
#include "vlog.h"
#include "dbug.h"


VLOG_DEFINE_THIS_MODULE(modb);


/*
 * pol-mgmt - Policy Management
 *
 * This represent the front-end communications and general comminications
 * witht he North bound interfaces. Below this is the MODB for which we load
 * with new policies and respond to trigger events from the policy enforcer
 * via the MODB.
 *
 * History:
 *    20-JUN-2014 dkehn@noironetworks.com - created.
 *
 */

static bool pm_initialized = false;
static FILENAME pm_fname;
static char *debug_level;
/*
 * config definitions:
 *  crash_recovery: (true/false): true we look for the $dbdir/pol-mgmt.dat
 *       file and recover from it.
 */


/* config file defaults, this exists such if ther is no definition in
 * the config file then the config option will defualt to what is 
 * defined here.
 */
struct option_ele pm_config_defaults[] = {
    {PM_SECTION, "crash_recovery", "false"},
    {PM_SECTION, "checkpoint_method", "acid"},
    {PM_SECTION, "default_controller", "json:localhost:8888"},
    {PM_SECTION, "pm_debug_level", "INFO"},
    {PM_SECTION, "max_active_sessions", "10"},
    {PM_SECTION, "max_active_sessions", "10"},
    {PM_SECTION, "port", "8085"},
    {NULL, NULL, NULL}
};

/* 
 * Protos dcls
 */
static char *set_debug_level(char *section, char *param);


/******************************************************************************
 * Policy Management code 
 *****************************************************************************/

/* pm_initialize - intialize the internal memory and setups the threads.
 *
 * where:
 *   @return: 0 if successful, else 1
 *
 */
bool pm_initialize(void)
{
    static char *mod= "pm_initialize";
    bool retb = 0;

    ENTER(mod);

    conf_initialize(pm_config_defaults);
    debug_level = set_debug_level(PM_SECTION, "pm_debug_level");
    if (debug_level == NULL) {
        debug_level = "INFO";
        vlog_set_levels_from_string(debug_level);
    }
            
    if (!pm_initialized) {
        
        
        /* 1. setup the session shash, this keeps track of connections that 
         * are active and inactive.
         */

        /* 2. crash recovery? */
        if (!strcasecmp(conf_get_value(PM_SECTION, "crash_recovery"), "true"))  {
            pm_fname = fnm_create(".dat", PM_FNAME, ovs_dbdir(), NULL);
            pm_crash_recovery(fnm_path(pm_fname));
        }

        /* 3. setup the server */

        pm_initialized = true;
    }
    else {
        VLOG_ERR("%s: PM already initialized", mod);
        retb = true;
    }

    LEAVE(mod);
    return(retb);
}

/* pm_is_intiialized - returns the pm_initialized flag.
 */
bool pm_is_initialized(void) 
{
    return(pm_initialized);
}


/* pm_cleanup - 
 *    
 * TODO dkehn: need to persist the data to disk.
 *
 * where
 */
void pm_cleanup(void)
{
    static char *mod = "pm_cleanup";

    ENTER(mod);

    if (pm_initialized) {
        pm_initialized = false;
    }
    LEAVE(mod);
}

/*
 * set_debug_level - 
 * 
 * where:
 * @param0 <section>      - I
 *         section that the variable is.
 * @param1 <param>        - I
 *         name of the paramater.
 * @returns: N/A
 */
static char *set_debug_level(char *section, char *param) 
{
    static char *mod = "set_debug_level";
    char *vl_string;
    char *debug_level = NULL;

    ENTER(mod);
    debug_level = conf_get_value(section, param);
    if (strcasecmp(debug_level, "WARN") || strcasecmp(debug_level, "ERR") ||
        strcasecmp(debug_level, "INFO") || strcasecmp(debug_level, "DBG") ||
        strcasecmp(debug_level, "EMER")) {
        VLOG_ERR("%s: invalid pm_debug_level", mod);
    }
    else {
        vl_string = vlog_set_levels_from_string(debug_level);
        if (vl_string) {
            VLOG_ERR("%s: error trying to set %s, %s", 
                     mod, debug_level, vl_string);
            free(vl_string);
        }
    }
    LEAVE(mod);
    return(debug_level);
}

/* 
 * pm_crash_recovery - restore session list from file.
 *
 * where
 *    <dbfile>     - I
 *       specifics the filename only, will assume the dbdir as the path.
 *       NOTE: only the path is an option form the config file.
 *    <retb.       - O
 *       0 - nothing was loaded, either the file doesn't exists or load error,
 *       in the case of a load error, pm is uneffected.
 *       1 - indicates the pm has been restored.
 */
bool pm_crash_recovery(const char *dbfile)
{
    static char *mod = "pm_crash_recovery";
    bool retb = 0;
    char *dbpath;
    FILENAME dbfname = {0};
    
    ENTER(mod);
    VLOG_INFO("dbfile=%s", dbfile);

    if (pm_initialized) {
        dbpath = (char *)ovs_dbdir();
        if (fnm_exists(dbfname)) {
            VLOG_INFO("%s: loading: %s\n", mod, fnm_path(dbfname));

            /* TODO: dkehn, must build out the crash recovery */
        }
    }
    LEAVE(mod);
    return(retb);
}
