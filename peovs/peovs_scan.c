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
static void get_libvirt_domains(virConnectPtr);
static void get_libvirt_networks(virConnectPtr);
static void get_libvirt_interfaces(virConnectPtr);
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

    VLOG_ENTER(__func__);

    pe_scan_collect_libvirt(PEOVS_LIBVIRT_CONN_NAME);

    VLOG_LEAVE(__func__);
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
    virConnectPtr conn = NULL;
    virNodeInfo nodeinfo;
    char *capabilities = NULL;

    VLOG_ENTER(__func__);

    if((conn = virConnectOpen(lvirt)) == NULL)
        VLOG_ABORT("Failed to open %s\n",lvirt);

    memset(&nodeinfo,0,sizeof(virNodeInfo));

    capabilities = virConnectGetCapabilities(conn);

    virNodeGetInfo(conn, &nodeinfo);//TODO: go through virNodeInfo struct

    VLOG_INFO("Capabilities of %s are %s\n",lvirt,capabilities);
    VLOG_INFO("\n\nVirtualization type: %s\n", virConnectGetType(conn));

    get_libvirt_domains(conn);
    get_libvirt_networks(conn);
    get_libvirt_interfaces(conn);

    free(capabilities);
    virConnectClose(conn);

    VLOG_LEAVE(__func__);
}

static void get_libvirt_domains(virConnectPtr conn) {
    virDomainPtr *adomains = { 0, 0 };
    virDomainPtr *idomains = { 0, 0 };
    int count = 0;
    int nr_active_domains = 0;
    int nr_inactive_domains = 0;
    int rval = 0;

    VLOG_ENTER(__func__);

    rval = virConnectListAllDomains(conn, &adomains,
                VIR_CONNECT_LIST_DOMAINS_ACTIVE);
    if (rval <= -1) {
        nr_active_domains = 0;
        VLOG_ERR("virConnectListAllDomains returned an error for "
                 "active domains\n");
    } else {
        nr_active_domains = rval;
    }

    rval = virConnectListAllDomains(conn, &idomains,
                VIR_CONNECT_LIST_DOMAINS_INACTIVE);
    if (rval <= -1) {
        nr_inactive_domains = 0;
        VLOG_ERR("virConnectListAllDomains returned an error for "
                     "inactive domains\n");
    } else {
        nr_inactive_domains = rval;
    }


    if (nr_active_domains > 0) {
        VLOG_INFO("\nList of active (%d) domains:\n", nr_active_domains);
        for (count = 0; count < nr_active_domains; count++) {
            VLOG_INFO("    %s\n", virDomainGetName(adomains[count]));
            VLOG_INFO("    %s\n", virDomainGetXMLDesc(adomains[count],VIR_DOMAIN_XML_SECURE));
            virDomainFree(adomains[count]);
        }
    } else {
        VLOG_INFO("\nThere are no active domains\n");
    }

    if (nr_inactive_domains > 0) {
        VLOG_INFO("\nList of inactive (%d) domains:\n",nr_inactive_domains);
        for (count = 0; count < nr_inactive_domains; count++) {
            VLOG_INFO("    %s\n", virDomainGetName(idomains[count]));
            VLOG_INFO("    %s\n", virDomainGetXMLDesc(idomains[count],
                                  VIR_DOMAIN_XML_SECURE|VIR_DOMAIN_XML_INACTIVE));
            virDomainFree(idomains[count]);
        }
    } else {
        VLOG_INFO("\nThere are no inactive domains\n");
    }

    VLOG_LEAVE(__func__);
}

