/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef  MEO_UTIL_H		/* Has the file been INCLUDE'd already? */
#define  MEO_UTIL_H  yes

#ifdef __cplusplus
    extern  "C" {
#endif

#include  <stdio.h>
#include  <stdlib.h>


/*    Numerical bases for meo_dump(). */
typedef  enum  MeoBase {
    MeoNone = 0,
    MeoOctal = 8,
    MeoDecimal = 10,
    MeoHexadecimal = 16,
    MeoByte = 4,
    MeoText = 26
}  MeoBase ;

/*    Miscellaneous declarations.*/
extern  int  meo_util_debug ;		/* Global debug switch (1/0 = yes/no). */

/*    Public functions. */
extern int meo_dump (FILE *file, const char *indentation, MeoBase base,
		       int num_bytes_per_line, void *address, const char *buffer,
		       int num_bytes_to_dump);
#define MEO_DUMPD(file, indentation, address, buffer, num_bytes_to_dump) \
    meo_dump (file, indentation, 10, 8, address, buffer, num_bytes_to_dump)
#define MEO_DUMPO(file, indentation, address, buffer, num_bytes_to_dump) \
    meo_dump (file, indentation, 8, 8, address, buffer, num_bytes_to_dump)
#define MEO_DUMPT(file, indentation, address, buffer, num_bytes_to_dump) \
    meo_dump (file, indentation, 26, 40, address, buffer, num_bytes_to_dump)
#define MEO_DUMPB(file, indentation, address, buffer, num_bytes_to_dump) \
    meo_dump (file, indentation, 4, 16, address, buffer, num_bytes_to_dump)
#define MEO_DUMPX(file, indentation, address, buffer, num_bytes_to_dump) \
    meo_dump (file, indentation, 16, 16, address, buffer, num_bytes_to_dump)
extern int meoLoad (const char *filename, long offset, void **start_address,
		       long *numBytes);
extern int meoSave (void *start_address, long numBytes, const char *filename,
                       long offset);

#ifdef __cplusplus
    }
#endif

#endif	/* If this file was not INCLUDE'd previously. */
