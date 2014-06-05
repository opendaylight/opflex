/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

/*
*    This package provides a filename parsing capability inspired
*    by the VAX/VMS lexical function, F$PARSE().  File specifications have
*    the following structure:
*
*                 node:/directory(s)/name.extension.version
*
*    Any field is optional.  NODE is a host name; DIRECTORY is one or more
*    names separated by "/"s; NAME follows the last "/" in the pathname.
*    VERSION is a 3-digit number (e.g., "002") and EXTENSION follows the
*    last dot before the VERSION dot.
*
*    A filename is created as follows:
*
*        #include  "fnm_util.h"			-- Filename utilities.
*        FileName  FILENAME;
*        ...
*        FILENAME = fnm_create ("<file_spec>", NULL);
*
*    fnm_create() expands the file specification, translating environment
*    variable references and filling in defaults for missing fields.
*
*    fnm_create() can be passed multiple file specifications, which are
*    then processed from left to right in the calling sequence:
*
*        FILENAME = fnm_create ("<spec1>", ..., "<specN>", NULL);
*
*    First, the leftmost file specification is examined and any references
*    to environment variables are translated.  The next file specification
*    is then examined.  Environment variables are translated and fields
*    missing in the first file specification are supplied from the new
*    file specification.  Subsequent file specifications are examined,
*    in turn, and "applied" to the results of the processing of the
*    previous file specifications.  Finally, system defaults (e.g., the
*    user's home directory) are supplied for missing fields that remain.
*
*    Is that clear?  I used to have a diagram that showed the file names
*    stacked up, one on top of another:
*
*                          ... System Defaults ...
*                               File_Spec_#N    |
*                                    ...        |
*                               File_Spec_#2    |
*                               File_Spec_#1    V
*                              --------------
*                                  Result
*
*    File name components would drop down through holes in lower-level
*    specifications to fill in missing fields in the result.
*
*    Specifying multiple file specifications is useful for replacing
*    extensions, concatenating directories, etc.:
*
*        #include  "fnm_util.h"			-- Filename utilities.
*        FileName  FILENAME;
*        ...					-- "/usr/me" (current directory)
*        FILENAME = fnm_create (NULL);
*						-- "/usr/me/prog.lis"
*        FILENAME = fnm_create (".lis", "prog.c", NULL);
*						-- "/usr/you/tools/dump.o"
*        FILENAME = fnm_create (".o", "tools/dump.c", "/usr/you/", NULL);
*
*    What can you do with a file name once it is created?  You call
*    fnm_parse() to get the whole file name or parts of the file name
*    as a string:
*
*        #include  "fnm_util.h"			-- Filename utilities.
*        char  *s;
*        FileName  FILENAME;
*        ...
*        FILENAME = fnm_create ("host:/usr/who/myprog.c.001", NULL);
*        s = fnm_parse (FILENAME, FNMPATH);		-- "host:/usr/who/myprog.c.001"
*        s = fnm_parse (FILENAME, FNMNODE);		-- "host:"
*        s = fnm_parse (FILENAME, FNMDIRECTORY);	-- "/usr/who"
*        s = fnm_parse (FILENAME, FNMFILE);		-- "myprog.c.001"
*        s = fnm_parse (FILENAME, FNMNAME);		-- "myprog"
*        s = fnm_parse (FILENAME, FNMEXTENSION);	-- ".c"
*        s = fnm_parse (FILENAME, FNMVERSION);	-- ".001"
*        fnm_destroy (FILENAME);
*
*    Shorthand macros - fnm_path(), fnmNode(), etc. - are defined for each
*    of the fnm_parse() calls above.
*
*
* Public Procedures:
*
*    fnm_build() - builds a pathname.
*    fnm_create() - creates a filename.
*    fnm_destroy() - destroys a filename.
*    fnm_exists() - checks if a file exists.
*    fnm_parse() - parses a filename.
*
* Macro Definitions:
*
*    fnm_path() - returns a filename's full pathname.
*    fnm_node() - returns the node from a filename.
*    fnm_directory() - returns the directory from a filename.
*    fnm_file() - returns the file, extension, and version from a filename.
*    fnm_name() - returns the file from a filename.
*    fnm_extension() - returns the extension from a filename.
*    fnm_version() - returns the version number from a filename.
*
* Private Procedures:
*
*    fnm_fill_parts() - fill in missing parts of a filename with defaults.
*    fnm_locate_parts() - locate the parts of a filename.
*    fnm_new() - allocates and initializes a filename structure.
*
*/

