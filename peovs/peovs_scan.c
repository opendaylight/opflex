/*
* Copyright (c) 2014 Cisco Systems, Inc. and others.
*               All rights reserved.
*
* This program and the accompanying materials are made available under
* the terms of the Eclipse Public License v1.0 which accompanies this
* distribution, and is available at
* http://www.eclipse.org/legal/epl-v10.html
*/
/*
 * The peovs_scan.c file contains functions that collect information
 * about the host on which the agent is running, including data from
 * libvirt and ovs.
 *
 * History:
 *      5-June-2014 smann@noironetworks.com = created.
 *     25-July-2014 smann@noironetworks.com = libvirt API code work.
 *
 */


#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>

#include "vlog.h"
#include "peovs_scan.h"

//TODO: move to conf-file (see peovs.h)
#define PEOVS_LIBVIRT_CONN_NAME "qemu:///system"

/* private protos */
static void pe_scan_collect_libvirt(const char *);
/**/

VLOG_DEFINE_THIS_MODULE(peovs_scan);

/* ====================================================================
 *
 * @brief pe_scan_init() 
 *        Initialize the scan of the host
 *
 * @param0 <none>       - I/O
 *         
 * @return <none>         - O
 *         
 *               
 **/
void pe_scan_init(void) {

    pe_scan_collect_libvirt(PEOVS_LIBVIRT_CONN_NAME);
}

/* ====================================================================
 *
 * @brief pe_collect_libvirt()
 *        Initialize the scan of the host
 *
 * @param0 <none>       - I/O
 *         
 * @return <none>         - O
 *         
 *               
 **/
static void pe_scan_collect_libvirt(const char *lvirt) {
    virConnectPtr conn;
    virNodeInfo nodeinfo;
    int count = 0;
    int nr_active_domains;
    int nr_inactive_domains;
    char *capabilities;
    int *active_domains;
    char **inactive_domains;

    if((conn = virConnectOpen(lvirt)) == NULL)
        VLOG_ABORT("Failed to open %s\n",lvirt);

    capabilities = virConnectGetCapabilities(conn);

    virNodeGetInfo(conn, &nodeinfo);//TODO: go through virNodeInfo struct

    nr_active_domains = virConnectNumOfDomains(conn);
    nr_inactive_domains = virConnectNumOfDefinedDomains(conn);

    active_domains = malloc(sizeof(int) * nr_active_domains);
    inactive_domains = malloc(sizeof(char *) * nr_inactive_domains);

    VLOG_INFO("Capabilities of %s are %s\n",lvirt,capabilities);
    VLOG_INFO("Virtualization type: %s\n", virConnectGetType(conn));
    
    VLOG_INFO("\nList of active domains:|n");
    for (count = 0; count < nr_active_domains; count++)
        VLOG_INFO("    %d\n", active_domains[count]);

    VLOG_INFO("\nList of inactive domains:|n");
    for (count = 0; count < nr_inactive_domains; count++)
        VLOG_INFO("    %s\n", inactive_domains[count]);

    free(active_domains);
    free(inactive_domains);
    free(capabilities);
    virConnectClose(conn);

}
