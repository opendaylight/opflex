/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

/*
 *    These functions perform various operations on memory regions.
 *
 *    The meo_dump() functions generate VMS-style dumps of arbitrary regions
 *    of memory.  Each line of output includes the address of the memory being
 *    dumped, the data (formatted in octal, decimal, or hexadecimal), and the
 *    ASCII equivalent of the data:
 *
 *        ...
 *        00000060:  60616263 64656667 68696A6B 6C6D6E6F  "`abcdefghijklmno"
 *        00000070:  70717273 74757677 78797A7B 7C7D7E7F  "pqrstuvwxyz{|}~."
 *        00000080:  80818283 84858687 88898A8B 8C8D8E8F  "................"
 *        ...
 *
 *    The MEO_UTIL package also provides a simple means of saving the contents
 *    of an arbitrary memory region to a file:
 *
 *        #include  <stdio.h>		-- Standard I/O definitions.
 *        #include  "meo_util.h"		-- Memory operations.
 *        char  oldBuffer[1234];
 *        ...
 *        meoSave (oldBuffer, sizeof oldBuffer, "buffer.dat", 0);
 *
 *    The contents of a file can be loaded into an existing region of memory:
 *
 *        char  *new_buffer = oldBuffer;
 *        int  num_bytes = sizeof oldBuffer;
 *        ...
 *        meo_load ("buffer.dat", 0, &new_buffer, &num_bytes);
 *
 *    or into memory dynamically allocated by meo_load():
 *
 *        char  *new_buffer = NULL;
 *        int  num_bytes;
 *        ...
 *        meo_load ("buffer.dat", 0, &new_buffer, &num_bytes);
 *        ... use the new buffer ...
 *        free (new_buffer);
 *
 *Public Procedures (* defined as macros):
 *    meo_dump() - outputs an ASCII dump of a memory region to a file.
 *  * MEO_DUMPD() - outputs a decimal ASCII dump to a file.
 *  * MEO_DUMPO() - outputs an octal ASCII dump to a file.
 *  * MEO_DUMPT() - outputs a text ASCII dump to a file.
 *  * MEO_DUMPX() - outputs a hexadecimal ASCII dump to a file.
 *    meo_load() - loads the contents of a file into memory.
 *    meoSave() - saves the contents of memory to a file.
 *
 */

#include  <ctype.h>
#include  <errno.h>
#include  <limits.h>
#ifndef PATH_MAX
#    include  <sys/param.h>
/*
  #    define  PATH_MAX  MAXPATHLEN
*/
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#ifndef isascii
#    define  isascii(c)  ((unsigned char) (c) <= 0177)
#endif
#include <gendcls.h>
#include "meo-util.h"
#include "str-util.h"
#include "debug.h"
#include "util.h"
#include "opt-util.h"
#include "get-util.h"
#include "dae-util.h"
#include "fnm-util.h"

int  meo_util_debug = 0;		/* Global debug switch (1/0 = yes/no). */

/*
 *    meo_dump () -  Generate an ASCII Dump of a Memory Region.
 *
 *    Function meo_dump() formats the binary contents of a memory region in ASCII
 *    and writes the ASCII dump to a file.  Each output line looks as follows:
 *
 *        <address>:  <data1> <data2> ... <dataN>  "data/ASCII"
 *
 *    The data items, DATA1, DATA2, etc., are formatted in decimal, hexadecimal,
 *    or octal notation.
 *
 *    Invocation:
 *        status = meo_dump (file, indentation, base, num_bytes_per_line,
 *                          address, buffer, num_bytes_to_dump);
 *
 *    where
 *        <file>			- I
 *            is the Unix FILE* handle for the output file.  If FILE is NULL,
 *            the dump is written to standard output.
 *        <indentation>		- I
 *            is a text string used to indent each line of the dump.  The
 *            string is used as the format string in an FPRINTF(3) statement,
 *            so you can embed anything you want.  This argument can be NULL.
 *        <base>			- I
 *            specifies the output base for the dump: "MeoOctal" for octal,
 *            "MeoDecimal" for decimal, "MeoHexadecimal" for hexadecimal,
 *            and "MeoText" for text.  (These are enumerated values defined
 *            in the "meo_util.h" header file.)
 *        <num_bytes_per_line>	- I
 *            specifies the number of bytes in the buffer to be formatted and
 *            dumped on a single line of output.  Good values are 8 values per
 *            line for octal, 8 for decimal, 16 for hexadecimal, and 40 for text.
 *        <address>		- I
 *            is the value to be displayed in the address field.  It can be
 *            different than the actual buffer address.
 *        <buffer>		- I
 *            is the buffer of data to be dumped.
 *        <num_bytes_to_dump>	- I
 *            is exactly what it says it is!
 *        <status>		- O
 *            returns the status of generating the dump, zero if there were
 *            no errors and ERRNO otherwise.
 *
 */