#include <errno.h>
#include <limits.h>
#if __STDC__
#    include  <stdarg.h>
#else
#    include  <varargs.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* #include <direct.h> */
#include <sys/stat.h>

#include "str1-util.h"
#include "debug.h"
#include "fnm-util.h"


/*
 *    File Name - contains the fully-expanded file specification as well as
 *       the individual components of the file specification.
 */
typedef  struct  _FILENAME {
    char  *path;			/* Fully-expanded file specification. */
    char  *node;			/* "node:" */
    char  *directory;			/* "/directory(ies)/" */
    char  *file;			/* "name.extension.version" */
    char  *name;			/* "name" */
    char  *extension;			/* ".extension" */
    char  *version;			/* ".version" */
}  _FILENAME;


/*
 *    Private Functions.
 */
static FILENAME fnm_fill_parts (const FILENAME name1, const FILENAME name2);
static int fnm_locate_parts (FILENAME file_spec);
static FILENAME fnm_new (const char *pathname);

/*
 *
 *    fnm_build - Build a Pathname. Builds a pathname from one or more file
 *       specifications.  fnm_build() is essentially an encapsulation of
 *       the following code fragment:
 *
 *       char  pathname[PATH_MAX];
 *       FILENAME  FILENAME = fnm_create (file_spec1, ..., file_specN, NULL);
 *       ...
 *       strcpy (pathname, fnm_parse (FILENAME, FNMPATH));
 *       fnm_destroy (FILENAME);
 *
 *   I got tired of coding up variations of this every time I needed to
 *   build a full pathname, so I made a copy of fnm_create() and modified
 *   it to return a character-string pathname instead of a FILENAME handle.
 *
 *    Invocation:
 *        pathname = fnm_build (part, [file_spec1, ..., file_specN,] NULL);
 *
 *   where
 *        <part>		- I
 *            specifies which part of the file name you want returned:
 *               FNMPATH - "node:/directory(ies)/name.extension.version"
 *               FNMNODE - "node:"
 *               FNMDIRECTORY - "/directory(ies)/"
 *               FNMFILE - "name[.extension[.version]]"
 *               FNMNAME - "name"
 *               FNMEXTENSION - ".extension"
 *               FNMVERSION - ".version"
 *           (These enumerated values are defined in "fnm_util.h".)
 *       <file_spec1>	- I
 *       <file_specN>	- I
 *           are the file specfications used to construct the resulting file
 *           name.  Each file specification is a UNIX pathname containing
 *           one or more of the components of a pathname (e.g., the directory,
 *           the extension, the version number, etc.).  Missing components in
 *           the result are filled in from the file specifications as they are
 *           examined in left-to-right order.  The NULL argument marks the end
 *           of the file specification list.
 *       <pathname>	- O
 *           returns the pathname constructed from the file specifications;
 *           "" is returned in the event of an error.  The returned string
 *           is private to this routine and it should be used or duplicated
 *           before calling fnm_build() again.
*/
char *fnm_build ( FNMPART part, const char *file_spec, ... )
{
    va_list  ap;
    FILENAME  defaults, new_result, result;
    int  status;
    static  char  pathname[PATH_MAX];

    va_start (ap, file_spec);

    result = NULL;

    while (file_spec != NULL) {

        defaults = fnm_new (file_spec);
        if (defaults == NULL) {
            vperror (0, "(fnm_build) Error creating defaults: %s\nfnm_new: ", 
                     file_spec);
            return ("");
        }
        fnm_locate_parts (defaults);

        new_result = fnm_fill_parts (result, defaults);
        status = errno;
        if (result != NULL)  fnm_destroy (result);
        fnm_destroy (defaults);
        if (new_result == NULL) {
            errno = status;
            vperror(0, 
                    "(fnm_build) Error creating intermediate result.\n"
                    "fnm_new: ");
            return ("");
        }
        result = new_result;

        file_spec = va_arg (ap, const char *);
    }
    va_end (ap);


    /* Fill in missing fields with the system defaults. */

    getcwd (pathname, sizeof pathname);
    strcat (pathname, "/");
    defaults = fnm_new (pathname);
    if (defaults == NULL) {
        vperror (0, "(fnm_build) Error creating system defaults: %s\nfnm_new: ",
                 pathname);
        return ("");
    }
    fnm_locate_parts (defaults);

    new_result = fnm_fill_parts (result, defaults);
    status = errno;
    if (result != NULL)  fnm_destroy (result);
    fnm_destroy (defaults);
    if (new_result == NULL) {
        errno = status;
        vperror (0, "(fnm_build) Error creating final result.\nfnm_new: ");
        return ("");
    }
    result = new_result;

    /* Return the full pathname to the caller. */
    strcpy (pathname, fnm_parse (result, part));
    fnm_destroy (result);
    return (pathname);
}

