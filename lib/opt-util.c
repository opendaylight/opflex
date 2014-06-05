/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

/*
 *
 * @file opt_util.c - Command Line Option Procesing Utility Functions.
 *
 * @brief This file contains a package of functions used to scan and parse
 *    UNIX-style command line arguments.  The OPT_UTIL utilities support:
 *        (1) single-letter options; e.g., "-a"
 *        (2) single-letter options expecting arguments; e.g., "-o <file>"
 *        (3) name options; e.g., "-readonly"
 *        (4) name options with arguments; e.g., "-list <file>"
 *        (5) redirection of standard input and output on systems that
 *            don't support it at the shell level; e.g., "> output_file"
 *
 *    @remark The scanning algorithm used by the OPT_UTIL functions allows name options
 *    to be abbreviated on the command line.  For example, the "-list <file>"
 *    name option in (4) above can be entered on the command line as "-l <file>",
 *    "-li <file>", "-lis <file>", or "-list <file>".  If the one-character
 *    abbreviation of a name conflicts with a single-letter option, the single-
 *    letter option takes priority.
 *
 *    The following command line
 *
 *        % program -a -o <object_file> -readonly -list <file>
 *
 *    would be scanned and parsed as follows using the OPT_UTIL package:
 *
 *        #include  "opt_util.h"
 *        char  *argument;
 *        int  option;
 *
 *        opt_init (argc, argv, 0, "ao:{readonly}{object:}", NULL);
 *        while (option = opt_get (NULL, &argument)) {
 *            switch (option) {
 *            case 1:  ... process "-a" option ...
 *            case 2:  ... process "-o <file>" option argument ...
 *            case 3:  ... process "-readonly" option ...
 *            case 4:  ... process "-list <file>" option ...
 *            case NONOPT:
 *                ... process non-option argument ...
 *            case OPTERR:
 *                ... invalid option or missing argument ...
 *            }
 *        }
 *
 *    Applications that will scan multiple command lines using the same set of
 *    allowable options can cache the original scan context, resetting it for
 *    each new command line (represented by a new ARGC/ARGV set of arguments):
 *
 *        static  char  *optionList[] = {		-- Command line options.
 *            "a", "o:", "{readonly}", "{object:}", NULL
 *        };
 *        static  OptContext  scan = NULL;
 *        ...
 *        for (... each new ARGC/ARGV set of arguments ...) {
 *            if (scan == NULL)
 *                opt_init (argc, argv, 1, optionList, &scan);
 *            opt_reset (scan, argc, argv);
 *            while (option = opt_get (scan, &argument)) {
 *                ... process options ...
 *            }
 *        }
 *
 *    Note in the example above that the valid command line options are
 *    specified in list form rather than in-line.
 *
 *    
 * Procedures:
 *
 *    OPT_CREATE_ARGV - creates an ARGV array of arguments for a command line.
 *    OPT_DELETE_ARGV - deletes an ARGV array of arguments.
 *    OPT_ERRORS - enables/disables the display of error messages.
 *    OPT_GET - gets the next option (and its argument) from the command line.
 *    OPT_INDEX - returns the index of the current option or its argument.
 *    OPT_INIT - initializes a command line scan.
 *    OPT_NAME - returns the name of an option returned by OPT_GET().
 *    OPT_RESET - resets a command line scan.
 *    OPT_SET - sets the option/argument index of a command line scan.
 *    OPT_TERM - terminates a command line scan.
 *
 */


#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>   
#include <stdarg.h>

#include <gendcls.h> 
#include <str-util.h>
#include <debug.h>
#include <get-util.h>
#include <opt-util.h>           


/*
 *    Options Scan Context - stores information about a command line scan.
 */
typedef  struct  _OptContext {
    char  *letterOptions;		/* String containing letter options. */
    int  *letterIndex;			/* Ord of letter options in orig opt string. */
    int  numNames;			/* # of name options. */
    char  **nameOptions;		/* Pointer to array of name options. */
    int  *nameIndex;			/* Ord of name options in orig opt string. */
    int  printErrors;			/* Should error messages be printed? */
    int  argc;				/* The number of arguments. */
    char  **argv;			/* The arguments. */
    int  *arglen;			/* The length of each argument. */
    int  endOfOptions;			/* Index of end-of-options marker. */
    int  current;			/* Index of current argument in command. */
    int  offset;			/* Offset within current argument. */
    enum  {
        none,
        invalid_option, missing_argument,
        io_redirection,
        letter, letter_with_argument,
        name, name_with_argument,
        non_option_argument
    }  optionType;			/* Type of current option. */
    char  *nameString;			/* Return value from OPT_NAME(). */
}  _OptContext;

static  OptContext  defaultContext = NULL;

int  opt_util_debug = 0;		/* Global debug switch (1/0 = yes/no). */

