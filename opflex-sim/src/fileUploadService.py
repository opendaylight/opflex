#!/usr/bin/env python
###############################################################################
#
# Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v1.0 which accompanies this distribution,
# and is available at http://www.eclipse.org/legal/epl-v10.html
#
# @File fileUploadService.py
#
# @Author   dkehn@mysql.com
# @date 21-APR-2008
# @version: 1.0
#
# @brief A service to handle a the uploading of a file to a server.
# 
# @example:
#
# Revision History:  (latest changes at top)
#    dkehn@noironetworks.com - 21-APR-2008 - created
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

import sys, os, popen2, signal
import time
import logging
import socket
from CommandServer import Service

class fileUploadService(Service):
    DB_LEVEL = logging.DEBUG

    def __init__(self, log):
        self.log = logging.getLogger("%s" % (self.__class__.__name__))
        self.log.setLevel(self.DB_LEVEL)
        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)
        self.log.debug("[%s]<--Called." % (mod))

        # basic commands the the generic service provides in the command lookup.
#        self.cmdLookup = {'name':       self.nameHandler,
#                          'length':     self.lengthHandler,
#                          'data':       self.dataHandler,
#                          'path':       self.pathHandler,
#                          'quit':       self.quitHandler
#                          }


        
        self.log.debug("[%s}-->return." % (mod))

    def serve(self, connTuple):
        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)
        self.log.debug("[%s]<--Called." % (mod))
        self.quitFlag = False;

        import pdb; pdb.set_trace()
        c = connTuple[0]
        
        fileName = None
        path = None
        fileLen = 0
        fd = None
        msg = None
        fileUploadRunning = False
        bytesTotal = 0
        quitFlag = False

        # -- the command processor for this service
        while True:
            c.send("fileUpload> ")           
            cmd = c.recv(8*1024)
            if (fileUploadRunning == True):
                try:
                    if (cmd[:4].lower() != 'quit'):
                        quitFlag = False                        
                        fd.write(cmd)
                        bytes = len(cmd)
                        bytesTotal += int(bytes)
                        self.log.debug("%s - %d byte recv, %d total bytes recv-ed" % 
                                       (mod, bytes, bytesTotal))
                    else:
                        quitFlag = True;
                        cmd = 'quit'

                    if ( (bytesTotal >= fileLen) or quitFlag == True):
                        fd.flush()
                        fd.close()
                        c.send("OK bytes=%d\n\r" %(bytesTotal))
                        fileUploadRunning = False
                    
                except Exception, e:
                    self.log.error("%s - exception=%s\n\ttype=%s\n\tinstance=%s" %
                                   (mod, Exception, type(e), e.args))
                    self.log.error("%s terminating" % (mod))
                    c.send("ERROR writing to %s\n\r" %(path+os.sep+fileName))

            ## Command processing
            self.log.debug("[%s] cmd recv: %s" % (mod, cmd))
            if (len(cmd) == 0):
                break
            cLine = cmd.split()
            #self.log.debug("[%s] cLine = %s" % (mod, cLine))

            k = cLine[0].lower()
            retc = 0
            if (k == 'quit'):
                c.send("TERMINATE\n\r")
                c.close()
                return
            elif (k == 'name'):
                fileName = cmd.split(' ')[1].strip()
                msg = "OK\n\r"
                
            elif (k == 'length'):
                fileLen = int(cmd.split(' ')[1].strip())
                msg = "OK\n\r"
                
            elif (k == 'path'):
                path = cmd.split(' ')[1].strip()
                msg = "OK\n\r"

                # check it out 
                if (os.path.exists(path)):
                    msg = "OK\n\r"
                else:
                    msg = "ERROR: path=%s does not exist.\n\r" % (path)
                
            elif (k == 'data'):
                if (path == None):
                    msg = ("ERROR no path specified.\n\r")

                elif (fileName == None):
                    msg = ("ERROR no file name specified.\n\r")

                elif (fileLen == 0):
                    msg = ("ERROR no file length given.\n\r")

                else:
                    # open up the file and get the fd
                    try:
                        fd = open(path+os.sep+fileName, "wb")
                        msg = ("OK\n\r")
                        fileUploadRunning = True
                        retc = 0
                        bytesTotal = 0
                        bytesWritten = 0
                    except Exception, e:
                        self.log.error("%s - exception=%s\n\ttype=%s\n\tinstance=%s" %
                                       (mod, Exception, type(e), e.args))
                        self.log.error("%s terminating" % (mod))
                        msg = ("ERROR can't open %s\n\r" %(path+os.sep+fileName))

            if (msg):
                c.send(msg)

        self.log.debug("[%s}-->return." % (mod))


    def close(self):
        pass


    # **************************************************************************
    # The handlers
    # **************************************************************************

