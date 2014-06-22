/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */


#include <config.h>

#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

#include "gendcls.h"
#include "command-line.h"
#include "compiler.h"
#include "daemon.h"
#include "dirs.h"
#include "dpif.h"
#include "dummy.h"
#include "fatal-signal.h"
#include "memory.h"
#include "sig.h"
#include "signals.h"
#include "poll-loop.h"
#include "simap.h"
#include "stream-ssl.h"
#include "stream.h"
#include "tv-util.h"
#include "fnm-util.h"
#include "tv-util.h"
#include "unixctl.h"
#include "util.h"
#include "config-file.h"
#include "modb.h"
#include "policy_enforcer.h"
#include "pol-mgmt.h"
#include "vlog.h"

VLOG_DEFINE_THIS_MODULE(pagentd);

/* 
 * Public Data - Globals
 */
bool g_daemonize = True;
bool g_syslogonly = False;
char g_logfile[64] = {0};

/*
 * Protos
 */
static void usage (void);
static bool init_server( void );
static char *parse_cmdline_options(int argc, char *argv[], char **unixctl_pathp);

/* 
 * Locals
 */
static char policy_agent_msg[] = "Policy Agent";
static char cmdline_buf[256] = {0};
static char *debug_level;

#ifndef USE_TEST_CONFIG
static char default_config_fname[] = "/etc/policy-agent/policy-agent.conf";
#else
static char default_config_fname[] = "../etc/policy-agent/policy-agent.conf";
#endif
static char config_fname[256] = "";

 /* ****************************************************************************
  * main - it all starts here!!
  */
 int
 main (int argc, char *argv[]) 
 {
     char *unixctl_path = NULL;
     char *remote;
     int retc;
     int d;

     proctitle_init(argc, argv);
     set_program_name(argv[0]);
     conf_initialize(NULL);

     /* build up the command line in case of a HUP
      */
     memset(cmdline_buf, 0, sizeof(cmdline_buf)); 
     for (d=0; d < argc; d++) {
         strcat(cmdline_buf, argv[d]);
         strcat(cmdline_buf, " ");
     }

     printf ("%s  Version %s\n", program_name, get_program_version());
     /* do this if there is no definition from the commandline */
     /* strcpy(config_fname, default_config_fname); */
     remote = parse_cmdline_options(argc, argv, &unixctl_path);
     /* load the config file, if not provided on the commandline. */
     if (!conf_file_isloaded()) {
         conf_load(config_fname);
     }
     debug_level = conf_get_value("global", "debug_level");
     if (strcasecmp(debug_level, "OFF")) {
         vlog_set_verbosity(debug_level);
         VLOG_INFO("Debug level: reset to: %s", debug_level);
         conf_dump(stdout);
     }

     init_server();

     /* retc = unixctl_server_create(unixctl_path, &unixctl); */
     if (retc) {
         exit(EXIT_FAILURE);
     }
     /* install signals */
     set_signals();

  
    /* Start the cli */
    /* pagentd_cli_start(); */
    return (0);
}

/* ***************************************************************************
 * parse_cmdline_options() 
 */