/*
 * opt_create_argv () Create an ARGV Array of Arguments for a Command Line.
 *    Function OPT_CREATE_ARGV builds an ARGV array of arguments for a
 *    command line string.  For example, the command line
 *
 *             "program -a -o <object_file> -readonly -list <file>"
 *
 *    would be broken up into the following arguments:
 *
 *                ARGV[0] = "program"
 *                ARGV[1] = "-a"
 *                ARGV[2] = "-o"
 *                ARGV[3] = "<object_file>"
 *                ARGV[4] = "-readonly"
 *                ARGV[5] = "-list"
 *                ARGV[6] = "<file>"
 *
 *    OPT_CREATE_ARGV() is intended for the processing of internally-generated
 *    command strings.  Given such a command string, a program would:
 *
 *        (1) Call OPT_CREATE_ARGV() to create an ARGV array of arguments.
 *        (2) Call OPT_INIT() to initilize a scan of these arguments.
 *        (3) Call OPT_GET() for each option/argument.
 *        (4) Call OPT_TERM() to terminate the scan.
 *        (5) Call OPT_DELETE_ARGV() to delete the ARGV array built by
 *            OPT_CREATE_ARGV().
 *
 *
 *    Invocation:
 *        status = opt_create_argv (program, command, &argc, &argv);
 *
 *    where:
 *        <program>	-I
 *            is the argument (usually the program name) assigned to ARGV[0];
 *            the first argument in the command string (see COMMAND) will be
 *            assigned to ARGV[1].  If PROGRAM is NULL, the first argument in
 *            COMMAND is assigned to ARGV[0].
 *        <command>	- I
 *            is the command line string from which an ARGV array of arguments
 *            will be built.
 *        <argc>		- O
 *            returns the number of arguments on the command line.
 *        <argv>		- O
 *            returns a pointer to an ARGV array of arguments.  This is simply
 *            an array of (CHAR *) pointers to argument strings.
 *        <status>	- O
 *            returns the status of constructing the argument array, zero if
 *            no errors occurred and ERRNO otherwise.
 *
 */

int opt_create_argv (const char *program, const char *command, 
                     int *argc, char *(*argv[]))
{
    char  *s;
    int  i, length;
    static  char  *quotes = "\"'{";

    if (command == NULL)
        command = "";

    /* Scan the command string and determine the number of arguments on the line. */
    i = (program == NULL) ? 0 : 1;
    length = 0;
    s = (char *) getstring (command, quotes, &length);
    while (length > 0) {
        i++;  s = (char *) getstring (s, quotes, &length);
    }
    *argc = i;

    /* Allocate an ARGV array to hold the arguments. */
    *argv = (char **) malloc ((*argc + 1) * sizeof (char *));
    if (*argv == NULL) {
        vperror (3, "(opt_create_argv) Error allocating ARGV array of %d elements.\nmalloc: ",
                 *argc);
        return (errno);
    }

    /* Scan the command string again, this time copying the arguments into the
       ARGV array. */
    i = 0;
    if (program != NULL)  (*argv)[i++] = strdup (program);
    length = 0;
    s = (char *) getstring (command, quotes, &length);
    while (length > 0) {
        (*argv)[i++] = strDestring (s, length, quotes);
        s = (char *) getstring (s, quotes, &length);
    }
    (*argv)[i] = NULL;

    if (opt_util_debug) {
        printf ("(opt_create_argv) %d arguments:\n", *argc);
        for (i = 0;  i < *argc;  i++)
            printf ("                      [%d] = \"%s\"\n", i, (*argv)[i]);
    }

    return (0);
}

/*
 *    opt_delete_argv () Delete an ARGV Array of Arguments.
 *
 *    Function OPT_DELETE_ARGV deletes an ARGV array of arguments originally
 *    built by OPT_CREATE_ARGV().
 *
 *
 *    Invocation:
 *        status = opt_delete_argv (argc, argv);
 *
 *    where:
 *        <argc>		- I
 *            is the number of arguments in the ARGV array.
 *        <argv>		- I
 *            is an ARGV array of arguments built by OPT_CREATE_ARGV().
 *        <status>	- O
 *            returns the status of deleting the ARGV array, zero if no
 *            errors occurred and ERRNO otherwise.
 *
 */
int opt_delete_argv(int argc, char *argv[])
{
    int i;

    /* Deallocate the individual argument strings. */
    for (i = 0;  argv[i] != NULL;  i++)
        free (argv[i]);

    /* Deallocate the ARGV array. */
    free (argv);

    if (opt_util_debug)
        printf ("(opt_delete_argv) Deleted %d arguments:\n", i);

    return (0);
}

/*
 *    opt_errors () - Enable/Disable Display of Error Messages.
 *
 *    Function OPT_ERRORS enables/disables the display of error messages when
 *    a command line error is detected.  Messages for invalid options and missing
 *    options are normally written to standard error when detected.
 *
 *
 *    Invocation:
 *        status = opt_errors (scan, displayFlag);
 *
 *    where:
 *        <scan>		- I
 *            is the command line scan context originally created by OPT_INIT().
 *            If this argument is NULL, the default scan context is used.
 *        <displayFlag>	- I
 *            if true (a non-zero value), the display of error messages is
 *            enabled.  If this argument is false (zero), error messages will
 *            not be displayed.  This flag only affects the specified scan
 *            context.
 *        <status>	- O
 *            returns the status of enabling or disabling the display of error
 *            messages, zero if no errors occurred and ERRNO otherwise.
 *
 */
int opt_errors(OptContext scan, int displayFlag)
{
    if (scan == NULL)
        scan = defaultContext;
    if (scan == NULL)
        return (EINVAL);

    /* Set the error display flag in the scan context structure. */
    scan->printErrors = displayFlag;
    if (opt_util_debug)  printf ("(opt_errors) Error messages are %s.\n",
                                 displayFlag ? "enabled" : "disabled");
    return (0);
}

