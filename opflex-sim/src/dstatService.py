#!/usr/bin/env python
###############################################################################
#
#
# Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v1.0 which accompanies this distribution,
# and is available at http://www.eclipse.org/legal/epl-v10.html
#
# @File dstatService.py
#
# @Author   dkehn@noironetworks.com
# @date 09-MAR-2008
# @version: 1.0
#
# @brief A service to handle a remote dstat for NDB monitoring.
# 
# @example:
#
# Revision History:  (latest changes at top)
#    don@noironetworks.com - 09-MAR-2008 - created
#
###############################################################################

__author__ = 'don@noironetworks.com'
__version__ = '1.0'
__date__ = '09-MAR-2008'


# ****************************************************************************
# Ensure booleans exist.  If not, "create" them
#
try:
    True

except NameError:
    False = 0
    True = not False

import sys, os, popen2, signal
import time
import logging
import socket
from CommandServer import Service

class dstatService(Service):
    DB_LEVEL = logging.INFO

    def __init__(self, log):
        self.log = logging.getLogger("%s" % (self.__class__.__name__))
        self.log.setLevel(self.DB_LEVEL)
        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)
        self.log.debug("[%s]<--Called." % (mod))

        self.cmdLookup = {'run':       self.runHandler,
                          'stop':      self.stopHandler,
                          'results':   self.resultHandler,
                          'stat':      self.statHandler,
                          'quit':      self.quitHandler
                          }


        self.runcmd = None
        self.dstat = None
        self.dstatRunningFlag = False
        
        self.log.debug("[%s}-->return." % (mod))

    def serve(self, connTuple):
        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)
        self.log.debug("[%s]<--Called." % (mod))

        c = connTuple[0]

        # -- the command processor for this service
        while True:
            c.send("rmt_dstat> ")
            cmd = c.recv(128)
            self.log.debug("[%s] cmd recv: %s" % (mod, cmd))
            if (len(cmd) == 0):
                break
            cLine = cmd.split()
            self.log.debug("[%s] cLine = %s" % (mod, cLine))

            k = cLine[0].lower()
            retc = 0
            if (self.cmdLookup.has_key(k)):
                #print self.cmdLookup[k]
                retc = self.cmdLookup[k](c,cmd)
                if (k == 'quit'):
                    c.close()
                    return
                
                if (retc):
                    self.log.error(
                        "[%s] error return from function %s:%d" % (mod,
                                                                   k,
                                                                   retc))
           
        self.log.debug("[%s}-->return." % (mod))


    def close(self):
        pass


    # **************************************************************************
    # The handlers
    # **************************************************************************

    # ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    # @brief runHandler - executes everything after the command, without question.
    #        If a failure is detectedt then all I/O to the sub-process is closed
    #        and any error info is returned to the client.
    #
    def runHandler(self, conn, cmd):
        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)
        self.log.debug("[%s]<--Called:cmd=%s" % (mod, cmd))
        retc = 0
        rtnMsg = None

        self.runcmd = cmd[cmd.find(' '):].strip()
        self.dstat = self.startProcess(self.runcmd)
        if (self.dstat == None):            
            rtnMsg = "Failed: %s" % (self.getErrors(self.dstat))
            self.closeAllChildIo(self.dstat)
            self.dstatRunningFlag = False
            #retc = -1
        else:
            self.dstatRunningFlag = True
            rtnMsg = " %s - running" % (cmd)


        conn.send("%s\n\r" % (rtnMsg))        
        self.log.debug("[%s}-->return: rtnMsg=%s." % (mod, rtnMsg))
        return retc
    
    # ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    # @brief stopHandler - terminates the monitor program, only!
    #
    def stopHandler(self, conn, cmd):
        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)
        self.log.debug("[%s]<--Called." % (mod))
        retc = 0

        if (self.isRunning()):
            os.kill(self.dstat.pid, signal.SIGKILL)
            self.resultHandler(conn, cmd)
        else:
            lcnt, str = self.getErrors(self.dstat)
            conn.send(str)
        
        self.log.debug("[%s}-->return." % (mod))
        return retc

    # ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    # @brief resultHandler - returns the results from the streams.
    #
    def resultHandler(self, conn, cmd):
        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)
        self.log.debug("[%s]<--Called." % (mod))
        retc = 0
        rtnStr = ""

        lcnt = 0
        for l in self.dstat.fromchild.xreadlines():
            rtnStr += l

        conn.send(rtnStr+'\n\r')
        self.log.debug("[%s}-->return." % (mod))
        return retc
    
    # ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    # @brief statHandler - returns the state of the running process.
    #
    def statHandler(self, conn, cmd):
        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)
        self.log.debug("[%s]<--Called." % (mod))
        retc = 0
        msg = "not running\n\r"

        if (self.isRunning()):
            msg = "running: pid=%d\n\r" % (self.dstat.pid)

        conn.send(msg)
        self.log.debug("[%s}-->return." % (mod))
        return retc
    
    # ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    # @brief quitHandler - shuts down this service.
    #
    def quitHandler(self, conn, cmd):
        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)
        self.log.debug("[%s]<--Called." % (mod))
        retc = 0

        if (self.isRunning()):
            os.kill(self.dstat.pid, signal.SIGKILL)
            self.closeAllChildIo(self.dstat)
            
        conn.send("terminating \n\r")
        self.log.debug("[%s}-->return." % (mod))
        return retc

    # ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    # @brief getErrors - extracts any error information from the sub-propcess's
    #        I/O channels and returns it the caller.
    #
    # @return (lcnt, errStr) - line count and the actually data from the streams.
    #
    def getErrors(self, child):
        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)
        self.log.debug("[%s]<--Called." % (mod))
        errStr =""
        errcnt = 0
        outcnt = 0
        lcnt = 0
        
        for l in child.childerr.xreadlines():
            if (errcnt == 0):
                errStr = "------------- STDERR ---------------------------\n\r"
                errcnt = 1
                
            errcnt += errcnt
            errStr += l

        for l in child.fromchild.xreadlines():
            if (outcnt == 0):
                errStr = "\n\r------------- STDOUT ---------------------------\n\r"
                outcnt = 1
                
            outcnt += outcnt
            errStr += l

        errStr += "\n\r"
        lcnt = errcnt + outcnt
        
        self.log.debug("[%s]-->return" % (mod))

        return (lcnt, errStr)

    # ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    # @brief closeAllChildIo
    #
    def closeAllChildIo(self, child):
        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)
        self.log.debug("[%s]<--Called." % (mod))

        child.fromchild.close()
        child.tochild.close()
        child.child.err.close()

        self.log.debug("[%s]-->return" % (mod))


    # ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    # @brief isRunning() - returns True if the dstat is running, else False.
    #
    def isRunning(self):
        #print self.dstat
        if ((self.dstat != None) and (self.dstat.poll() == -1)):
            return True

        return False
        
    # ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    # removeCmd
    #
    def removeCmd(self, cmd):
        return cmd[cmd.find(' '):]
        
        
    # ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    # @brief startProcess() - starts a process and returns the child.
    #
    def startProcess(self, cmd, closein=True):
        mod = sys._getframe().f_code.co_name
        self.log.debug("%s<-- Called:CMD->%s" % (mod, cmd))
        
        try:
            child = popen2.Popen3(cmd, True)
        except Exception, inst:
           self.log.critical("exception=%s\n\ttype=%s\n\tinstance=%s" %
                           (Exception, type(inst), inst.args))
           return None                                
            
        if (closein):
            child.tochild.close()
            
        if (child.poll() != -1):
            self.log.fatal("%s - %s is not running: " % (mod, cmd))
            for l in child.fromchild.xreadlines():
                self.log.error("%s - \t%s: " % (mod, l))

            for l in child.childerr.xreadlines():
                self.log.fatal("%s - \t%s: " % (mod, l))
            return None
            
        self.log.debug("%s-->return" % (mod))
        return child