/*
 *    fnm_create () - Create a File Name.
 *    The fnm_create() function creates a file name.
 *
 *    Invocation:
 *        fname = fnm_create ([file_spec1, ..., file_specN,] NULL);
 *
 *  where
 *        <file_spec1>	- I
 *       <file_specN>	- I
 *           are the file specfications used to construct the resulting file
 *           name.  Each file specification is a UNIX pathname containing
 *           one or more of the components of a pathname (e.g., the directory,
 *           the extension, the version number, etc.).  Missing components in
 *           the result are filled in from the file specifications as they are
 *           examined in left-to-right order.  The NULL argument marks the end
 *           of the file specification list.
 *       <fname>	- O
 *           returns a handle that can be used in other FNM_UTIL calls.  NULL
 *           is returned in the event of an error.
 */

FILENAME fnm_create ( const char *file_spec, ... )
{
    va_list  ap;
    char  pathname[PATH_MAX];
    FILENAME  defaults, new_result, result;
    int  status;
    static char mod[] = "fnm_create";

    /* Process each file specification in the argument list. */
    va_start (ap, file_spec);

    result = NULL;

    while (file_spec != NULL) {
        defaults = fnm_new (file_spec);
        if (defaults == NULL) {
            vperror (0, "(%s) Error creating defaults: %s\nfnm_new: ",
                     mod, file_spec);
            return (NULL);
        }
        fnm_locate_parts (defaults);
        new_result = fnm_fill_parts (result, defaults);
        status = errno;
        if (result != NULL)  fnm_destroy (result);
        fnm_destroy (defaults);
        if (new_result == NULL) {
            errno = status;
            vperror (0, 
                     "(%s) Error creating intermediate result.\nfnm_new: ",
                     mod);
            return (NULL);
        }
        result = new_result;
        file_spec = va_arg (ap, const char *);
    }
    va_end (ap);
    
    /* Fill in missing fields with the system defaults. */
    getcwd (pathname, sizeof pathname);
    strcat (pathname, "/");
    defaults = fnm_new (pathname);
    if (defaults == NULL) {
        vperror (0, "(%s) Error creating system defaults: %s\nfnm_new: ",
                 mod, pathname);
        return (NULL);
    }
    fnm_locate_parts (defaults);

    new_result = fnm_fill_parts (result, defaults);
    status = errno;
    if (result != NULL)  fnm_destroy (result);
    fnm_destroy (defaults);
    if (new_result == NULL) {
        errno = status;
        vperror (0, "(%s) Error creating final result.\nfnm_new: ", mod);
        return (NULL);
    }
    result = new_result;

    return (result);
}

