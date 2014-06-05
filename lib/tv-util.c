/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

/*
 * @File:tv_util.c
 *    These are utility functions used to manipulate (e.g., add and
 *    subtract) UNIX "timeval"s, which represent time in seconds and
 *    microseconds.
 */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "tv-util.h"

/*
*    tv_add - returns the sum of two TIMEVALs.
*
*    where
*        <time1>		- I
*            is the first operand, a time represented by a UNIX TIMEVAL
*            structure.
*        <time2>		- I
*            is the second operand, a time represented by a UNIX TIMEVAL
*            structure.
*        <result>	- O
*            returns, in a UNIX TIMEVAL structure, the sum of TIME1 and
*            TIME2.
*
*/
struct timeval tv_add (struct timeval time1, struct timeval time2)
{ 
    struct timeval result;

    /* Add the two times together. */
    result.tv_sec = time1.tv_sec + time2.tv_sec;
    result.tv_usec = time1.tv_usec + time2.tv_usec;
    if (result.tv_usec > 1000000) {			/* Carry? */
      result.tv_sec++;  result.tv_usec = result.tv_usec - 1000000;
    }
    return (result);
}

/*
*    tv_compare - compares two TIMEVALs.
*
*    Invocation:
*        comparison = tv_compare (time1, time2);
*
*    where
*        <time1>		- I
*            is a time represented by a UNIX TIMEVAL structure.
*        <time2>		- I
*            is another time represented by a UNIX TIMEVAL structure.
*        <comparison>	- O
*            returns an integer indicating how the times compare:
*                    -1  if TIME1 < TIME2
*                     0  if TIME1 = TIME2
*                    +1  if TIME1 > TIME2
*
*/
int tv_compare (struct timeval time1, struct timeval time2)
{
    if (time1.tv_sec < time2.tv_sec)
        return (-1);				/* Less than. */
    else if (time1.tv_sec > time2.tv_sec)
        return (1);				/* Greater than. */
    else if (time1.tv_usec < time2.tv_usec)
        return (-1);				/* Less than. */
    else if (time1.tv_usec > time2.tv_usec)
        return (1);				/* Greater than. */
    else
        return (0);				/* Equal. */

}

/*
*    tv_create - given a time specified in seconds and microseconds,
*    creates and returns a TIMEVAL representing that time.
*
*    Invocation:
*        result = tv_create (seconds, microseconds);
*
*    where
*        <seconds>	- I
*            is the whole number of seconds in the time.
*        <microseconds>	- I
*            is the remaining microseconds in the time.
*        <result>	- O
*            returns a UNIX TIMEVAL structure representing the specified time.
*
*/

struct timeval tv_create (long seconds, long microseconds)
{
    struct  timeval  result;

    seconds = (seconds < 0) ? 0 : seconds;
    microseconds = (microseconds < 0) ? 0 : microseconds;

    /* "Normalize" the time so that the microseconds field is less than a million. */
    while (microseconds >= 1000000) {
        seconds++;  microseconds = microseconds - 1000000;
    }

    /* Return the time in a TIMEVAL structure. */
    result.tv_sec = seconds;
    result.tv_usec = microseconds;
    return (result);
}

/*
*    tv_createf - given a floating-point number representing a
*    time in seconds and fractional seconds, creates and returns a TIMEVAL
*    representing that time.
*
*    Invocation:
*        result = tv_createf (fseconds);
*
*    where
*        <fseconds>	- I
*            is a floating-point number representing a time in seconds and
*            fractional seconds.
*        <result>	- O
*            returns a UNIX TIMEVAL structure representing the specified time.
*
*/
struct timeval tv_createf (double fseconds)
{
    struct timeval result;

    if (fseconds < 0) {					/* Negative time? */
        result.tv_sec = 0;  result.tv_usec = 0;
    } else if (fseconds > (double) LONG_MAX) {		/* Time too large? */
        result.tv_sec = LONG_MAX;  result.tv_usec = 999999;
    } else {						/* Valid time. */
        result.tv_sec = fseconds;
        result.tv_usec = (fseconds - (double) result.tv_sec) * 1000000.0;
    }
    return (result);
}

/*
*    tv_float - converts a TIMEVAL structure (seconds and microseconds)
*    into the equivalent, floating-point number of seconds.
*
*    Invocation:
*        realTime = tv_float (time);
*
*    where
*        <time>		- I
*            is the TIMEVAL structure that is to be converted to floating point.
*        <realTime>	- O
*            returns the floating-point representation in seconds of the time.
*
*/

