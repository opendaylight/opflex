/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef ACCESS_UTIL_H 
#define ACCESS_UTIL_H 
/*
*        Header stuff for the access utilities
* 
* To Do: 
* 
* Revision History:  (latest changes at top) 
*    08312001 - dek - created.
* 
*/
#include <stdbool.h>

extern int access_db_level ;

extern bool allow_access(char *deny_list, char *allow_list, char *cname,
                         char *caddr);
extern bool check_access(int sock,  char *allow_list, char *deny_list);
extern char *client_addr(int fd);
extern char *client_name(int fd);
extern bool is_ipaddress(const char *str);
extern unsigned int interpret_addr(char *str);
extern bool matchname(char *remotehost,struct in_addr  addr);

#endif /* ACCESS_UTIL_H */