/*
 *    fnm_destroy ()- Destroy a File Name.
 *    The fnm_destroy() function destroys a file name.
 *
 *    Invocation:
 *        status = fnm_destroy (fname);
 *
 *  where
 *        <fname>	- I
 *           is the file name handle returned by fnm_create().
 *       <status>	- O
 *           returns the status of destroying the file name, zero if
 *           there were no errors and ERRNO otherwise.
 */
int fnm_destroy(FILENAME fname)
{
    if (fname == NULL) {
        errno = EINVAL;
        vperror (1, "(fnm_destroy) NULL file handle: ");
        return (errno);
    }

    if (fname->path != NULL)  free (fname->path);
    if (fname->node != NULL)  free (fname->node);
    if (fname->directory != NULL)  free (fname->directory);
    if (fname->file != NULL)  free (fname->file);
    if (fname->name != NULL)  free (fname->name);
    if (fname->extension != NULL)  free (fname->extension);
    if (fname->version != NULL)  free (fname->version);

    free (fname);
    return (0);
}

/*
 *    fnm_exists ()
 *    Check If a File Exists. The fnm_exists() function checks to see 
 *    if the file referenced by a file name actually exists.
 *
 *   Invocation:
 *        exists = fnm_exists (fname);
 *
 *   where
 *        <fname>	- I
 *            is the file name handle returned by fnm_create().
 *       <exists>	- O
 *           returns true (1) if the referenced file exists and false (0)
 *           if it doesn't exist.
 */
int fnm_exists (const FILENAME fname)
{
    struct stat file_info;

    if (fname == NULL) {
        errno = EINVAL;
        vperror (1,"(fnm_exists) NULL file handle: ");
        return (0);
    }

    if (stat (fname->path, &file_info)) {
        switch (errno) {
        case EACCES:				/* Expected errors. */
        case ENOENT:
        case ENOTDIR:
            break;
        default:				/* Unexpected errors. */
            vperror (1, 
                     "(fnm_exists) Error getting information for %s.\nstat: ",
                     fname->path);
        }
        return (0);				/* Not found. */
    }
    return (1);				/* Found. */
}

/*
 *    fnm_parse () - Parse a File Name.
 *    The fnm_parse() function returns the requested part of a file name, e.g.,
 *    the directory, the name, the extension, etc.
 *
 *   Invocation:
 *       value = fnm_parse (fname, part);
 *
 *  where
 *        <fname>	- I
 *           is the file name handle returned by fnm_create().
 *       <part>		- I
 *           specifies which part of the file name you want returned:
 *               FNMPATH - "node:/directory(ies)/name.extension.version"
 *               FNMNODE - "node:"
 *               FNMDIRECTORY - "/directory(ies)/"
 *               FNMFILE - "name[.extension[.version]]"
 *               FNMNAME - "name"
 *               FNMEXTENSION - ".extension"
 *               FNMVERSION - ".version"
 *           (These enumerated values are defined in "fnm_util.h".)
 *       <value>		- O
 *           returns the requested part of the file name; "" is returned
 *          in the event of an error or if the requested part is missing.
 *           The returned string is private to the file name and it should
 *           not be modified or deleted; it should not be used after the
 *           file name is deleted.
 */
