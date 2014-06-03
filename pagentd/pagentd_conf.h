/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef	_PAGENTD_CNF_H
#define _PAGENTD_CNF_H 
/*
 *
 * @File pagentd_conf.h 
 * 
 * @brief 
 * 
 * Considerations: 
 * 
 * To Do: 
 * 
 * Revision History:  (latest changes at top) 
 *      13-MAR-2014 - dkehn@noironetworks.com - created.
 * 
 * 
 */

#ifdef __cplusplus
    extern  "C" {
#endif

#include <stdio.h>
#include <stdbool.h>


/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 
 * protos  
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/ 
extern bool pagentd_conf_load(const char *cnf_fname);
extern bool pagentd_conf_isloaded(void);
extern void pagentd_conf_dump(void);
extern char *pagentd_conf_getitem(const char *key);

#ifdef __cplusplus
    }
#endif

#endif 