/*
 *    opt_get () - Get the Next Option/Argument from the Command Line.
 *
 *    Function OPT_GET gets the next option and its argument (if there is one)
 *    from the command line.
 *
 *    Name options can be abbreviated on the command line.  For example,
 *    a "-flag" option could be entered on the command line as "-f", "-fl",
 *    "-fla", or "-flag".  If, however, the one-character abbreviation of
 *    a name option (e.g., "-f" for "-flag") conflicts with a single-letter
 *    option (e.g., "-f"), the single-letter option takes priority.
 *
 *    The following options receive special handling:
 *
 *        "-"     A hyphen by itself is returned to the calling routine as
 *                a non-option argument.
 *
 *        "--"    Two hyphens indicate the end of option processing; the
 *                remaining arguments on the command line are treated as
 *                non-option arguments.
 *
 *        "< input_file"
 *        "> output_file"
 *                These symbols cause UNIX shell-style I/O redirection.
 *
 *
 *   Invocation:
 *        index = opt_get (scan, &argument);
 *
 *    where:
 *        <scan>		- I
 *            is the command line scan context created by OPT_INIT().  If
 *            this argument is NULL, the default scan context is used.
 *        <argument>	- O
 *            returns a (CHAR *) pointer to the next option's argument.  NULL
 *            is returned for single-letter options without arguments and
 *            name options without arguments.  If an invalid option is
 *            encountered and OPTERR is returned as the INDEX (see below),
 *            then ARGUMENT returns a pointer to the option in error.
 *        <index>		- I
 *            returns the index (1..N) of the next option; i.e., is this option
 *            the 1st, 2nd, or Nth option in the set of options specified in
 *            the call to OPT_INIT()?  For example, if "ab:{flag}c" are the
 *            allowable options, then the appearance of "-c" on the command
 *            line will be returned by OPT_GET() as an index of 4.  OPTEND
 *            (zero) is returned when the command line scan is complete;
 *            OPTERR is returned for invalid options and missing arguments.
 *
 */