int  meo_dump (FILE *file, const char *indentation, MeoBase base, 
               int num_bytes_per_line, void *address, const char *buffer,
               int num_bytes_to_dump)
{
    static char mod[] = "meo_dump";
    char c, *out, *outbuf;
    int alignment, integer, num_bytes_dumped, num_bytes_this_line;
    unsigned char *buf, *inbuf, chardata;

    if (file == NULL)
        file = stdout;

    /* Allocate an input buffer to hold the bytes to be dumped.  Doing this
    **	prevents bus errors should the input buffer be misaligned (i.e., on
    **	an odd-byte boundary). 
    */
    inbuf = (unsigned char *) malloc(num_bytes_per_line);
    if (inbuf == NULL) {
        DEBUG_OUT(0, ("(%s) Error allocating temporary, %d-byte input buffer.\nmalloc: ",
                      mod, num_bytes_per_line));
        return (errno);
    }

    /* Allocate a string in which to build each output line. */
    outbuf = (char *) malloc (num_bytes_per_line * 6);
    if (outbuf == NULL) {
        DEBUG_OUT(0, ("(%s Error allocating temporary, %d-byte output buffer.\nmalloc: ",
                      mod, num_bytes_per_line * 6));
        return (errno);
    }

    /*
     *    Generate each line of the dump.
     */
    while (num_bytes_to_dump > 0) {
        num_bytes_this_line = (num_bytes_to_dump > num_bytes_per_line)
            ? num_bytes_per_line : num_bytes_to_dump;
        memset (inbuf, '\0', num_bytes_per_line);		/* Zero trailing bytes. */
        memcpy (inbuf, buffer, num_bytes_this_line);

        /* Output the line indentation and the memory address. */
        if (indentation != NULL)  fprintf (file, indentation);
#ifdef YOU_WANT_VARIABLE_LENGTH_ADDRESS
        fprintf (file, "%p:\t", address);
#else
        fprintf (file, "%08lX: ", (long) address); /* Assumes 32-bit address.*/
#endif

        /* Format the data in the requested base. */
        buf = inbuf;  num_bytes_dumped = 0;
        out = outbuf;

        switch (base) {
        /* Base 8 - display the contents of each byte as an octal number. */
        case MeoOctal:
            while (num_bytes_dumped < num_bytes_per_line) {
                if (num_bytes_dumped++ < num_bytes_this_line)
                    sprintf (out, " %3.3o", *buf++);
                else
                    sprintf (out, " %3s", " ");
                out = out + strlen (out);
            }
            break;

        /* Base 10 - display the contents of each 
         * byte as a decimal number. */
        case MeoDecimal:
            while (num_bytes_dumped < num_bytes_per_line) {
                if (num_bytes_dumped++ < num_bytes_this_line)
                    sprintf (out, " %3u", *buf++);
                else
                    sprintf (out, " %3s", " ");
                out = out + strlen (out);
            }
            break;
            
        /* Base 16 - display the contents of each integer as a 
           hexadecimal number. */
        case MeoHexadecimal:
            while (num_bytes_dumped < num_bytes_per_line) {
                if (num_bytes_dumped < num_bytes_this_line) {
                    alignment = (long) buf % sizeof (int);
                    if (alignment > 0) {
                        for (integer = 0;  alignment > 0;  alignment--)
                            integer = (integer << 8) | (*buf++ & 0x0FF);
                    }
                    else {
                        integer = *((int *) buf);
                        buf = buf + sizeof (int);
                    }
                    sprintf (out, " %08X", integer);
                }
                else {
                    sprintf (out, " %8s", " ");
                }
                out = out + strlen (out);
                num_bytes_dumped = num_bytes_dumped + sizeof (int);
            }
            break;

        case MeoByte:
            while (num_bytes_dumped < num_bytes_per_line) {
                if (num_bytes_dumped < num_bytes_this_line) {
                    alignment = (long) buf % sizeof (char);
                    if (alignment > 0) {
                        for (integer = 0;  alignment > 0;  alignment--)
                            integer = (*buf++ & 0x0FF);
                    } else {
                        chardata = *((unsigned char *) buf);
                        buf = buf + sizeof (char);
                    }
                    sprintf (out, " %02X", chardata);
                }
                else {
                    sprintf (out, " %2s", " ");
                }
                out = out + strlen (out);
                num_bytes_dumped = num_bytes_dumped + sizeof (char);
            }
            break;

        /* "Base 26" - treat the data as ASCII text. */
        case MeoText:
        default:
            break;

        }

        /* Append the ASCII version of the buffer. */
        if (base != MeoText) {
            *out++ = ' ';  *out++ = ' ';
        }
        *out++ = '"';
        memcpy (out, buffer, num_bytes_this_line);
        num_bytes_dumped = 0;
        while (num_bytes_dumped++ < num_bytes_this_line) {
            if (isascii (*out) && isprint (*out)) {
                c = *out;  *out++ = c;
            } 
            else {
                *out++ = '.';
            }
        }
        *out++ = '"';  *out++ = '\0';

        /* Output the dump line to the file. */
        fprintf (file, "%s\n", outbuf);

        /* Adjust the pointers and counters for the next line. */
        address = (char *) address + num_bytes_this_line;
        buffer = (char *) buffer + num_bytes_this_line;
        num_bytes_to_dump -= num_bytes_this_line;
    }

    /* Deallocate the temporary input/output buffers. */
    free (inbuf);
    free (outbuf);

    return (0);
}