char *fnm_parse (const FILENAME fname, FNMPART part)
{
    if (fname == NULL) {
        errno = EINVAL;
        vperror (1, "(fnm_parse) NULL file handle: ");
        return ("");
    }

    switch (part) {
    case FNMPATH:
        return ((fname->path == NULL) ? "" : fname->path);
    case FNMNODE:
        return ((fname->node == NULL) ? "" : fname->node);
    case FNMDIRECTORY:
        return ((fname->directory == NULL) ? "" : fname->directory);
    case FNMFILE:
        return ((fname->file == NULL) ? "" : fname->file);
    case FNMNAME:
        return ((fname->name == NULL) ? "" : fname->name);
    case FNMEXTENSION:
        return ((fname->extension == NULL) ? "" : fname->extension);
    case FNMVERSION:
        return ((fname->version == NULL) ? "" : fname->version);
    default:
        return ("");
    }
}

/*
 *    fnm_fill_parts () Fill the Missing Parts of a File Name with Defaults.
 *    Function fnm_fill_parts() fills the missing parts of a file name with the
 *   corresponding parts from a defaults file name.
 *
 *   Invocation:
 *        result = fnm_fill_parts (fname, defaults);
 *
 *   where
 *        <fname>	- I
 *           is the handle returned by fnm_create() for the file name in question.
 *       <defaults>	- I
 *           is the handle returned by fnm_create() for the file name containing
 *           the defaults.
 *       <result>	- O
 *           returns a handle for a new file name consisting of the old file
 *           name with missing parts supplied by the defaults file name.  NULL
 *           is returned in the event of an error.
 */
static FILENAME fnm_fill_parts(const FILENAME fname, const FILENAME defaults)
{
    char  *ddef, *dnew, pathname[PATH_MAX];
    FILENAME  result;
    int  ldef, lnew;

    strcpy (pathname, "");

    /* Substitute the node name. */
    if ((fname == NULL) || (fname->node == NULL)) {
        if (defaults->node != NULL)  strcat (pathname, defaults->node);
    } else {
        strcat (pathname, fname->node);
    }

    /* Substitute the directory.  First, process dot directories in the
       new file specification ("fname").  Single dots (current directory)
       are replaced by the current working directory; double dots (parent
       directory) remove successive child directories from the default
       file specfication ("defaults").  Dot directories in the default
       FS have no effect, unless the new FS has no directory yet. */
    
    dnew = ((fname == NULL) || (fname->directory == NULL))
           ? "" : fname->directory;
    lnew = strlen (dnew);
    ddef = (defaults->directory == NULL) ? "" : defaults->directory;
    ldef = strlen (ddef);
    ddef = defaults->directory + ldef;		/* DDEF points at '\0'. */

    /* Prior to loop:  DDEF points to end of (N+1)-th component of directory.
       Be careful making changes to this code - it's not very straightforward.
       It should handle cases like the "/" directory, no dot directories, and
       so on. */

    while ((lnew > 0) && (ldef > 0)) {
        if (strcmp (dnew, ".") == 0) {			/* Current directory. */
            ldef = 0;
        } else if (strncmp (dnew, "./", 2) == 0) {	/* Current directory. */
            ldef = 0;
        } else if (strcmp (dnew, "..") == 0) {		/* Up one directory. */
            dnew = dnew + 2;  lnew = lnew - 2;
            do { ldef--; } while ((ldef > 0) && (*--ddef != '/'));
        } else if (strncmp (dnew, "../", 3) == 0) {	/* Up one directory. */
            dnew = dnew + 3;  lnew = lnew - 3;
            do { ldef--; } while ((ldef > 0) && (*--ddef != '/'));
        } else {					/* No dot directory. */
            break;
        }
    }

    /* After loop:  DDEF points to end of (N+1-M)-th component of directory,
       where M is the number of ".." (parent) directories processed.  Get rid
       of the "+1"-th component. */
    while ((ldef > 0) && (*--ddef != '/'))
        ldef--;

    ddef = (defaults->directory == NULL) ? "" : defaults->directory;

    /* After processing the dot directories, perform the actual directory
       substitutions.  This procedure is complicated by the two types of
       directories, absolute and relative.  If the new directory and the default
       directory are both absolute or both relative, use the new directory.
       If one directory is relative and the other absolute, append the relative
       directory to the absolute directory. */    
    if (lnew == 0) {			/* No previous directory spec. */
        strncat (pathname, ddef, ldef);
    }
    else if ((strcmp  (dnew, ".") == 0) ||
             (strncmp (dnew, "./", 2) == 0)) {
	/* Dot directories in default FS won't have any effect; use new FS. */
        getcwd (&pathname[strlen (pathname)],
                sizeof pathname - strlen (pathname));
        strcat (pathname, "/");
    }
    else if ((strcmp  (dnew, "..") == 0) ||
             (strncmp (dnew, "../", 3) == 0)) {
	/* Dot directories in default FS won't have any effect; use new FS. */
        strncat (pathname, dnew, lnew);
    }
    else if (*ddef == '/') {
        if (*dnew == '/')		/* Two absolute directory specs. */
            strncat (pathname, dnew, lnew);
        else {				/* Append relative to absolute. */
            strncat (pathname, ddef, ldef);
            strncat (pathname, dnew, lnew);
        }
    }
    else {
        if (*dnew == '/') {		/* Append relative to absolute. */
            strncat (pathname, dnew, lnew);
            strncat (pathname, ddef, ldef);
        }
        else				/* Two relative directory specs. */
            strncat (pathname, dnew, lnew);
    }

    /* Substitute the file name. */
    if ((fname == NULL) || (fname->name == NULL)) {
        if (defaults->name != NULL)  strcat (pathname, defaults->name);
    }
    else {
        strcat (pathname, fname->name);
    }

    /* Substitute the extension. */
    if ((fname == NULL) || (fname->extension == NULL)) {
        if (defaults->extension != NULL)
            strcat (pathname, defaults->extension);
    }
    else {
        strcat (pathname, fname->extension);
    }

    /* Substitute the version number. */
    if ((fname == NULL) || (fname->version == NULL)) {
        if (defaults->version != NULL)  strcat (pathname, defaults->version);
    }
    else {
        strcat (pathname, fname->version);
    }

    /* Construct a file name structure for the resulting file name. */
    result = fnm_new (pathname);
    if (result == NULL) {
        vperror (0, "(fnm_fill_parts) Error creating result: %s\nfnm_new: ", pathname);
        return (NULL);
    }
    fnm_locate_parts (result);
    return (result);
}