int opt_get(OptContext scan, char **argument)
{ 
    char  *group, *s;
    int  i, optionIndex = OPTERR;

    *argument = NULL;
    if (scan == NULL)  scan = defaultContext;
    if (scan == NULL) {
        errno = EINVAL;
        vperror (3, "(opt_get) Null scan context.\n");
        return (OPTERR);
    }

    /*
     * The CURRENT and OFFSET values in the scan context index the last option
     * or argument returned by OPT_GET().  Depending on the type of the last
     * option, increment these values to index the next option on the command
     * line.
     */

    switch (scan->optionType) {
    case none:
        /* Beginning of scan: position set by OPT_INIT() or OPT_RESET(). */
        /* End of scan: don't advance. */
        break;
    case invalid_option:
        if ((scan->numNames > 0) && (scan->offset == 1)) {
            scan->current++;			/* Skip invalid name option. */
            scan->offset = 0;
            break;
        } else {				/* Skip invalid letter option. */
            /* Continue on into the "letter" case. */
        }
    case letter:
        scan->offset++;
        if (scan->argv[scan->current][scan->offset] == '\0') {
            scan->current++;  scan->offset = 0;
        }
        break;
    case missing_argument:
        /* The option expecting an argument was either at the end of the
           command line (e.g., "command ... -b") or was followed by another
           options group (e.g., "command ... -b -c").  In the latter case,
           stay put; CURRENT is already set for the new options group. */
        scan->offset = 0;
        break;
    case io_redirection:
    case letter_with_argument:
    case name:
    case name_with_argument:
    case non_option_argument:
    default:
        scan->current++;  scan->offset = 0;
        break;
    }

    /*
     * Scan the command line and return the next option or, if none, the next
     *  non-option argument.  At the start of each loop iteration, the CURRENT
     * and OFFSET fields in the scan context structure index the next option/
     * argument to be examined.
     */
    for ( *argument = NULL;  scan->current < scan->argc;  *argument = NULL) {
        group = scan->argv[scan->current];


        /*
         * If the current option group begins with '-', then check for the 
         * special hyphen cases ("-" and "--") and for single-letter or 
         * name options.
         */
        if ((group[0] == '-') && (scan->current < scan->endOfOptions)) {

            /* Check for the special options, "-" and "--".  Return "-" 
               as a non-option argument.
               For the "--" end-of-options indicator, remember the current
               position on the command line so that the remaining 
               arguments are all treated as non-option arguments. */
            if (scan->offset == 0) {
                if (group[1] == '\0') {			/* "-" */
                    scan->optionType = non_option_argument;
                    *argument = group;
                    return (NONOPT);
                } else if ((group[1] == '-') &&		/* "--" */
                           (group[2] == '\0')) {
                    scan->endOfOptions = scan->current++;
                    continue;
                } else {				/* "-xyz" */
                    scan->offset++;
                }
            }

            /* Are we at the end of the current options group?  If so, 
               loop and try the
               next command line argument. */
            else if (group[scan->offset] == '\0') {
                scan->current++;  scan->offset = 0;
                continue;
            }

            /* If we're at the beginning of the options group, check if a name
               option was specified.  Using STRNCMP() (length-restricted 
               comparison) to match name options means that name options 
               can be abbreviated.  For example,  expected option "{name}" 
               will be matched by any of the following on the
               command line: "-n", "-na", "-nam", or "-name". */
            scan->optionType = none;

            if (scan->offset == 1) {
						/* Scan the name list. */
                for (i = 0;  i < scan->numNames;  i++) {
                    if (strncmp (&group[1], scan->nameOptions[i],
                                 scan->arglen[scan->current] - 1) == 0)
                        break;
                }
                /* Found and no conflict with single-letter option? */
                if ((i < scan->numNames) &&
                    ((scan->arglen[scan->current] > 2) ||
                     (strchr (scan->letterOptions, group[1]) == NULL))) {
                    optionIndex = scan->nameIndex[i];
                    if (optionIndex < 0) {
                        scan->optionType = name_with_argument;
                        optionIndex = -optionIndex;
                    }
                    else {
                        scan->optionType = name;
                    }
                }
            }

			/* Otherwise, check for a single-letter option. */
            if ((scan->optionType == none) &&
                (s = strchr (scan->letterOptions, group[scan->offset]))) {
                i = s - scan->letterOptions;
                optionIndex = scan->letterIndex[i];
                if (optionIndex < 0) {
                    scan->optionType = letter_with_argument;
                    optionIndex = -optionIndex;
                } else {
                    scan->optionType = letter;
                }
            }

			/* If the current option is not one of the allowed 
               single-letter or name
               options, then signal an error. */
            if (scan->optionType == none) {
                if (scan->printErrors) {
                    if ((scan->numNames > 0) && (scan->offset == 1)) {
                        fprintf (stderr, "%s: illegal option -- %s\n",
                                 scan->argv[0], &group[scan->offset]);
                    } else {
                        fprintf (stderr, "%s: illegal option -- %c\n",
                                 scan->argv[0], group[scan->offset]);
                    }
                }
                scan->optionType = invalid_option;
                *argument = &group[scan->offset];
                return (OPTERR);
            }

        }  /* If the option group begins with a hyphen ('-') */

		/*
          The current options group does NOT begin with a hyphen.  Check for
          I/O redirection or a non-option argument. */

        else {
			/* Check for I/O redirection, indicated by "<" (input) or ">" 
               (output) characters. */
            if ((scan == defaultContext) &&
                ((group[0] == '<') || (group[0] == '>'))) {
                scan->optionType = io_redirection;
            }

			/* This is a non-option argument.  Return the argument to 
               the calling routine. */
            else {
                scan->optionType = non_option_argument;
                *argument = group;	/* Return NONOPT and argument. */
                return (NONOPT);
            }

        }     /* If the option group does NOT begin with a hyphen ('-') */

		/*
          This point in the code is reached if a single-letter or name option 
          was specified, or if I/O redirection was requested.
          If an argument is expected (because the option expects one or if I/O 
          is being redirected to a file), then get the argument.  For single-
          letter options and I/O redirection, the argument may be flush up 
          against the option (i.e., the argument is the remainder of the 
          current ARGV) or it may be separated from the option by white space
          (i.e., the argument is the whole of the next ARGV).  For name 
          options, the argument is always found in the next ARGV. */
        switch (scan->optionType) {
        case io_redirection:
        case letter_with_argument:
            scan->offset++;
            if (group[scan->offset] == '\0') {		/* Argument separated by space? */
                scan->current++;
                if ((scan->current >= scan->argc) ||	/* Missing argument? */
                    (*scan->argv[scan->current] == '-')) {
                    if (scan->printErrors)
                        fprintf (stderr, "%s: option requires an argument -- %c\n",
                                 scan->argv[0], group[scan->offset-1]);
                    scan->optionType = missing_argument;
                    *argument = &group[scan->offset-1];
                    return (OPTERR);
                }
                scan->offset = 0;
            }
            *argument = &scan->argv[scan->current][scan->offset];
            break;

        case name_with_argument:
            scan->current++;
            if ((scan->current >= scan->argc) ||	/* Missing argument? */
                (*scan->argv[scan->current] == '-')) {
                if (scan->printErrors)
                    fprintf (stderr, "%s: option requires an argument -- %s\n",
                             scan->argv[0], &group[1]);
                scan->optionType = missing_argument;
                *argument = &group[1];
                return (OPTERR);
            }
            *argument = scan->argv[scan->current];
            break;

        default:
            *argument = NULL;
            break;

        }


		/* If I/O redirection was requested, then redirect standard input or standard
   			output to the specified file. */
        if (scan->optionType == io_redirection) {

            switch (group[0]) {
            case '<':
                if (freopen ((const char *)*argument, "r", stdin) == NULL) {
                    if (scan->printErrors) {
                        fprintf (stderr, "%s: unable to redirect input from %s\n",
                                 scan->argv[0], (char *)*argument);
                        perror ("freopen");
                    }
                    return (OPTERR);
                }
                break;
            case '>':
                if (freopen ((const char *)*argument, "w", stdout) == NULL) {
                    if (scan->printErrors) {
                        fprintf (stderr, "%s: unable to redirect output to %s\n",
                                 scan->argv[0], *argument);
                        perror ("freopen");
                    }
                    return (OPTERR);
                }
                break;
            default:
                break;
            }

            scan->current++;			/* Position to next argument. */
            scan->offset = 0;
            continue;				/* Loop for next option. */

        }

		/*
          At last!  A valid option and (optionally) its argument have been located.
          Return it (or them) to the calling routine.
        */
        return (optionIndex);

    }     /* For each argument on the command line */

	/* The end of the command line has been reached.  Signal the calling routine
   		that there are no more arguments. */

    return (OPTEND);
}

