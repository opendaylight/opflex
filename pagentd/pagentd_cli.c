/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

/* 
 * pagentd_cli.c - this handles the command console for admin functions.
 */
#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "gendcls.h"
#include "command-line.h"
#include "compiler.h"
#include "daemon.h"
#include "dirs.h"
#include "dpif.h"
#include "tv-util.h"
#include "fnm-util.h"
#include "util.h"
#include "pagentd_conf.h"
#include "vlog.h"

/*
 * Local dcls
 */
typedef struct _CmdStruct {
    char cmd_char;
    char cmd[32];
    BOOL (*cmdHandler) (char *cmd);
} COMMAND;

static char *monitor_welcome = "*** Policy Agent vers:%s ********";
static char *prompt = "pagent:> ";
static char *cmdp = NULL;
static BOOL connected = False;
static char delim = ' ';


static BOOL genericHandler(char *cl) ;
static BOOL mapHandler(char *cl) ;

static COMMAND *find_command (char *cmd);


static char menuMsg[] = 
  {"\n\n----------------- Test Client Menu --------------------------------\n" \
   "?: [h]elp - display this menu \n"                                   \
   "[s]hutdown  : shutdown the agentd\n"                                \
   "[q]uit      : terminate the monitor\n" 
   };


/*****************************************************************************
 * LOCAL ROUTINES 
 ****************************************************************************/

/*
 * menuHandler - 
 *
 */
static BOOL show_menu(char *cl) 
{
    printf ("Policy Agent CLI  Vers:%s\n" , get_program_version());
    puts(menuMsg);
    return 0;
}

/*
 * quitHandler - 
 *
 */
static BOOL quit(char *cl) 
{
  if (connected == True) {
    /* send the terminate session command */
  }

  DEBUG_OUT(0, ("Terminating client......\n"));
  exit(0);
  return 0;
}

/****************************************************************************
 * The Command Processor
 ***************************************************************************/

/*
 * mon_handler_init
 */
BOOL pagentd_cli_init(void) 
{
    static char mod[] = "mon_handler_init";
    DBUG_ENTER(mod);
    

    DBUG_RETURN(0);

}
/*
 * rl_gets - Read a string, and return a pointer to it.  Returns NULL on EOF. 
 */
char *rl_gets ( void )
{
  /* If the buffer has already been allocated, return the memory
     to the free pool. */
  if (cmdp) {
    free (cmdp);
    cmdp = (char *)NULL;
  }
  
  /* Get a line from the user. */
  cmdp = readline (prompt);
     
  /* If the line has any text in it, save it on the history. */
  if (cmdp && *cmdp)
    add_history (cmdp);
     
  return (cmdp);
}


/*
 * Command lookup structure 
 */
COMMAND cmdLookup[] = { 
  {'h', "menu",        show_menu } ,
  {'q', "quit",        quit } ,
  {'?', "menu",        show_menu } ,
  {'\0', {0}, NULL} 
}; 

/*
 * start_cmd_processing - main command processing loop.
 *
 */
void pagentd_cli_start ( void ) {
  char *cmdline;
  COMMAND *theCmd;
  BOOL retb;
  char cmdl[100];
  
  puts(menuMsg);

  /*
   * the main command loop.
   */
  while (1) {
    cmdline = rl_gets();
    strcpy(cmdl, cmdline);
    theCmd = find_command(cmdline);
    if (!theCmd) 
      printf("%s: No such command!\n", cmdline);
    else {
      retb = theCmd->cmdHandler(cmdl);
    }
  }
}

/*
 * find_command - scans the cmdLookup looking this the entered command.
 *                Returnes the stucture element if found, else NULL.
 */
static COMMAND *find_command( char *cmd) 
{
  int a;
  COMMAND *rtn_cmd = NULL;

  for (a=0; cmdLookup[a].cmd_char != 0; a++) {
    if ((cmdLookup[a].cmd_char == cmd[0])) {
      if ( (cmd[1] != ' ') && (strlen(cmd) > 1) ) {
	if (strncmp(cmd, cmdLookup[a].cmd, strlen(cmdLookup[a].cmd)) == 0) {
	  rtn_cmd = &cmdLookup[a];
	  break;
	}
      }
      else {
	rtn_cmd = &cmdLookup[a];
	break;
      }
    }
  }
  return (rtn_cmd);
} 

