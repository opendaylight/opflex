/*
* Copyright (c) 2014 Cisco Systems, Inc. and others.
*               All rights reserved.
*
* This program and the accompanying materials are made available under
* the terms of the Eclipse Public License v1.0 which accompanies this
* distribution, and is available at
* http://www.eclipse.org/legal/epl-v10.html
*/

/* This file is used to synchronize the enums with
 * commands and arguments
 * Note that it may be included more than once in
 * a file.
 */

/* Syntax:
 * COMMAND_DEFN(pe_ovs_command_t, char *, char *)
 *              enum, command, subcommand
 */

COMMAND_DEFN( VS_CREATE_BRIDGE, "ovs-vsctl", "add-br")
COMMAND_DEFN( VS_DELETE_BRIDGE, "ovs-vsctl", "del-br")
COMMAND_DEFN( OF_ADD_GROUP, "ovs-ofctl", "add-group")
COMMAND_DEFN( CREATE_HOST_PORT, "foo", "bie")
COMMAND_DEFN( CREATE_ROUTER, "foo", "bie")
COMMAND_DEFN( ADD_BD_MEMBER, "foo", "bie")
COMMAND_DEFN( ADD_FLOW_ARP, "foo", "bie")
COMMAND_DEFN( ADD_FLOW_SELECT_SEPG, "foo", "bie")
COMMAND_DEFN( ADD_FLOW_SELECT_DEPG_L2, "foo", "bie")
COMMAND_DEFN( ADD_FLOW_SELECT_DEPG_L3, "foo", "bie")
COMMAND_DEFN( ADD_FLOW_POLICY_DENY, "foo", "bie")
COMMAND_DEFN( ADD_FLOW_POLICY_DROP, "foo", "bie")
COMMAND_DEFN( ADD_FLOW_POLICY_PERMIT, "foo", "bie")
COMMAND_DEFN( ADD_FLOW_POLICY_PERMIT_BI, "foo", "bie")
COMMAND_DEFN( COMMIT_FLOWS, "foo", "bie")