/*
 *    opt_index () Return Position in Command Line Scan.
 *
 *    Function OPT_INDEX returns the index of the current option/argument in a
 *    command line scan.  The index is just the current position in the ARGV
 *    argument array and has a value between 1 and ARGC-1, inclusive.
 *
 *
 *    Invocation:
 *        index = opt_index (scan);
 *
 *    where:
 *        <scan>		- I
 *            is the command line scan context originally created by OPT_INIT().
 *            If this argument is NULL, the default scan context is used.
 *        <index>		- O
 *            returns the index, 1..ARGC-1, of the current option/argument.
 *            Zero is returned if the command line scan is complete; -1 is
 *            returned in case of an error (e.g., no scan context).
 *
 */
int opt_index (OptContext scan)
{
    if (scan == NULL)
        scan = defaultContext;
    if (scan == NULL)
        return (-1);

    if (scan->current >= scan->argc)
        return (0);			/* End of command line scan. */
    else if (scan->current < 1)
        return (-1);			/* Error. */
    else
        return (scan->current);		/* Index of current option/argument. */
}

/*
 *    opt_init () - Initialize Command Line Scan.
 *
 *    Function OPT_INIT initializes a command line scan.  A context structure,
 *    created and initialized by OPT_INIT(), will keep track of the progress
 *    of the scan.  A (VOID *) pointer to the context structure is returned
 *    to the calling routine for use in subsequent OPT_GET() calls.
 *
 *
 *    Invocation (with a single option string):
 *        char  *optionString = "...";
 *        int  isList = 0;
 *        status = opt_init (argc, argv, isList, optionString, &scan);
 *
 *    Invocation (with an option string array):
 *        char  *optionList[]  = { "...", "...", ..., NULL };
 *        int  isList = 1;
 *        status = opt_init (argc, argv, isList, optionList, &scan);
 *
 *    where:
 *        <argc>		- I
 *            specifies the number of arguments in the command line's ARGV array.
 *        <argv>		- I
 *            is an array of (CHAR *) pointers to the command line's arguments.
 *        <isList>	- I
 *            specifies whether the allowable options are specified in a single
 *            option string or in a NULL-terminated list of options strings.
 *            If ISLIST is false (zero), OPT_INIT()'s next argument should be
 *            a simple character string specifying the options.  If ISLIST is
 *            true (non-zero), OPT_INIT()'s next argument should be an array of
 *            character strings, each specifying one or more of the allowable
 *            options; the last element of the array must be a list-terminating
 *            NULL pointer.
 *        <optionString>	- I
 *        <optionList>	- I
 *            specify the possible command line options.  If ISLIST is false
 *            (zero), a single option string should be passed to OPT_INIT().
 *            If ISLIST is true (non-zero), a NULL-terminated array of option
 *            strings should be passed to OPT_INIT().  Within an option string,
 *            options are specified as followed:
 *                    "a"		Single-letter option
 *                    "b:"	Single-letter option expecting an argument
 *                    "{flag}"	Name option
 *                    "{list:}"	Name option expecting an argument
 *        <scan>		- O
 *            returns a handle for this command line scan's context.  If this
 *            argument is NULL, then a default context is used.
 *
 */

