/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */


#ifndef  FNM_UTIL_H		/* Has the file been INCLUDE'd already? */
#define  FNM_UTIL_H  yes

#ifdef __cplusplus
    extern  "C" {
#endif


/*
 *  Filename (Client View) and Definitions.
 */
typedef  struct  _FILENAME  *FILENAME ;	/* Filename handle. */

typedef  enum  FNMPART {
    FNMPATH = 0,			/* The whole pathname. */
    FNMNODE,				/* Node name, if specified. */
    FNMDIRECTORY,			/* Directory only. */
    FNMFILE,				/* Name, extension, and version. */
    FNMNAME,				/* File name. */
    FNMEXTENSION,			/* File extension. */
    FNMVERSION				/* Version number. */
}  FNMPART ;

/*
 *    Public functions.
 */
extern char *fnm_build(FNMPART part, const char *path, ...);
extern FILENAME fnm_create(const char *path, ...);
extern int fnm_destroy(FILENAME filename);
extern int fnm_exists(const FILENAME filename);
extern char *fnm_parse(const FILENAME FILENAME, FNMPART part);
#define fnm_path(FILENAME)  fnm_parse (FILENAME, FNMPATH)
#define fnm_node(FILENAME)  fnm_parse (FILENAME, FNMNODE)
#define fnm_directory(FILENAME)  fnm_parse (FILENAME, FNMDIRECTORY)
#define fnm_file(FILENAME)  fnm_parse (FILENAME, FNMFILE)
#define fnm_name(FILENAME)  fnm_parse (FILENAME, FNMNAME)
#define fnm_extension(FILENAME)  fnm_parse (FILENAME, FNMEXTENSION)
#define fnm_version(FILENAME)  fnm_parse (FILENAME, FNMVERSION)

#ifdef __cplusplus
    }
#endif

#endif				/* If this file was not INCLUDE'd previously. */