static void get_libvirt_networks(virConnectPtr conn) {
    virNetworkPtr *a_nets = { 0, 0 };
    virNetworkPtr *i_nets = { 0, 0 };
    int count = 0;
    int nr_active_nets = 0;
    int nr_inactive_nets = 0;
    int rval = 0;

    VLOG_ENTER(__func__);

    rval = virConnectListAllNetworks(conn, &a_nets,
                VIR_CONNECT_LIST_NETWORKS_ACTIVE);
    if (rval <= -1) {
        nr_active_nets = 0;
        VLOG_ERR("virConnectListAllNetworks returned an error for "
                 "active nets\n");
    } else {
        nr_active_nets = rval;
    }

    rval = virConnectListAllNetworks(conn, &i_nets,
                VIR_CONNECT_LIST_NETWORKS_INACTIVE);
    if (rval <= -1) {
        nr_inactive_nets = 0;
        VLOG_ERR("virConnectListAllNetworks returned an error for "
                     "inactive nets\n");
    } else {
        nr_inactive_nets = rval;
    }

    if (nr_active_nets > 0) {
        VLOG_INFO("\nList of active (%d) nets:\n", nr_active_nets);
        for (count = 0; count < nr_active_nets; count++) {
            VLOG_INFO("    %s\n", virNetworkGetName(a_nets[count]));
            VLOG_INFO("    %s\n", virNetworkGetXMLDesc(a_nets[count],
                                  0));
            virNetworkFree(a_nets[count]);
        }
    } else {
        VLOG_INFO("\nThere are no active nets\n");
    }

    if (nr_inactive_nets > 0) {
        VLOG_INFO("\nList of inactive (%d) nets:\n",nr_inactive_nets);
        for (count = 0; count < nr_inactive_nets; count++) {
            VLOG_INFO("    %s\n", virNetworkGetName(i_nets[count]));
            VLOG_INFO("    %s\n", virNetworkGetXMLDesc(i_nets[count],
                                  VIR_NETWORK_XML_INACTIVE));
            virNetworkFree(i_nets[count]);
        }
    } else {
        VLOG_INFO("\nThere are no inactive nets\n");
    }

    VLOG_LEAVE(__func__);
}
static void get_libvirt_interfaces(virConnectPtr conn) {
    virInterfacePtr *a_ifaces = { 0, 0 };
    virInterfacePtr *i_ifaces = { 0, 0 };
    int count = 0;
    int nr_active_ifaces = 0;
    int nr_inactive_ifaces = 0;
    int rval = 0;

    VLOG_ENTER(__func__);

    rval = virConnectListAllInterfaces(conn, &a_ifaces,
                VIR_CONNECT_LIST_INTERFACES_ACTIVE);
    if (rval <= -1) {
        nr_active_ifaces = 0;
        VLOG_ERR("virConnectListAllInterfaces returned an error for "
                 "active ifaces\n");
    } else {
        nr_active_ifaces = rval;
    }

    rval = virConnectListAllInterfaces(conn, &i_ifaces,
                VIR_CONNECT_LIST_INTERFACES_INACTIVE);
    if (rval <= -1) {
        nr_inactive_ifaces = 0;
        VLOG_ERR("virConnectListAllInterfaces returned an error for "
                     "inactive ifaces\n");
    } else {
        nr_inactive_ifaces = rval;
    }

    if (nr_active_ifaces > 0) {
        VLOG_INFO("\nList of active (%d) ifaces:\n", nr_active_ifaces);
        for (count = 0; count < nr_active_ifaces; count++) {
            VLOG_INFO("    %s\n", virInterfaceGetName(a_ifaces[count]));
            VLOG_INFO("    %s\n", virInterfaceGetXMLDesc(a_ifaces[count],
                                  0));
            virInterfaceFree(a_ifaces[count]);
        }
    } else {
        VLOG_INFO("\nThere are no active ifaces\n");
    }

    if (nr_inactive_ifaces > 0) {
        VLOG_INFO("\nList of inactive (%d) ifaces:\n",nr_inactive_ifaces);
        for (count = 0; count < nr_inactive_ifaces; count++) {
            VLOG_INFO("    %s\n", virInterfaceGetName(i_ifaces[count]));
            VLOG_INFO("    %s\n", virInterfaceGetXMLDesc(a_ifaces[count],
                                  VIR_INTERFACE_XML_INACTIVE));
            virInterfaceFree(i_ifaces[count]);
        }
    } else {
        VLOG_INFO("\nThere are no inactive ifaces\n");
    }

    VLOG_LEAVE(__func__);
}