/*
 *    fnm_locate_parts () - Locate the Parts of a File Name.
 *    The fnm_locate_parts() function determines the locations of the different
 *    parts of a file name, e.g., the directory, the name, the extension, etc.
 *
 *   Invocation:
 *        status = fnm_locate_parts (fname);
 *
 *   where
 *        <fname>	- I
 *            is the file name handle returned by fnm_create().
 *       <status>	- O
 *           returns the status of dissecting the file name, zero if there
 *           were no errors and ERRNO otherwise.
 */
static int fnm_locate_parts (FILENAME fname)
{
    char *fs, pathname[PATH_MAX], *s;

    /* Advance the "fs" pointer as you scan the file specification. */
    strcpy (pathname, fname->path);
    fs = pathname;

    /* First, if the file specification contains multiple pathnames, separated
       by commas or spaces, discard the trailing pathnames. */
    if ((s = strchr (fs, ' ')) != NULL)  *s = '\0';
    if ((s = strchr (fs, ',')) != NULL)  *s = '\0';

    /* Locate the node.  The node name is separated from the rest of the file
       name by a colon (":"). */
    if ((s = strchr (fs, ':')) != NULL) {
        fname->node = strndup (fs, (int) (s - fs + 1));
        if (fname->node == NULL) {
            vperror (0, "(fnm_locate_parts) Error duplicating node of %s.\nstrndup: ",
                     fname->path);
            return (errno);
        }
        fs = ++s;
    }

    /* Locate the directory.  The directory extends through the last "/" in the
       file name. */
    if ((s = strrchr (fs, '/')) != NULL) {
        fname->directory = strndup (fs, (int) (s - fs + 1));
        if (fname->directory == NULL) {
            vperror (0, "(fnm_locate_parts) Error duplicating directory of %s.\nstrndup: ",
                     fname->path);
            return (errno);
        }
        fs = ++s;
    }

    /* The remainder of the pathname is the combined name, extension, and
       version number. */
    if (*fs != '\0') {
        fname->file = strdup (fs);
        if (fname->file == NULL) {
            vperror (0, "(fnm_locate_parts) Error duplicating file of %s.\nstrdup: ",
                     fname->path);
            return (errno);
        }
    }

    /* Locate the version number.  Since version numbers are not part of
       UNIX, these version numbers are a user convention.  Any file extension
       that can be converted to an integer is considered a version number;
       e.g., ".007", etc.  (So we can make this test, a version number of zero
       is not allowed. */
    if (((s = strrchr (fs, '.')) != NULL) && (atoi (++s) != 0)) {
        fname->version = strdup (--s);
        if (fname->version == NULL) {
            vperror (0, "(fnm_locate_parts) Error duplicating version of %s.\nstrdup: ",
                     fname->path);
            return (errno);
        }
        *s = '\0';			/* Exclude version temporarily. */
    }

    /* Locate the extension.  The extension is the last part of the file name
       preceded by a "." (not including the version number, though). */
    if ((s = strrchr (fs, '.')) != NULL) {
        fname->extension = strdup (s);
        if (fname->extension == NULL) {
            vperror (0, "(fnm_locate_parts) Error duplicating extension of %s.\nstrdup: ",
                     fname->path);
            return (errno);
        }
        *s = '\0';			/* Exclude extension temporarily. */
    }

    /* Locate the name.  The name is the rest of the file name, excluding the
       last extension and the version number, if any. */
    if (*fs != '\0') {
        fname->name = strdup (fs);
        if (fname->name == NULL) {
            vperror (0, "(fnm_locate_parts) Error duplicating name of %s.\nstrdup: ",
                     fname->path);
            return (errno);
        }
    }

    /* Restore extension and version. */
    if (fname->extension != NULL)  fs[strlen (fs)] = '.';
    if (fname->version != NULL)  fs[strlen (fs)] = '.';

    return (0);
}

