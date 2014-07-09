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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>

#include "vlog.h"
#include "util.h" //for ovs_abort
#include "misc-util.h"


#undef PAG_USING_XSI
#undef PAG_USING_GNU
#if (_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) && ! _GNU_SOURCE
#    define PAG_USING_XSI
#else
#    define PAG_USING_GNU
#endif

VLOG_DEFINE_THIS_MODULE(misc_util);

/* ============================================================
 *
 * \brief pipe_write_na()
 *        Open a pipe for writing.
 *        Same as pipe_write except that on error, this function 
 *        does not abort, but returns errno in save_errno
 *
 * @param0
 *          cmd - string containing the command to execute
 * @param1
 *          save_errno - set to errno returned by popen()
 *
 * \return { FILE * to the pipe }
 *
 **/
FILE *pipe_write_na(const char *cmd, int *save_errno) {
    FILE *pipe_p;

    VLOG_ENTER(__func__);
    if (cmd == NULL)
        ovs_abort(-1,"NULL passed into %s",__func__);

    pipe_p = popen(cmd,"w");
    *save_errno = errno;

    if(pipe_p == NULL) {
        if(errno != 0)
            VLOG_ERR("errno is %d\n",*save_errno);
        else
            VLOG_ERR("pipe_p is NULL & errno 0 => fork or pipe failed.\n");
    }

    VLOG_LEAVE(__func__);
    return(pipe_p);
}

/* ============================================================
 *
 * \brief pipe_read_na()
 *        Open a pipe for reading.
 *        Same as pipe_read except that on error, this function 
 *        does not abort, but returns errno in save_errno
 *
 * @param0
 *          cmd - string containing the command to execute
 * @param1
 *          save_errno - set to errno returned by popen()
 *
 * \return { FILE * to the pipe }
 *
 **/
FILE *pipe_read_na(const char *cmd, int *save_errno) {
    FILE *pipe_p;

    VLOG_ENTER(__func__);

    if (cmd == NULL)
        ovs_abort(-1,"NULL passed into %s",__func__);

    pipe_p = popen(cmd,"r");
    *save_errno = errno;

    if(pipe_p == NULL) {
        if(errno != 0)
            VLOG_ERR("errno is %d\n",*save_errno);
        else
            VLOG_ERR("pipe_p is NULL & errno 0 => fork or pipe failed.\n");
    }

    VLOG_LEAVE(__func__);
    return(pipe_p);
}

/* ============================================================
 *
 * \brief pipe_write()
 *        Open a pipe for writing.
 *        On error, this function calls ovs_abort, which will
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

    VLOG_ENTER(__func__);
    if (cmd == NULL)
        ovs_abort(-1,"NULL passed into %s",__func__);

    pipe_p = popen(cmd,"w");
    save_errno = errno;
    if (pipe_p == NULL)
        strerr_wrapper(save_errno);

    VLOG_LEAVE(__func__);
    return(pipe_p);
}

/* ============================================================
 *
 * \brief pipe_read()
 *        Open a pipe for reading.
 *        On error, this function calls ovs_abort, which will
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

    VLOG_ENTER(__func__);

    if (cmd == NULL)
        ovs_abort(-1,"NULL passed into %s",__func__);

    pipe_p = popen(cmd,"r");
    save_errno = errno;
    if (pipe_p == NULL)
        strerr_wrapper(save_errno);

    VLOG_LEAVE(__func__);
    return(pipe_p);
}

/* ============================================================
 *
 * \brief pipe_close()
 *        Close an open pipe. On error this function calls ovs_abort,
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

    VLOG_ENTER(__func__);

    if (pipe_p == NULL)
        ovs_abort(-1,"NULL passed into %s",__func__);
    
    retval = pclose(pipe_p);
    save_errno = errno;

    VLOG_INFO("pclose returned %i and errno is %i\n", retval, errno);

    if (retval == -1)
        strerr_wrapper(save_errno);

    VLOG_LEAVE(__func__);
    return(retval);
}

/* ============================================================
 *
 * \brief pipe_close_na()
 *        Close an open pipe and pass error codes back.
 *
 * @param[]
 *          pipe_p - an open FILE *
 *          save_errno - holder for errno if needed
 *
 * \return { int containing termination status from process }
 *
 **/
int pipe_close_na(FILE *pipe_p, int *save_errno) {
    int retval = 0;

    VLOG_ENTER(__func__);

    *save_errno = 0;

    if (pipe_p == NULL)
        ovs_abort(-1,"NULL passed into %s",__func__);
    
    retval = pclose(pipe_p);
    *save_errno = errno;

    VLOG_LEAVE(__func__);
    return(retval);
}

/* ============================================================
 *
 * \brief strerr_wrapper()
 *        Convert errno to a string using thread-save strerror_r
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
        ovs_abort(save_errno, error_message);
    else
        ovs_abort(save_errno,"Unknown error %i/%i",str_err,save_errno);
#else /* PAG_USING_GNU */
    char *msg = NULL;
    msg = strerror_r(err_no,error_message,PAG_ERROR_MSG_LEN);
    if (msg != NULL)
        ovs_abort(save_errno, error_message);
    else
        ovs_abort(save_errno,"Unknown error %i",save_errno);
#endif
}

/*
 * @brief file-exists 
 *  check if a file exists
 *
 * @param0 char * of the file
 *
 * @return True if flie exists, else False if not
 *
 */
bool file_exist(char *fname)
{
    struct stat st;

    if (stat(fname,&st) != 0)
        return (false);      /* Not found. */

    return(true);
}

/*
 * @brief timestring()  Returns the date and time as a string
 *
 * @return the date string
 *
 */
char *timestring(void)
{
    static fstring tbuf;
    time_t t;
    struct tm *tm;

    t = time(NULL);
    tm = localtime(&t);
    if (!tm) {
        snprintf(tbuf, sizeof(tbuf)-1, "%ld seconds since the Epoch",  (long)t);
    } else {
        strftime(tbuf,100,"%b-%d %H:%M:%S",tm);
    }

    return(tbuf);
}

/*
 * @brief get_file_size() gets the file size
 *
 * @param0 char * the file path
 *
 * @return the size in bytes of the named file
 *
 */
size_t get_file_size(char *file_name)
{
    struct stat buf;
    buf.st_size = 0;
    if(stat(file_name,&buf) != 0)
        return (0)-1;
    return(buf.st_size);
}

/*
 * @brief pag_pthread_create() calls pthread_create()
 *        and manages the error conditions
 *
 * @param0 pthread_t * the pointer to the pthread_t struct
 *
 * @param1 pthread_attr_t * the pointer to the pthread_attr_t struct
 *
 * @param2 routine - the pointer to the function which the thread executes
 *
 * @param3 void * - pointer to the argument(s) passed into routine
 *
 * @return void
 *
 */
void pag_pthread_create(pthread_t *thread,
                        const pthread_attr_t *attr,
                        void *(*routine) (void *),
                        void *arg)
{
    int save_errno = 0;

    if ((save_errno = pthread_create(thread, attr, routine, arg)) != 0)
        ovs_abort(save_errno, "pthread_create failed");
}

/*
 * @brief pag_pthread_join() calls pthread_join()
 *        and manages the error conditions
 *
 * @param0 pthread_t - the pthread_t struct
 *
 * @param1 void ** exit status of the terminated thread
 *
 * @return void
 *
 */
void pag_pthread_join(pthread_t thread, void **retval)
{
    int save_errno = 0;

    if((save_errno = pthread_join(thread, retval)) != 0)
        ovs_abort(save_errno, "pthread_join failed");
}