int opt_init(int argc, char *argv[], int isList, ...)
{
    char mod[] = "opt_init";
    char  *name, *optionString, *optionStringList, *s;
    int  i, length, numLetters, numStrings, position;
    OptContext  *scan;
    va_list  ap;

	/* If the IS_LIST argument is false (zero), then the option string argument
   	is a normal (CHAR *) string pointer.  If IS_LIST is true (non-zero), then
   	the option string argument is a pointer to an array of (CHAR *) string
   	pointers; the last pointer in the array should be NULL, thus indicating
   	the end of the list. */

    va_start (ap, isList);

    if (isList) {
        optionStringList = va_arg (ap, char *);
        for (i = 0, numStrings = 0;  optionStringList+i != NULL;  i++)
            numStrings++;
    } else {
        optionString = va_arg (ap, char *);
        optionStringList = (char *)&optionString;
        numStrings = 1;
    }
    scan = va_arg (ap, OptContext *);
    va_end (ap);


    /* Allocate a scan context block for the new options scan.  If the caller
       does not want the context returned, then save it as the default context.
    */
    if (scan == NULL) {
        if (defaultContext != NULL)
            opt_term (defaultContext);
        scan = &defaultContext;
    }

    *scan = (OptContext) malloc (sizeof (_OptContext));
    if (scan == NULL) {
        vperror (0, "(%s) Error allocating scan context block.\nmalloc: ", mod);
        return (errno);
    }

    (*scan)->letterOptions = NULL;
    (*scan)->numNames = 0;
    (*scan)->nameOptions = NULL;
    (*scan)->printErrors = 1;
    (*scan)->argc = argc;
    (*scan)->argv = argv;
    (*scan)->arglen = NULL;
    (*scan)->endOfOptions = argc + 1;
    (*scan)->current = 1;
    (*scan)->offset = 0;
    (*scan)->optionType = none;
    (*scan)->nameString = NULL;


	/* Now, scan the option string(s) and count the number of single-letter
   		options and the number of name options. */

    numLetters = 0;  (*scan)->numNames = 0;
    for (i = 0;  i < numStrings;  i++) {
        s = optionStringList+i;
        if (s == NULL)  break;
        while (*s != '\0') {			/* Scan the option string. */
            if (*s == '{') {			/* Name option? */
                (*scan)->numNames++;
                name = s;
                s = strchr (s, '}');
                if (s == NULL)
                    s = name + strlen (name);	/* Skip to end of string. */
                else
                    s++;			/* Advance past "}". */
            } else {				/* Single-letter option? */
						/* Count letters. */
                if (*s++ != ':')  numLetters++;
            }
        }     /* For each character in the option string */
    }     /* For each option string */


	/* Allocate a string to hold the single-letter options and an array to hold
   		the index of each option.  The index array is used to remember each option's
   		position in the original options string. */

    (*scan)->letterOptions = (char *) malloc (numLetters+1);
    if ((*scan)->letterOptions == NULL) {
        vperror (0, "(%s) Error allocating string to hold %d single-letter options.\nmalloc: ",
                 mod, numLetters);
        return (errno);
    }
    (*scan)->letterOptions[numLetters] = '\0';

    (*scan)->letterIndex = (int *) malloc (numLetters * sizeof (int));
    if ((*scan)->letterIndex == NULL) {
        vperror (0, "(%s) Error allocating array to hold %d single-letter indices.\nmalloc: ",
                 mod, numLetters);
        return (errno);
    }


	/* Allocate an array to hold the name options and an array to hold the index
   		of each option.  The index array is used to remember each option's position
   		in the original options string. */
    (*scan)->nameOptions = (char **) malloc ((*scan)->numNames *
                                             sizeof (char *));
    if ((*scan)->nameOptions == NULL) {
        vperror (0, "(%s) Error allocating array to hold %d name options.\nmalloc: ",
                 mod, (*scan)->numNames);
        return (errno);
    }

    (*scan)->nameIndex = (int *) malloc ((*scan)->numNames * sizeof (int));
    if ((*scan)->nameIndex == NULL) {
        vperror (0, "(%s) Error allocating array to hold %d name indices.\nmalloc: ",
                 mod, (*scan)->numNames);
        return (errno);
    }

	/* Finally, scan the option strings again, copying single-letter
       options and name options to the appropriate fields in the context 
       structure. */
    numLetters = 0;  (*scan)->numNames = 0;  position = 1;

    for (i = 0;  i < numStrings;  i++) {

        s = optionStringList+i;
        if (s == NULL)  break;

        while (*s != '\0') {			/* Scan the option string. */
            if (*s == '{') {			/* Name option? */
                name = s + 1;
                s = strchr (s, '}');
                length = (s == NULL) ? strlen (name) : (s - name);
                if (name[length-1] == ':') {
                    length--;
                    (*scan)->nameIndex[(*scan)->numNames] = -position++;
                } else {
                    (*scan)->nameIndex[(*scan)->numNames] = position++;
                }
                /* Wastefully duplicate name. */
                (*scan)->nameOptions[(*scan)->numNames] = strdup (name);
                (*scan)->nameOptions[(*scan)->numNames++][length] = '\0';
                if (s == NULL)
                    s = name + strlen (name);	/* Skip to end of string. */
                else
                    s++;			/* Advance past "}". */
            }
            else {				/* Single-letter option? */
						/* Copy letters. */
                (*scan)->letterOptions[numLetters] = *(s++);
                if (*s == ':') {
                    (*scan)->letterIndex[numLetters] = -(position++);
                    s++;
                } else {
                    (*scan)->letterIndex[numLetters] = position++;
                }
                numLetters++;
            }
        }     /* For each character in the option string */
    }     /* For each option string */

	/* If there are name options, then call OPT_RESET() to construct a table
   		containing the length of each argument in the ARGV array.  Doing so
   		now will save OPT_GET() the trouble of doing so when trying to match
   		abbreviated name options. */
    if (opt_reset (*scan, argc, argv)) {
        vperror (0, "(opt_init) Error building table of argument lengths.\nopt_reset: ");
        return (errno);
    }

    if (opt_util_debug) {
        printf ("(opt_init) Single-letter options = \"%s\"\n",
                (*scan)->letterOptions);
        for (i = 0;  (*scan)->letterOptions[i] != '\0';  i++) {
            printf ("(opt_init) Letter option (%d) = '%c'\n",
                    (*scan)->letterIndex[i], (*scan)->letterOptions[i]);
        }
        for (i = 0;  i < (*scan)->numNames;  i++)
            printf ("(opt_init) Name option (%d) = \"%s\"\n",
                    (*scan)->nameIndex[i], (*scan)->nameOptions[i]);
    }
    return (0);
}

/*
 *    opt_name () - Return Name of Option Returned by OPT_GET().
 *
 *    Function OPT_NAME returns the name corresponding to an option number
 *    returned by OPT_GET().  For the current option on the command line,
 *    OPT_GET() returns the position, 1..N, of that option within the
 *    set of options specified when OPT_INIT() was called.  OPT_NAME()
 *    simply returns the textual version of the option.  For example,
 *    if "d" was the third option in OPT_INIT()'s options string, then
 *    OPT_GET() would return 3 if it encountered "-d" on the command
 *    line and OPT_NAME(3) would return "-d".
 *
 *    Invocation:
 *        name = opt_name (scan, index);
 *
 *    where:
 *        <scan>		- I
 *            is the command line scan context originally created by OPT_INIT().
 *            If this argument is NULL, the default scan context is used.
 *        <index>		- I
 *            is the index (1..N) of the option; i.e., is this option the 1st,
 *            2nd, or Nth option in the set of options specified in the call
 *            to OPT_INIT()?  NONOPT, OPTEND, and OPTERR can be specified.
 *        <name>		- O
 *            returns the name of the option corresponding to the specified
 *            index.  Word options do not have the enclosing curly braces.
 *            Options expecting an argument have a trailing ':'.  "NONOPT:",
 *            "OPTEND", and "OPTERR:" are returned for NONOPT, OPTEND, and
 *            OPTERR, respectively.  The text of the name is stored in memory
 *            belonging to the OPT utilities and it should not be modified by
 *            the calling routine.  Furthermore, the name should be used or
 *            duplicated before calling OPT_NAME() again.
 *
 */
