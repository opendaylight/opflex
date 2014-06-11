/*
* Copyright (c) 2014 Cisco Systems, Inc. and others.
*               All rights reserved.
*
* This program and the accompanying materials are made available under
* the terms of the Eclipse Public License v1.0 which accompanies this
* distribution, and is available at
* http://www.eclipse.org/legal/epl-v10.html
*/

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <features.h>

#include "dbug.h"
#include "util.h" //for pag_abort
#include "misc-util.h"


#undef PAG_USING_XSI
#undef PAG_USING_GNU
#if (_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) && ! _GNU_SOURCE
#    define PAG_USING_XSI
#else
#    define PAG_USING_GNU
#endif

/* ============================================================
 *
 * \brief pipe_write()
 *        Remove an entry from the ring buffer.
 *        On error, this function calls pag_abort, which will
 *        dump core.
 *
 * @param[]
 *          cmd - string containing the command to execute
 *
 * \return { FILE * to the pipe }
 *
 **/
FILE *pipe_write(const char *cmd) {
    FILE *pipe_p;
    int save_errno = 0;

    DBUG_ENTER("pipe_write");
    if (cmd == NULL)
        pag_abort(-1,"NULL passed into %s",__func__);

    pipe_p = popen(cmd,"w");
    save_errno = errno;
    if (pipe_p == NULL)
        strerr_wrapper(save_errno);

    DBUG_LEAVE;
    return(pipe_p);
}

/* ============================================================
 *
 * \brief pipe_read()
 *        Remove an entry from the ring buffer.
 *        On error, this function calls pag_abort, which will
 *        dump core.
 *
 * @param[]
 *          cmd - string containing the command to execute
 *
 * \return { FILE * to the pipe }
 *
 **/
FILE *pipe_read(const char *cmd) {
    FILE *pipe_p;
    int save_errno = 0;

    DBUG_ENTER("pipe_read");

    if (cmd == NULL)
        pag_abort(-1,"NULL passed into %s",__func__);

    pipe_p = popen(cmd,"r");
    save_errno = errno;
    if (pipe_p == NULL)
        strerr_wrapper(save_errno);

    DBUG_LEAVE;
    return(pipe_p);
}

/* ============================================================
 *
 * \brief pipe_close()
 *        Close an open pipe. On error this function calls pag_abort,
 *        which will cause a core dump.
 *
 * @param[]
 *          pipe_p - an open FILE *
 *
 * \return { int containing termination status from process }
 *
 **/
int pipe_close(FILE *pipe_p) {
    int retval = 0;
    int save_errno = 0;

    DBUG_ENTER("pipe_close");

    if (pipe_p == NULL)
        pag_abort(-1,"NULL passed into %s",__func__);
    
    retval = pclose(pipe_p);
    save_errno = errno;
    if (retval == -1)
        strerr_wrapper(save_errno);

    DBUG_LEAVE;
    return(retval);
}

/* ============================================================
 *
 * \brief ring_buffer_pop()
 *        Remove an entry from the ring buffer.
 *
 * @param[]
 *          none
 *
 * \return { void * containing the pointer retrieved from the buffer }
 *
 **/
void strerr_wrapper(int err_no) {
    int save_errno = 0;
    char *error_message = NULL;

    error_message = xmalloc(PAG_ERROR_MSG_LEN);

#ifdef PAG_USING_XSI
    int str_err = 0;
    str_err = strerror_r(err_no,error_message,PAG_ERROR_MSG_LEN);
    save_errno = errno;
    if (str_err == 0)
        pag_abort(save_errno, error_message);
    else
        pag_abort(save_errno,"Unknown error %i/%i",str_err,save_errno);
#else /* PAG_USING_GNU */
    char *msg = NULL;
    msg = strerror_r(err_no,error_message,PAG_ERROR_MSG_LEN);
    if (msg != NULL)
        pag_abort(save_errno, error_message);
    else
        pag_abort(save_errno,"Unknown error %i",save_errno);
#endif
}
