#!/usr/bin/env python
#
# Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v1.0 which accompanies this distribution,
# and is available at http://www.eclipse.org/legal/epl-v10.html
#
#
# @File opflexService.py
#
# @Author   dkehn@noironetworks.com
# @date 30-APR-2014
# @version: 1.0
#
# @brief A service to provide OpFlex protocol and console.
# 
# @example:
#
# Revision History:  (latest changes at top)
#   dkehn@nironetworks.com - 30-APR-2014 - created
#
###############################################################################

__author__ = 'dkehn@noironetworks.com'
__version__ = '1.0'
__date__ = '21-APR-2008'


# ****************************************************************************
# Ensure booleans exist.  If not, "create" them
#
try:
    True

except NameError:
    False = 0
    True = not False

import sys, os
from time import time, sleep
import logging
import socket
from CommandServer import Service
import simplejson as json

import pdb

class opflexService(Service):
    DB_LEVEL = logging.DEBUG
    server = None             # server instance
    conn = None               # connTuple passed from Listener
    console = None            # console instance
    console_tuple = None

    def __init__(self, log):
        self.log = logging.getLogger("%s" % (self.__class__.__name__))
        self.log.setLevel(self.DB_LEVEL)
        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)
        self.log.debug("%s:<--Called." % (mod))
        self.quit_flag = False

        if opflexService.server is None:
            opflexService.server = self

        # basic commands the the generic service provides in the command lookup.
        self.cmdLookup = {
            'identity':             self.identity_handler,
            'echo':                 self.echo_handler,
            "resolve_policy":       self.resolve_policy_handler,
            "udpate_policy":        self.update_policy_handler,
            "trigger_policy":       self.trigger_policy_handler,
            "endpoint_declaration": self.endpoint_declaration_handler,
            "endpoint_request":     self.endpoint_request_handler,
            "endpoint_update":      self.endpoint_update_handler,
            "state_report":         self.state_report_handler,
            'quit':                 self.quit_handler,
            'error':                self.error_handler,
            }
        
        self.log.debug("[%s}-->return." % (mod))

    def serve(self, connTuple):
        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)
        self.log.debug("%s:<--Called." % (mod))
        self.quitFlag = False;

        if opflexService.conn is None:
            opflexService.conn = connTuple

        conn = connTuple[0]
        
        pdb.set_trace()
        # -- the command processor for this service
        while (self.quit_flag == False):
            opf_msg = conn.recv(4096)
            if not opf_msg:
                break
            msg = self.deserialize_msg(opf_msg)
            self.cmdLookup[msg['method']](conn, msg)


        opflexService.server = None
        opflexService.conn = None
        conn.close()

        self.log.debug("[%s}-->return." % (mod))
        return

    def close(self):
        pass

    def deserialize_msg(self, msg):
        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)
        self.log.debug("%s:<--" % (mod))
        raw_msg = {}

        # TODO: dkehn should probably check this agains a known set of keys.
        try:
            raw_msg = json.loads(msg)
        except:
            errmsg = ("raw_msg=%s" % (raw_msg))
            raw_msg = {'method': 'error', 'msg': errmsg}

        self.log.debug("%s:-->.%s" % (mod, raw_msg))
        return raw_msg

    # **************************************************************************
    # The handlers
    # **************************************************************************

    def get_lan_ip(self):
        ip = socket.gethostbyname(socket.gethostname())
        if ip.startswith("127."):
            interfaces = [
                "eth0",
                "eth1",
                "eth2",
                "eth3",
                "eth4",
                "wlan0",
                "wlan1",
                "wifi0",
                "ath0",
                "ath1",
                "ppp0",
                ]
            for ifname in interfaces:
                try:
                    ip = get_interface_ip(ifname)
                    break
                except IOError:
                    pass
        return ip

    def validate_keys(self, key_list, params):
        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)
        self.log.debug("%s:<--keys=%s, buf=%s" % (mod, keys, params))
        validates = True

        for k in params.keys():
            if k not in key_list:
                validates = False
                break;

        self.log.debug("%s:<--validates=%d" % (mod, vlaidates))

    # --- quit handler
    # the quit will terminate this service, this is NOT part of the OpFlex
    # protocol, but is a handle way to kill this from the client.
    def quit_handler(self, conn, msg):
        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)
        self.log.debug("%s:<--" % (mod))        
        self.quit_flag = True
        self.log.debug("%s:<--quit_flag=%d" % (mod, self.quit_flag))

    # --- error handler
    # Prints out whats in the error msg.
    def error_handler(self, conn, msg):
        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)
        self.log.debug("%s:<--" % (mod))
        self.log.error(msg)
        self.log.debug("%s:<--quit_flag=%d" % (mod, self.quit_flag))

    # --- Identity handling
    #    This method identifies the participant to its peer in the protocol
    #    exchange and MUST be sent as the first OpFlex protocol method.  The
    #    method indicates the transmitter's role and the administrative domain
    #    to which it belongs.  Upon receiving an Identity message, the
    #    response will contain the configured connectivity information that
    #    the participant is using to communicate with each of the OpFlex
    #    components.  If the response receiver is a Policy Element and is not
    #    configured with connectivity information for certain OpFlex logical
    #    components, it SHOULD use the peer's connectivity information to
    #    establish communication with the OpFlex logical components that have
    #    not been locally configured.
    #
    #    The Identity request contains the following members:
    #    o  "method": "send_identity"
    #    o  "params": [
    #          "name": <string>
    #          "domain": <string>
    #          ["my_role": <role>]+
    #          ]
    #
    #    o result: [
    #          "name": <string>
    #          [ "my_role": <role> ]+
    #          "domain": <string>
    #          [ {"role": <role>
    #          "connectivity_info": <string>}* ]
    #          ]
    def identity_handler(self, conn, msg):
        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)
        self.log.debug("%s:<--%s" % (mod, msg))

        pdb.set_trace()
        ip = self.get_lan_ip()
        keys = ['domain', 'name', 'my_role', 'id']
        response = {
            "result": {
                "name": ("controller:%s" % ip),
                "my_roles": ["controller"],
                "domain": "my_domeain",
                "id": "24df1d04-d5cb-41e1-8de5-61ed77c558df",
                "roles": [{"role": "controller", "connectivity_info": ("%s" % ip)},
                 {"role": "repository", "connectivity_info": ("%s" % ip)}]
                }
            }

        self.log.info("IDENTITY: %s" % msg)

        if (self.validate_keys(keys, msg['params'])):
            json_rsp = json.encode(response);
            self.log.debug("%s: json_rsp=%s", mod, json_rsp)
            conn.send(json_rsp)
        else:
            self.log.error("%s: JSON message validation error: expected keys=%s, actual=%s",
                           mod, keys, msg['params'].keys())

        
        self.log.debug("%s:<--" % (mod))
        
    # --- echo handling
    # The "echo" method can be used by OpFlex Control Protocol peers to
    # verify the liveness of a connection.  It MUST be implemented by all
    # participants.  The members of the request are:
    # o  "method": "echo"
    # o  "params": JSON array with any contents
    # o  "id": <nonnull-json-value>
    #
    # The response object has the following members:
    # o  "result": same as "params"
    # o  "error": null
    # o  "id": same "id" as request
    def echo_handler(self, conn, msg):
        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)
        self.log.debug("%s:<--%s" % (mod, msg))

        pdb.set_trace()
        ip = self.get_lan_ip()

        response = {
            "result": {
                "name": ("controller:%s" % ip),
                "my_role": [{"my_role": "controller"}],
                "domain": "24df1d04-d5cb-41e1-8de5-61ed77c558df",
                "role": [{"role": "controller", "connectivity_info": ("%s" % ip)},
                         {"role": "repository", "connectivity_info": ("%s" % ip)}
                         ]
                }
            }

        self.log.debug("%s: Validating" % mode);
        
        self.log.info("IDENTITY: %s" % msg)
        self.log.debug("%s:<--" % (mod))

    # --- resolve_policy handling
    # This method retrieves the policy associated with the given policy
    # name.  The policy is returned as a set of managed objects.  This
    # method is typically sent by the PE to the PR.
    #
    # The request object contains the following members:
    # o  "method": "resolve_policy"
    # o  "params": [
    #       "subject": <string>
    #       "context": <string>
    #       "policy_name": <string>
    #       "on_behalf_of": <URI>
    #       "data": <string>
    #       ]
    #
    # Upon successful policy resolution, the response object contains the
    # following members:
    #
    # o  "result": [
    #       "policy": <mo>+,
    #       "prr": <integer>]
    #   
    def resolve_policy_handler(self, conn, msg):
        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)
        self.log.debug("%s:<--%s" % (mod, msg))

        pdb.set_trace()
        self.log.debug("%s:<--" % (mod))

    # --- update_policy handling
    # This method is sent to Policy Elements when there has been a change
    # of policy definition for policies which the Policy Element has
    # requested resolution.  Policy Updates will only be sent to Policy
    # Element for which the policy refresh rate timer has not expired.
    #
    # The Policy Update contains the following members:
    #
    # o  "method": "update_policy"
    #
    # o  "params": [
    #
    #       "context": <named_tlv>
    #       ["subtree": <mo>+]+
    #       "prr": <integer>
    #       ]
    # The response object contains the following members:
    # o  "result": {}
    # o  "error": null
    # o  "id": same "id" as request
    #
    def update_policy_handler(self, conn, msg):
        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)
        self.log.debug("%s:<--%s" % (mod, msg))

        pdb.set_trace()
        self.log.debug("%s:<--" % (mod))

    # --- trigger_policy handling
    # A policy trigger is issued from one Policy Element to a peer Policy
    # Element in order to trigger a policy resolution on the peer.  It is
    # typically done to indicate a attachment state change or a change in
    # the consumption of the peer resources.  For example, a Policy Element
    # in a switch may cause a policy trigger in the upstream switch to
    # enable a particular VLAN on the peer's interface.  It may also be
    # issued upon receiving a Policy Update or Policy Resolution response.
    #
    # The Policy Trigger contains the following members:
    #
    # o  "method": "trigger_policy"
    # o  "params": [
    #       "policy_type": <string>
    #       "context": <string>
    #       "policy_name": <string>
    #       "prr": <integer>
    #       ]
    #
    # o  "id": <nonnull-json-value>
    #
    # The response object contains the following members:
    # o  "result": {}
    # o  "error": null
    # o  "id": same "id" as request
    #
    def trigger_policy_handler(self, conn, msg):
        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)
        self.log.debug("%s:<--%s" % (mod, msg))

        pdb.set_trace()
        self.log.debug("%s:<--" % (mod))

    # --- endpoint_declaration_handler
    # This method is used to indicate the attachment and detachment of an
    # endpoint.  It is sent from the Policy Element to the Endpoint
    # Registry.
    #
    # The Endpoint Declaration contains the following members:
    #
    # o  "method": "endpoint_declaration"
    # o  "params": [
    #       "subject": <string>
    #       "context": <string>
    #       "policy_name": <string>
    #       "location": <URI>
    #       ["identifier": <string>]+
    #       ["data": <string>]*
    #       "status": <status>
    #       "prr": <integer>
    #       ]
    #
    #
    # The response object contains the following members:
    # o  "result": {}
    # o  "error": null
    # o  "id": same "id" as request

    def endpoint_declaration_handler(self, conn, msg):
        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)
        self.log.debug("%s:<--%s" % (mod, msg))

        pdb.set_trace()
        self.log.debug("%s:<--" % (mod))

    # --- endpoint_request_handler
    # This method queries the EPR for the registration of a particular EP.
    # The request is made using the identifiers of the endpoint.  Since
    # multiple identifiers may be used to uniquely identify a particular
    # endpoint, there may be more than 1 endpoint returned in the reply if
    # the identifiers presented do not uniquely specify the endpoint.
    #
    # The Endpoint Request contains the following members:
    # o  "method": "endpoint_request"
    # o  "params": [
    #       "subject": <string>
    #       "context": <string>
    #       ["identifier": <string>]+
    #       ]
    #
    # The response object contains the registrations of one or more
    # endpoints.  Each endpoint contains the same information that was
    # present in the original registration.  The following members are
    # present in the response:
    #
    # o  "result": {
    #       [ endpoint :
    #       {"subject": <string>
    #       "context": <string>
    #       "policy_name": <string>
    #       "location": <URI>
    #       ["identifier": <string>]+
    #       ["data": <string>]*
    #       "status": <status>
    #       "prr": <integer>
    #       } ]+
    #       }
    #
    def endpoint_request_handler(self, conn, msg):
        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)
        self.log.debug("%s:<--%s" % (mod, msg))

        pdb.set_trace()
        self.log.debug("%s:<--" % (mod))

    # --- endpoint_update_handler
    #    This method is sent to Policy Elements by the EPR when there has been
    #    a change relating to the EP Declaration for an Endpoint that the
    #    Policy Element has requested.  Policy Updates will only be sent to
    #    Policy Elements for which the Policy Refresh Rate timer timer for the
    #    Endpoint Request has not expired.
    #
    #    The Endpoint Policy Update contains the following members:
    #    o  "method": "endpoint_update_policy"
    #    o  "params": [
    #          "subject": <string>
    #          "context": <string>
    #          "policy_name": <string>
    #          "location": <URI>
    #          ["identifier": <string>]+
    #          ["data": <string>]*
    #          "status": <status>
    #          "ttl": <integer>
    #          ]
    #    o  "id": <nonnull-json-value>
    #
    #    All of the "params" contain identical information to the descriptions
    #    given as part of the Endpoint Declaration.
    #
    #    The response object contains the following members:
    #    o  "result": {}
    #    o  "error": null
    #    o  "id": same "id" as request
    #
    def endpoint_update_handler(self, conn, msg):
        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)
        self.log.debug("%s:<--%s" % (mod, msg))

        pdb.set_trace()
        self.log.debug("%s:<--" % (mod))

    # --- state_report_handler
    #    This method is sent by the Policy Element to the Observer.  It
    #    provides fault, event, statistics, and health information in the form
    #    of managed objects.
    #
    #    The State Report contains the following members:
    #    o  "method": "report_state"
    #    o  "params": [
    #          "subject": <URI>
    #          "context": <string>
    #          "object": <mo>
    #          ["fault": <mo>]*
    #          ["event": <mo>]*
    #          ["statistics": <mo>]*
    #          ["health": <mo>]*
    #          ]
    #
    #    The response object contains the following members:
    #    o  "result": {}
    #
    #    o  "error": null
    #
    #    o  "id": same "id" as request
    #
    def state_report_handler(self, conn, msg):
        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)
        self.log.debug("%s:<--%s" % (mod, msg))

        pdb.set_trace()
        self.log.debug("%s:<--" % (mod))