/*
 *    meo_load () Load Memory from a File.
 *    Function meo_load() loads the binary contents of a memory region from a
 *    disk file.  The contents must have been previously saved using meoSave().
 *
 *    Invocation (dynamically allocated buffer):
 *        void  *start_address = NULL;
 *        ...
 *        status = meo_load (filename, offset, &start_address, &num_bytes);
 *
 *    Invocation (caller-specified buffer):
 *        void  *start_address = buffer;
 *        long  num_bytes = sizeof buffer;
 *        ...
 *        status = meo_load (filename, offset, &start_address, &num_bytes);
 *
 *    where:
 *        <filename>	- I
 *            is the name of the file from which the memory contents will be
 *            loaded.  Environment variables may be embedded in the file name.
 *        <offset>	- I
 *            is the byte offset within the file from which the load will begin.
 *        <start_address>	- I/O
 *            is the address of a (VOID *) pointer that specifies or returns the
 *            address where the contents of the file will be stored.  If the
 *            (VOID *) pointer is NULL, meo_load() will MALLOC(3) a buffer for
 *            the file contents and return its address through this argument;
 *            the caller is responsible for FREE(3)ing the memory when it is
 *            no longer needed.  If the (VOID *) pointer is NOT NULL, meo_load()
 *            uses it as the address of a caller-allocated buffer in which the
 *            file contents are to be stored; the size of the buffer is
 *            specified by the NUMBYTES argument.
 *        <num_bytes>	- I/O
 *            is the address of a longword that specifies or returns the size of
 *            the memory buffer.  If the memory buffer is dynamically  allocated
 *            by meo_load(), this argument returns the size of the buffer.  If
 *            meo_load() uses a caller-allocated buffer, this argument specifies
 *            the size of the buffer.
 *        <status>	- O
 *            returns the status of loading the memory from a file, zero
 *            if there were no errors and ERRNO otherwise.
 *
 */
int  meo_load (const char *filename, long offset, void **start_address,
	      long *num_bytes)
{
    static char mod[] = "meo_load";
    FILE *file;
    long fileSize;
    struct stat info;

    filename = fnm_build (FNMPATH, filename, NULL);
    file = fopen (filename, "r");
    if (file == NULL) {
        DEBUG_OUT(0, ("(%s) Error opening %s.\n",
                      mod, filename));
        return (errno);
    }

    /* Determine the amount of data to be loaded from the file. */
    if (fstat (fileno (file), &info)) {
        DEBUG_OUT(0, ("(%s) Error determining the size of %s.\n",
                      mod, filename));
        return (errno);
    }

    fileSize = info.st_size - offset;

    /* Allocate a memory buffer, if necessary. */
    if (*start_address == NULL) {
        *num_bytes = fileSize;
        *start_address = malloc (fileSize);
        if (*start_address == NULL) {
            DEBUG_OUT(0, 
                      ("(%s) Error allocating %ld-byte memory buffer for %s.\n",
                          mod, fileSize, filename));
            return (errno);
        }
  }

  /* Read the (possibly truncated) contents of the file into the memory pool. */
  if (fseek (file, offset, SEEK_SET) != offset) {
    DEBUG_OUT(0, ("(%s) Error positioning to offset %ld in %s.\nfseek: ",
                  mod, offset, filename));
    return (errno);
  }

  if (fileSize > *num_bytes)
    fileSize = *num_bytes;

  if (fread (*start_address, fileSize, 1, file) != 1) {
    DEBUG_OUT(0, ("(%s) Error loading %ld bytes from %s to %p.\nfread: ",
                  mod, fileSize, filename, *start_address));
    return (errno);
  }

  fclose (file);
  if (meo_util_debug)
      DEBUGADD(3, ("(%s) Loaded %ld bytes from %s to %p.\n",
				   mod, fileSize, filename, *start_address));
  return (0);
}