char *opt_name(OptContext scan, int index)
{
    int  i, length;

    if (scan == NULL)
        scan = defaultContext;
    if (scan == NULL)
        return ("");

	/* Deallocate the previously-returned name string. */
    if (scan->nameString != NULL)
        free (scan->nameString);
    scan->nameString = NULL;

	/* Determine the name corresponding to the specified option index. */
    switch (index) {
    case NONOPT:
        return ("NONOPT:");

    case OPTEND:
        return ("OPTEND");

    case OPTERR:
        return ("OPTERR:");

    default:
					/* Scan the letter options. */
        for (i = 0;  scan->letterOptions[i] != '\0';  i++) {
            if (scan->letterOptions[i] == ':')  continue;
            if ((scan->letterIndex[i] < 0) &&
                (index == -scan->letterIndex[i])) {
                scan->nameString = malloc (1 + 3);
                sprintf (scan->nameString, "-%c:", scan->letterOptions[i]);
                return (scan->nameString);
            } 
            else if (index == scan->letterIndex[i]) {
                scan->nameString = malloc (1 + 2);
                sprintf (scan->nameString, "-%c", scan->letterOptions[i]);
                return (scan->nameString);
            }
        }
					/* Scan the word options. */
        for (i = 0;  i < scan->numNames;  i++) {
            if ((scan->nameIndex[i] < 0) && (index == -scan->nameIndex[i])) {
                length = strlen (scan->nameOptions[i]);
                scan->nameString = malloc (length + 3);
                sprintf (scan->nameString, "-%s:", scan->nameOptions[i]);
                return (scan->nameString);
            }
            else if (index == scan->nameIndex[i]) {
                length = strlen (scan->nameOptions[i]);
                scan->nameString = malloc (length + 2);
                sprintf (scan->nameString, "-%s", scan->nameOptions[i]);
                return (scan->nameString);
            }
        }
        /* If not found in either, return "". */
        return ("");
    }
}

/*
 *    opt_reset () - Reset Command Line Scan.
 *
 *    Function OPT_RESET resets a command line scan so that the next call to
 *    OPT_GET() begins at the beginning of the command line.  A new command
 *    line can be specified by passing OPT_RESET() a new ARGC/ARGV set of
 *    arguments.  This allows a program to process multiple command lines
 *    without having to create a new scan context for each command line.
 *
 *    Invocation:
 *        status = opt_reset (scan, argc, argv);
 *
 *    where:
 *        <scan>		- I
 *            is the command line scan context originally created by OPT_INIT().
 *            If this argument is NULL, the default scan context is used.
 *        <argc>		- I
 *            specifies the number of arguments in the new command line's ARGV
 *            array.
 *        <argv>		- I
 *            is an array of (CHAR *) pointers to the new command line's
 *            arguments.  If this argument is NULL, OPT_RESET() simply resets
 *            the scan context structure's internal fields so that the existing
 *            ARGC/ARGV is rescanned.
 *        <status>	- O
 *            returns the status of enabling or disabling the display of error
 *            messages, zero if no errors occurred and ERRNO otherwise.
 *
 */
int opt_reset(OptContext scan, int argc, char *argv[])
{
    int  i;

    if (scan == NULL)
        scan = defaultContext;
    if (scan == NULL) {
        errno = EINVAL;
        vperror (3,"(opt_reset) Null scan context.\n");
        return (errno);
    }

	/* If a new command line was specified, then replace the existing one in
   	the scan context structure.  If there are name options, then construct
   	a new argument length table for the ARGV array; the length table is used
   	by OPT_GET() when trying to match abbreviated name options. */
    if (argv != NULL) {
        if (scan->numNames > 0) {
            if ((argc > scan->argc) && (scan->arglen != NULL)) {
				/* Force a new table allocation if there
				   isn't enough room in the current table. */
                free (scan->arglen);  scan->arglen = NULL;
            }
            if (scan->arglen == NULL) {		/* Allocate a new table. */
                scan->arglen = (int *) malloc (argc * sizeof (int));
                if (scan->arglen == NULL) {
                    vperror (0, "(opt_reset) Error allocating array to hold %d argument lengths.\nmalloc: ",
                             argc);
                    return (errno);
                }
            }
            for (i = 0;  i < argc;  i++)	/* Tabulate argument lengths. */
                scan->arglen[i] = strlen (argv[i]);
        }

        scan->argc = argc;
        scan->argv = argv;
    }

	/* Reset the dynamic fields in the scan context structure so that the command
   	line will be rescanned from the beginning. */
    scan->endOfOptions = scan->argc + 1;
    scan->current = 1;
    scan->offset = 0;
    scan->optionType = none;

    if (opt_util_debug) {
        if (scan == defaultContext)
            printf ("(opt_reset) Resetting default scan.\n");
        else
            printf ("(opt_reset) Resetting scan %p.\n", scan);
    }
    return (0);
}

