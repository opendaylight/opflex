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
 * COMMAND_DEFN(pe_ovs_command_t, char *, uint32_t)
 *              enum, command, number of args
 */

COMMAND_DEFN( CREATE_BRIDGE, "ovs-vsctl add-br", 1)
COMMAND_DEFN( DELETE_BRIDGE, "ovs-vsctl del-br", 1)
COMMAND_DEFN( CREATE_BROADCAST_DOMAIN, "foo", 2)
COMMAND_DEFN( CREATE_HOST_PORT, "foo", 2)
COMMAND_DEFN( CREATE_ROUTER, "foo", 2)
COMMAND_DEFN( ADD_BD_MEMBER, "foo", 2)
COMMAND_DEFN( ADD_FLOW_ARP, "foo", 2)
COMMAND_DEFN( ADD_FLOW_SELECT_SEPG, "foo", 2)
COMMAND_DEFN( ADD_FLOW_SELECT_DEPG_L2, "foo", 2)
COMMAND_DEFN( ADD_FLOW_SELECT_DEPG_L3, "foo", 2)
COMMAND_DEFN( ADD_FLOW_POLICY_DENY, "foo", 2)
COMMAND_DEFN( ADD_FLOW_POLICY_DROP, "foo", 2)
COMMAND_DEFN( ADD_FLOW_POLICY_PERMIT, "foo", 2)
COMMAND_DEFN( ADD_FLOW_POLICY_PERMIT_BI, "foo", 2)
COMMAND_DEFN( COMMIT_FLOWS, "foo", 2)
