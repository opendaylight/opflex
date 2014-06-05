#!/usr/bin/env python
#
# Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v1.0 which accompanies this distribution,
# and is available at http://www.eclipse.org/legal/epl-v10.html
#
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

from CommandServer import Service
import sys, os
import logging
from opflexService import *
import simplejson as json
import socket
from time import time, sleep

import pdb

class consoleService(Service):
    DB_LEVEL = logging.DEBUG

    def __init__(self, log):
        self.log = logging.getLogger("%s" % (self.__class__.__name__))
        self.log.setLevel(self.DB_LEVEL)
        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)
        self.log.debug("[%s]<--Called." % (mod))

        self.quit_flag = False
        self.server = opflexService.server
        self.conn_tuple = opflexService.conn
        opflexService.console = self

        # basic commands the the generic service provides in the command lookup.
        self.cmdLookup = {'kill':       self.kill_handler,
                          'quit':       self.quit_handler,
                          }

        self.log.debug("[%s}-->return." % (mod))

    def serve(self, connTuple):
        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)
        self.log.debug("[%s]<--Called." % (mod))
        opflexService.console_tuple = connTuple

        c = connTuple[0]
        
        fileName = None
        path = None
        fd = None
        msg = None

        # -- the command processor for this service
        while self.quit_flag == False:            
#            pdb.set_trace()
            c.send("OpFlex:> ")
            cmd = c.recv(256)
            if (len(cmd) == 0):
                break
            cLine = cmd.split()
            self.log.debug("[%s] cLine = %s" % (mod, cLine))

            k = cLine[0].lower()
            retc = 0
            if (self.cmdLookup.has_key(k)):
                retc = self.cmdLookup[k](c,cmd)
                if (k == 'quit'):
                    c.close()
                    return
                
                if (retc):
                    self.log.error(
                        "[%s] error return from function %s:%d" % (mod,
                                                                   k,
                                                                   retc))
            else:
                c.send("Error: Command Unknown: %s\n", cmd)
           
        self.log.debug("[%s}-->return." % (mod))


    def close(self):
        pass


    # **************************************************************************
    # The handlers
    # **************************************************************************

    # ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    # @brief dataHandler - executes everything after the command, without question.
    #        If a failure is detectedt then all I/O to the sub-process is closed
    #        and any error info is returned to the client.
    #
    def kill_handler(self, conn, cmd):
        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)
        self.log.debug("[%s]<--Called:cmd=%s" % (mod, cmd))
        retc = 0

        self.server.quit_flag = True
        while opflexService.server != None:
            sleep(1)
            
            
            self.log.debug("[%s}-->return: rtnMsg=%d." % (mod, retc))
            return retc

    def quit_handler(self, conn, cmd):
        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)
        self.log.debug("[%s]<--Called:cmd=%s" % (mod, cmd))
        retc = 0
        self.quit_falg = True
        self.log.debug("[%s}-->return: rtnMsg=%d." % (mod, retc))
        return retc