static char *
parse_cmdline_options(int argc, char *argv[], char **unixctl_pathp)
{
    enum {
        OPT_PEER_CA_CERT = UCHAR_MAX + 1,
        OPT_MLOCKALL,
        OPT_UNIXCTL,
        VLOG_OPTION_ENUMS,
        OPT_BOOTSTRAP_CA_CERT,
        OPT_ENABLE_DUMMY,
        OPT_DISABLE_SYSTEM,
        DAEMON_OPTION_ENUMS
    };
    static const struct option long_options[] = {
        {"help",        no_argument, NULL, 'h'},
        {"version",     no_argument, NULL, 'V'},
        {"config",      required_argument, NULL,'c'},
        {"unixctl",     required_argument, NULL, OPT_UNIXCTL},
        DAEMON_LONG_OPTIONS,
        VLOG_LONG_OPTIONS,
        STREAM_SSL_LONG_OPTIONS,
        {"peer-ca-cert", required_argument, NULL, OPT_PEER_CA_CERT},
        {"bootstrap-ca-cert", required_argument, NULL, OPT_BOOTSTRAP_CA_CERT},
        {"enable-dummy", optional_argument, NULL, OPT_ENABLE_DUMMY},
        {"disable-system", no_argument, NULL, OPT_DISABLE_SYSTEM},
        {NULL, 0, NULL, 0},
    };
    /* static const struct option longopts[] = { */
    /*     { "verbose",       no_argument, NULL, 'd', }, */
    /*     { "configfile",    required_argument, NULL, 'c', }, */
    /*     DAEMON_LONG_OPTIONS, */
    /*     VLOG_LONG_OPTIONS, */
    /*     { NULL, 0, NULL 0}, */
    /* }; */
    
    char *short_options = long_options_to_short_options(long_options);

    /* Process command line args */ 
    for (;;) {
        int c;
        c = getopt_long(argc, argv, short_options, long_options, NULL);
        if (c == -1) {
            break;
        }

        switch (c) {
        case 'h':
            usage();

        case 'V':
            ovs_print_version(OFP10_VERSION, OFP10_VERSION);
            exit(EXIT_SUCCESS);

        case 'f':
            if (strlen(optarg) > 0) {
                if (!file_exist(optarg)) {
                    VLOG_FATAL("config file: %s does not exist!", optarg);
                }
                strcpy(config_fname, optarg);
            } else {
                strcpy(config_fname, default_config_fname);
            }                
            conf_load(config_fname);
            break;
        
        VLOG_OPTION_HANDLERS
        DAEMON_OPTION_HANDLERS
        STREAM_SSL_OPTION_HANDLERS

        case '?':
            exit(EXIT_FAILURE);

        default:
            abort();

        }
    } 
    free(short_options);

    /* if the config file was not provided on the cmd line load default,
    * if not default, we are done!
    */
    if (strlen(config_fname) == 0) {
        if (!file_exist(default_config_fname)) {
            VLOG_FATAL("No config file found, either provide one on command line or "
                       "create one in default location: %s", default_config_fname);
        } else {
            strcpy(config_fname, default_config_fname);
            conf_load(config_fname);
        }
    }

    argc -= optind;
    argv += optind;

    switch (argc) {
    case 0:
        return xasprintf("unix:%s/db.sock", ovs_rundir());

    case 1:
        return xstrdup(argv[0]);

    default:
        VLOG_FATAL("at most one non-option argument accepted; "
                   "use --help for usage");
    }
}

static void
usage (void)
{
    printf("%s: %s: %s \n"
           "Usage: %s [OPTIONS]\n",
           program_name, get_program_version(), 
           program_name, policy_agent_msg);
    stream_usage("DEAMON", true, false, true);
    daemon_usage();
    vlog_usage();
    printf("\nOther options:\n"
           "  --unixctl=SOCKET        override default control socket name\n"
           "  -h, --help              display this help message\n"
           "  -V, --version           display version information\n");

    exit(EXIT_SUCCESS);
}

/* ***************************************************************************
 * init_server() - initialize the server
 */
typedef struct _CmdStruct { 
    char *title;
    bool (*cmd) (void); 
} INIT_JMP_TABLE; 

/*
 * Everythiong in here gets called int eh init_server
 */
static INIT_JMP_TABLE initialize_funcs[] = {
    MODB_INITIALIZE,
    PM_INITIALIZE,
    PE_INITIALIZE,    
    {NULL, NULL}
};

static bool
init_server(void)
{
    static char *mod = "init_server";
    int retc = 0;
    INIT_JMP_TABLE *cp;

    VLOG_DBG("%s: <---", mod);

    for (cp = initialize_funcs; cp->cmd != NULL; cp++) {
        VLOG_INFO("Initializing: %s ......", cp->title);
        if (cp->cmd()) {
            VLOG_FATAL("Failed initializing: %s", cp->title);
        }
    }
    
    VLOG_DBG("%s: -->retc=%d", mod, retc);
    return (retc);
}