#    # ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#    # @brief dataHandler - executes everything after the command, without question.
#    #        If a failure is detectedt then all I/O to the sub-process is closed
#    #        and any error info is returned to the client.
#    #
#    def dataHandler(self, conn, cmd):
#        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)
#        self.log.debug("[%s]<--Called:cmd=%s" % (mod, cmd))
#        retc = 0
#
#        if (self.path == None):
#            conn.send("ERROR no path specified.\n\r")
#            retc = -1
#        elif (self.fileName == None):
#            conn.send("ERROR no file name specified.\n\r")
#            retc = -2
#        elif (self.fileLen == 0):
#            conn.send("ERROR no file length given.\n\r")
#            retc = -3
#        else:
#            
#            # open up the file and get the fd
#            try:
#                self.fd = open(self.path+os.sep+self.fileName, "wb")
#                conn.send("OK\n\r")
#                self.fileUploadRunning = True
#                retc = 0
#                self.bytesTotal = 0
#                self.bytesWritten = 0
#            except Exception, e:
#                self.log.error("%s - exception=%s\n\ttype=%s\n\tinstance=%s" %
#                               (mod, Exception, type(e), e.args))
#                self.log.error("%s can't import %s terminating" % (mod, self.op.testPyWrapper))
#                conn.send("ERROR can't open %s\n\r" %(self.path+os.sep+self.fileName))
#                retc = -4
#                    
#            
#            # read unitl all data recived.
#            
#                
#        self.log.debug("[%s}-->return: rtnMsg=%d." % (mod, retc))
#        return retc
#    
#    # ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#    # @brief stopHandler - terminates the monitor program, only!
#    #
#    def stopHandler(self, conn, cmd):
#        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)
#        self.log.debug("[%s]<--Called." % (mod))
#        retc = 0
#        
#        self.log.debug("[%s}-->return." % (mod))
#        return retc
#
#    # ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#    # @brief lengthHandler - returns the results from the streams.
#    #
#    def lengthHandler(self, conn, cmd):
#        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)
#        self.log.debug("[%s]<--Called." % (mod))
#        retc = 0
#        self.fileLen = int(cmd.split(' ')[1].strip())
#
#        self.log.debug("[%s}-->return: length=%s" % (mod, self.fileLen))
#        return retc
#
#    # ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#    # @brief pathHandler - returns the results from the streams.
#    #
#    def pathHandler(self, conn, cmd):
#        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)
#        self.log.debug("[%s]<--Called." % (mod))
#        retc = 0
#        self.path = cmd.split(' ')[1].strip()
#
#        # check it out 
#        if (os.path.exists(self.path)):
#            conn.send("OK\n\r")
#        else:
#            conn.send("ERROR: path=%s does not exist.\n\r" % (self.path))
#            
#        self.log.debug("[%s}-->return: path=%s" % (mod, self.path))
#        return retc
#    
#    # ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#    # @brief statHandler - returns the state of the running process.
#    #
#    def nameHandler(self, conn, cmd):
#        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)
#        self.log.debug("[%s]<--Called." % (mod))
#        retc = 0
#        self.fileName = cmd.split(' ')[1].strip()
#        conn.send("OK\n\r")
#        self.log.debug("[%s}-->return:name=%s" % (mod, self.fileName))
#        return retc
#    
#    # ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#    # @brief quitHandler - shuts down this service.
#    #
#    def quitHandler(self, conn, cmd):
#        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)
#        self.log.debug("[%s]<--Called." % (mod))
#        retc = 0
#
#            
#        conn.send("TERMINATE\n\r")
#        self.log.debug("[%s}-->return." % (mod))
#        return retc

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