double tv_float (struct timeval time)
{
    return ((double) time.tv_sec + (time.tv_usec / 1000000.0));
}

/*
*    tv_show - returns the ASCII representation of a binary TIMEVAL
*    structure (seconds and microseconds).  A caller-specified, strftime(3)
*    format is used to format the ASCII representation.
*
*    Invocation:
*        ascii_time = tv_show (binary_time, in_local, format);
*
*    where
*        <binary_time>	- I
*            is the binary TIMEVAL that will be converted to ASCII.
*        <in_local>	- I
*            specifies (true or false) if the binary time is to be adjusted
*            to local time by adding the GMT offset.
*        <format>	- I
*            is the STRFTIME(3) format used to format the binary time in
*            ASCII.  If this argument is NULL, the time is formatted as
*            "YYYY-DOY-HR:MN:SC.MLS".
*        <ascii_time>	- O
*            returns a pointer to an ASCII string, which contains the formatted
*            time.  The ASCII string is stored in multiply-buffered memory local
*            to tv_show(); tv_show() can be called 4 times before it overwrites
*            the result from the earliest call.
*
*/
char *tv_show (struct timeval binary_time, int in_local, const char *format)
{
    struct tm calendar_time;
#define  MAX_TIMES  4
    static  char  ascii_time[MAX_TIMES][64];
    static  int  current = 0;

    // Convert the TIMEVAL to calendar time: year, month, day, etc.
    if (in_local)
        calendar_time = *(localtime ((time_t *) &binary_time.tv_sec));
    else
        calendar_time = *(gmtime ((time_t *) &binary_time.tv_sec));

    /* Format the time in ASCII. */
    current = (current + 1) % MAX_TIMES;
    if (format == NULL) {
        strftime (ascii_time[current], 64, "%H:%M:%S", &calendar_time);
        sprintf (ascii_time[current] + strlen (ascii_time[current]),
                 ".%03ld", (binary_time.tv_usec % 1000000) / 1000);
    } else {
        strftime (ascii_time[current], 64, format, &calendar_time);
    }
    return (ascii_time[current]);
}

/*
*    tv_subtract - subtracts one TIMEVAL from another TIMEVAL.
*
*    Invocation:
*        result = tv_subtract (time1, time2);
*
*    where
*        <time1>		- I
*            is the minuend (didn't you graduate from elementary school?),
*            a time represented by a UNIX TIMEVAL structure.
*        <time2>		- I
*            is the subtrahend (I repeat the question), a time represented
*            by a UNIX TIMEVAL structure.
*        <result>	- O
*            returns, in a UNIX TIMEVAL structure, TIME1 minus TIME2.  If
*            TIME2 is greater than TIME1, then a time of zero is returned.
*
*/
struct timeval tv_subtract (struct timeval time1, struct timeval time2)
{
    struct timeval result;

    /* Subtract the second time from the first. */
    if ((time1.tv_sec < time2.tv_sec) ||
        ((time1.tv_sec == time2.tv_sec) &&
         (time1.tv_usec <= time2.tv_usec))) {		/* TIME1 <= TIME2? */
        result.tv_sec = result.tv_usec = 0;
    } else {						/* TIME1 > TIME2 */
        result.tv_sec = time1.tv_sec - time2.tv_sec;
        if (time1.tv_usec < time2.tv_usec) {
            result.tv_usec = time1.tv_usec + 1000000 - time2.tv_usec;
            result.tv_sec--;				/* Borrow a second. */
        } else {
            result.tv_usec = time1.tv_usec - time2.tv_usec;
        }
    }
    return (result);
}

/*
*    tv_tod - returns the current time-of-day (GMT).  tv_tod()
*    simply calls the UNIX system function, GETTIMEOFDAY(3), but it
*    provides a simple means of using the current time in "timeval"
*    calculations without having to allocate a special variable for
*    it and making a separate call (i.e., you can "inline" a call to
*    tv_tod() within a call to one of the other TV_UTIL functions).
*
*        NOTE: Under VxWorks, tv_tod() returns the current value
*              of the real-time clock, not GMT.
*
*    Invocation:
*        currentGMT = tv_tod ();
*
*    where
*        <current_gmt>	- O
*            returns, in a UNIX TIMEVAL structure, the current GMT.  (Under
*            VxWorks, the current value of the real-time clock is returned
*            instead of GMT.)
*
*/
struct timeval tv_tod (void)
{
    struct timeval current_gmt;

    gettimeofday (&current_gmt, NULL);
    return (current_gmt);

}