/*
 *    opt_set () - Set the Position in a Command Line Scan.
 *
 *    Function OPT_SET changes the current position in a command line scan.
 *    The next argument on the command line examined by OPT_GET() will be
 *    that specified in the call to OPT_SET().
 *
 *
 *    Invocation:
 *        status = opt_set (scan, newIndex);
 *
 *    where:
 *        <scan>		- I
 *            is the command line scan context originally created by OPT_INIT().
 *            If this argument is NULL, the default scan context is used.
 *        <newIndex>	- I
 *            specifies the index, 1..ARGC, in the ARGV array of the next
 *            argument to be examined by OPT_GET().  Note that an index of
 *            ARGC will cause OPT_GET() to return an end-of-scan indication.
 *        <status>	- O
 *            returns the status of changing the current scan position, zero
 *            if no errors occurred and ERRNO otherwise.
 *
 */
int opt_set(OptContext scan, int newIndex)
{

    if (scan == NULL)
        scan = defaultContext;
    if (scan == NULL) {
        errno = EINVAL;
        vperror (3,"(opt_set) Null scan context.\n");
        return (errno);
    }

    if ((newIndex < 1) || (newIndex > scan->argc)) {
        errno = EINVAL;
        vperror (3,"(opt_set) Invalid index: %d\n", newIndex);
        return (errno);
    }

	/* Set the current position of the command line scan so that the new position
   	will be examined on the next call to OPT_GET(). */
    if (newIndex < scan->endOfOptions)
        scan->endOfOptions = scan->argc + 1;
    scan->current = newIndex;
    scan->offset = 0;
    scan->optionType = none;

    if (opt_util_debug) {
        if (scan == defaultContext)
            printf ("(opt_reset) New position of default scan: %d\n",
                    newIndex);
        else
            printf ("(opt_reset) New position of scan %p: %d\n",
                    scan, newIndex);
    }
    return (0);
}

/*
 *    opt_term () - Terminate Command Line Scan.
 *
 *    Function OPT_TERM terminates a command line scan.  The scan context
 *    structure, created and initialized by OPT_INIT(), is deleted.
 *
 *
 *    Invocation:
 *        status = opt_term (scan);
 *
 *    where:
 *        <scan>		- I
 *            is the command line scan context originally created by OPT_INIT().
 *            If this argument is NULL, the default scan context is used.
 *        <status>	- O
 *            returns the status of terminating the command line scan, zero if
 *            no errors occurred and ERRNO otherwise.
 *
 */
int opt_term(OptContext scan)
{
    int  i;
    if (scan == NULL)  scan = defaultContext;
    if (scan == NULL)  return (EINVAL);

    if (opt_util_debug) {
        if (scan == defaultContext)
            printf ("(opt_term) Terminating default scan.\n");
        else
            printf ("(opt_term) Terminating scan %p.\n", scan);
    }

	/* Delete the scan context structure. */

    if (scan->letterOptions != NULL)  free (scan->letterOptions);
    if (scan->letterIndex != NULL)    free (scan->letterIndex);
    if (scan->nameOptions != NULL) {
        for (i = 0;  i < scan->numNames;  i++)
            if (scan->nameOptions[i] != NULL)  free (scan->nameOptions[i]);
        free (scan->nameOptions);
    }
    if (scan->nameIndex != NULL)    free (scan->nameIndex);
    if (scan->arglen != NULL)  free (scan->arglen);
    if (scan->nameString != NULL)  free (scan->nameString);
    free (scan);

    if (defaultContext == scan) {
        defaultContext = NULL;
    }
    return (0);
}

#ifdef  TEST
/*
 *
 *    Program to test the OPT_UTIL functions.
 *
 *    Under VMS,
 *        compile and link as follows:
 *            $ CC/DEFINE=TEST/INCLUDE=?? opt_util.c
 *            $ LINK opt_util, <libraries>
 *        invoke with one of the following command lines:
 *            $ opt_util <optionString>
 *
 */

main (int argc, char *argv[])
{ 
    char  *argument, *string, *t_argv;
    int  length, option, t_argc;
    OptContext  context;
    extern  int  vperror_print;

    vperror_print = 1;
    opt_util_debug = 1;

    opt_init (argc, argv, 0, argv[1], &context);
    while (option = opt_get (context, &argument)) {
        printf ("(Index %d)  Option %d, Argument = %s\n",
                opt_index (context), option,
                (argument == NULL) ? "<nil>" : argument);
    }
	printf("================\n");
    opt_reset (context, argc, argv);
    while (option = opt_get (context, &argument)) {
        printf ("(Index %d)  Option %d, Argument = %s\n",
                opt_index (context), option,
                (argument == NULL) ? "<nil>" : argument);
    }

    printf ("\nOption Indices:\n");
    for (option = -2; ;  option++) {
        string = opt_name (context, option);
        if (strcmp (string, "") == 0)  break;
        printf ("Option %d:\t\"%s\"\n", option, string);
    }

    printf ("\nInternal Command String:\n\n");

    string = "  program -ab xyz -name file -flag";
    printf ("Command String = %s\n", string);
    opt_create_argv (NULL, string, &t_argc, &t_argv);
    opt_delete_argv (t_argc, t_argv);

    string =
        "ab \"ab cd\" 'ab cd' ab\"cd\" \"ab\"'cd' \"ab'cd\" {ab\"Hello!\"cd}";
    printf ("\nCommand String = %s\n", string);
    opt_create_argv ("program", string, &t_argc, &t_argv);
    opt_delete_argv (t_argc, t_argv);

    exit (0);
}
#endif  /* TEST */
