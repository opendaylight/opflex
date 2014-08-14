/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */
#ifndef OPFLEX_H
#define OPFLEX_H 1


/* opflex dispatcher commands
 */
typedef enum _enum_opflex_dcmd {
    OPFLEX_DCMD_IDENTIFY = 0,
    OPFLEX_DCMD_POL_RES,
    OPFLEX_DCMD_POL_UPD,
    OPFLEX_DCMD_ECHO,
    OPFLEX_DCMD_POL_TRIG,
    OPFLEX_DCMD_EP_DCL,
    OPFLEX_DCMD_EP_RQST,
    OPFLEX_DCMD_EP_POL_UPD,
    OPFLEX_DCMD_CLOSE,
    OPFLEX_DCMD_KILLALL,
} enum_opflex_dcmd;

/* Public routines
 */
extern bool opflex_send(enum_opflex_dcmd cmd, char *target);
extern char *opflex_recv(char *target, long timeout_secs);
extern void opflex_list_delete(struct list *list);
extern bool opflex_dispatcher(session_p sessp);

#endif