/*
 *    meoSave () Save Memory to a File.
 *    Function meoSave() saves the binary contents of a memory region to a
 *    disk file.  The contents can be reloaded at a later time using meo_load().
 *
 *    Invocation:
 *        status = meoSave (start_address, num_bytes, filename, offset);
 *
 *    where:
 *        <start_address>	- I
 *            specifies the start of the memory region that is to be saved.
 *        <num_bytes>	- I
 *            specifies the number of bytes of data to save.
 *        <filename>	- I
 *            is the name of the file to which the memory contents will be
 *            written.  Environment variables may be embedded in the file name.
 *        <offset>	- I
 *            is the byte offset within the file at which the save will begin.
 *        <status>	- O
 *            returns the status of saving the memory to a file, zero if
 *            there were no errors and ERRNO otherwise.
 *
 */
int meoSave(void *start_address, long num_bytes, const char *filename, 
            long offset)
{
    static char mod[] = "meoSave";
    FILE  *file;

    filename = fnm_build (FNMPATH, filename, NULL);
    file = fopen (filename, "w");
    if (file == NULL) {
        DEBUG_OUT(0, ("(%s) Error opening %s to save memory at %p.\n",
                      mod, filename, start_address));
        return (errno);
    }
    /* Write the contents of the memory out to the file. */
    if (fseek (file, offset, SEEK_SET) != offset) {
        DEBUG_OUT(0, ("(%s) Error positioning to offset %ld in %s.\nfseek: ",
                      mod, offset, filename));
        return (errno);
    }
    if (fwrite (start_address, num_bytes, 1, file) != 1) {
        DEBUG_OUT(0, ("(%s) Error saving %ld bytes at %p to %s.\n",
                      mod, num_bytes, start_address, filename));
        return (errno);
    }
    if (fclose (file)) {
        DEBUG_OUT(0, ("(%s) Error closing %s for memory at %p.\n",
                      mod, filename, start_address));
        return (errno);
    }
    if (meo_util_debug)
        DEBUG_OUT(3, ("(%s) Saved %ld bytes at %p to %s\n",
                      mod, num_bytes, start_address, filename));
    return (0);
}

#ifdef  TEST
/*
 *
 *    Program to test the MEO_UTIL() functions.
 *
 *    Under UNIX,
 *        compile and link as follows:
 *            % cc -g -DTEST meo_util.c -I<... includes ...> <libraries ...>
 *        and run with the following command line:
 *            % a.out <wildcard_file_spec>
 *
 *    Under VxWorks,
 *        compile and link as follows:
 *            % cc -g -c -DTEST -DVXWORKS meo_util.c -I<... includes ...> \
 *                       -o test_drs.o
 *            % ld -r test_drs.o <libraries ...> -o test_drs.vx.o
 *        load as follows:
 *            -> ld <test_drs.vx.o
 *        and run with the following command line:
 *            -> test_drs.vx.o "<wildcard_file_spec>"
 *
 */
main (int argc, cahr *argv[])
{
    char  *filename = argv[1];
    unsigned  char  buffer[256];
    long  i, num_bytes;
    void  *new_buffer;

    meo_util_debug = 1;
    vperror_print = 1;

    for (i = 0;  i < sizeof buffer;  i++)
        buffer[i] = i;

    meoSave (buffer, sizeof buffer, filename, 0);

    new_buffer = NULL;
    meo_load (filename, 0, &new_buffer, &num_bytes);

    printf ("\nOctal Dump:\n\n");
    MEO_DUMPO (NULL, "    ", 0, new_buffer, num_bytes);
    printf ("\nDecimal Dump:\n\n");
    MEO_DUMPD (NULL, "    ", 0, new_buffer, num_bytes);
    printf ("\nHexadecimal Dump:\n\n");
    MEO_DUMPX (NULL, "    ", 0, new_buffer, num_bytes);
    printf ("\nText Dump:\n\n");
    MEO_DUMPT (NULL, "    ", 0, new_buffer, num_bytes);
}
#endif