/*
 *    fnm_new () - Allocates a File Name Structure.
 *    The fnm_new() function allocates and initializes a file name structure.
 *
 *   Invocation:
 *        fname = fnm_new (pathname);
 *
 *   where
 *        <pathname>	- I
 *           is the initial pathname.
 *       <fname>	- O
 *           returns a pointer to the allocated file name structure.  The
 *           caller is responible for FREE(3)ing the structure when it is
 *           no longer needed.  NULL is returned in the event of an error.
 */
static FILENAME fnm_new (const char *pathname)
{
    char exp_pathname[PATH_MAX];
    FILENAME fname;
    int status;

    /* Allocate the file name structure. */
    fname = (FILENAME) malloc (sizeof (_FILENAME));
    if (fname == NULL) {
        vperror (0, "(fnm_new) Error allocating structure for %s.\n", pathname);
        return (NULL);
    }

    /* Initialize the structure. */
    if (pathname == NULL) {
        fname->path = NULL;
    }
    else {
        strEnv (pathname, -1, exp_pathname, sizeof exp_pathname);
        fname->path = strdup (exp_pathname);
        if (fname->path == NULL) {
            vperror (0,"(fnm_new) Error duplicating pathname: %s\n",
                     exp_pathname);
            status = errno;  free (fname);  errno = status;
            return (NULL);
        }
    }

    fname->node = NULL;
    fname->directory = NULL;
    fname->file = NULL;
    fname->name = NULL;
    fname->extension = NULL;
    fname->version = NULL;

    return (fname);

}
