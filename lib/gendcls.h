#ifndef _GENDCLS_H
#define _GENDCLS_H
/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

/*
 * @File gendcls.h 
 * 
 * @brief General dcls for the library and other enviroments.
 *
 * Revision History:  (latest changes at top) 
 *    07-MAR-2014 - dkehn@noioronetworks.com - created
 * 
 */
#include <ansi-setup.h>

 #ifdef __cplusplus
extern "C" {
#endif

/*
 *  GLOBAL 
 */
//#define MAXPATHLEN  256
#define BOOLSTR(b) ((b) ? "Yes" : "No")
#define BITSETB(ptr,bit) ((((char *)ptr)[0] & (1<<(bit)))!=0)
#define BITSETW(ptr,bit) ((SVAL(ptr,0) & (1<<(bit)))!=0)

#define DEFAULT_CONFIGFILE      "/etc/opflex_proxy/opflexd.conf"
#define DEFAULT_PIDFILE         "/etc/opflexd_proxy/opflexd.pid"
#define	DEFAULT_VERSION		"1.1"
#define DEFAULT_LOGFILE		"/var/log/opflex_proxy/opflexd.log"
#define LIST_SEP " \t,;:\n\r"
#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif
#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif
#ifndef ABS
#define ABS(a) ((a)>0?(a):(-(a)))
#endif

#ifndef SIGNAL_CAST
#define SIGNAL_CAST (void (*)(int))
#endif

/*
 * GLOBAL STRUCTURES/TYPEDEFS 
 */
typedef char pstring[1024];
typedef char fstring[128];

/* case handling */
enum case_handling {CASE_LOWER,CASE_UPPER};

#define PTR_DIFF(p1,p2) ((size_t)(((const char *)(p1)) - (const char *)(p2)))


#ifdef __cplusplus
}
#endif

#include "debug.h"           /* debug decls */

#endif /* !_GENDCLS_H_ */

