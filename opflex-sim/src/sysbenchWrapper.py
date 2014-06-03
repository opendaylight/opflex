#!/usr/bin/env python
################################################################################
#
# Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v1.0 which accompanies this distribution,
# and is available at http://www.eclipse.org/legal/epl-v10.html
#
#
# @File sysbenchWrapper.py
#
# @Author   don@noironetworks.com - MySQL, Inc.
# @date 15-APR-2008
# @version: 1.0
#
# @brief The is a generic test driver. Is command driven.
# 
# @example:
#
# Revision History:  (latest changes at top)
#    don@noironetworks.com - 15-APR-2008 - created
#
###############################################################################

__author__ = 'dkehn@noironetworks.com'
__version__ = '1.0'
__date__ = '05-APR-2008'


import pdb

import threading, sys, time, os, re, popen2, signal
import commands, getopt
import logging
import fcntl
import telnetlib

#from ThreadPool import *
#from Configuration import *

PRGM_NAME = "pyTestDriver.py"
VERSION = '1.0'
INIFILE = "./config/testDriver.ini"

# ****************************************************************************
# Ensure booleans exist.  If not, "create" them
# Workaround for python <= 2.2.1
#
try:
    True

except NameError:
    False = 0
    True = not False

class sysbenchWrapper:
    DB_LEVEL = logging.INFO
    RMT_PROMPT = "rmt_dstat> "
    RMT_TIMEOUT = 2
    BENCH_PRGM = "/u02/devl/bench/sysbench-0.4.8/sysbench/sysbench"

    numThreads=4
    engine="ndbcluster"
    host="localhost"
    user="root"
    password=""
                 
    def __init__(self, basedir, runpath, eng, dbc):
        self.log = logging.getLogger("%s" % (self.__class__.__name__))
        self.log.setLevel(self.DB_LEVEL)
        self.currentDebugLevel = self.DB_LEVEL
        mod = sys._getframe().f_code.co_name
        self.log.debug("%s<-- Called." % (mod))
        self.testProgramClass = None
        self.testProgramInstance = None
        self.baseDir = basedir
        self.runPath = runpath
        self.engine = eng
        self.host = None
        self.user = None
        self.port = 3306
        self.password = None
        self.database = None
        self.socket = None
        self.sleepTimeSeconds = 10
        
        # parse the the db connect string
        try:
            i = 0
            parts = dbc.split(':')
            self.host = parts[i]
            i += 1
            self.database = parts[i]
            i += 1
            self.user = parts[i]
            i += 1
            self.password = parts[i]
            i += 1
            self.port = int(parts[i])
            i += 1
            self.socket = parts[i]
        except Exception, e:
            self.log.fatal("%s - exception=%s\n\ttype=%s\n\tinstance=%s" %
                           (mod, Exception, type(e), e.args))
            self.log.fatal("%s Bad database connect string %s" % (mod, dbc))
            sys.exit(-1)        
        
        
                
        self.testParams = {'point_selects':0,
                           'simple_ranges':0,
                           'sum_ranges':0,
                           'order_ranges':0,
                           'distinct_ranges':0,
                           'index_updates':0,
                           'non_index_updates':0,
                           }
        self.test_runs = {'point_selects-client':   (self.run, 
                                                      'point_selects', 
                                                      {'point_selects':1}),
                          'point_selects-server':    (self.run, 
                                                      'point_selects', 
                                                      {'point_selects':1}),
                          'simple_ranges-client':    (self.run, 
                                                      'simple_ranges', 
                                                      {'simple_ranges':1}),
                          'simple_ranges-server':    (self.run, 
                                                      'simple_ranges', 
                                                      {'simple_ranges':1}),
                          'sum_ranges-client':       (self.run, 
                                                      'sum_ranges', 
                                                      {'sum_ranges':1}),
                          'sum_ranges-server':       (self.run, 
                                                      'sum_ranges', 
                                                      {'sum_ranges':1}),
                          'order_ranges-client':     (self.run, 
                                                      'order_ranges', 
                                                      {'order_ranges':1}),
                          'order_ranges-server':     (self.run, 
                                                      'order_ranges', 
                                                      {'order_ranges':1}),
                          'distinct_ranges-client':  (self.run, 
                                                      'distinct_ranges', 
                                                      {'distinct_ranges':1}),
                          'distinct_ranges-server':  (self.run, 
                                                      'distinct_ranges', 
                                                      {'distinct_ranges':1}),
                          'index_updates-client':    (self.run, 
                                                      'index_updates', 
                                                      {'index_updates':1}),
                          'index_updates-server':    (self.run, 
                                                      'index_updates', 
                                                      {'index_updates':1}),
                          'non_index_updates-client':(self.run, 
                                                      'non_index_ranges', 
                                                      {'non_index_updates':1}),
                          'non_index_updates-server':(self.run, 
                                                      'non_index_ranges', 
                                                      {'non_index_updates':1}),
                          'complex-client':          (self.run, 
                                                      'complex', 
                                                      {'point_selects':10,
                                                       'simple_ranges':1,
                                                       'sum_ranges':1,
                                                       'order_ranges':1,
                                                       'distinct_ranges':1,
                                                       'index_updates':1,
                                                       'non_index_updates':1}),
                          'complex-server':          (self.run, 
                                                      'complex', 
                                                      {'point_selects':10,
                                                       'simple_ranges':1,
                                                       'sum_ranges':1,
                                                       'order_ranges':1,
                                                       'distinct_ranges':1,
                                                       'index_updates':1,
                                                       'non_index_updates':1})
                          }

        # this is a link list of tests that will be running both as a client and server
        # (client with no prepare, server all statements are prepared 1st). The poosile
        # tests are:
        # Depending on the command line options, each transaction may contain the following statements:
        #    * Point queries: (point_selects)
        #      SELECT c FROM sbtest WHERE id=N
        #    * Range queries: (simple_ranges)
        #      SELECT c FROM sbtest WHERE id BETWEEN N AND M 
        #    * Range SUM() queries: (sum_ranges)
        #      SELECT SUM(K) FROM sbtest WHERE id BETWEEN N and M
        #    * Range ORDER BY queries: (order_ranges)
        #      SELECT c FROM sbtest WHERE id between N and M ORDER BY c
        #    * Range DISTINCT queries: (distinct_ranges)
        #      SELECT DISTINCT c FROM sbtest WHERE id BETWEEN N and M ORDER BY c
        #    * UPDATEs on index column: (index_updates)
        #      UPDATE sbtest SET k=k+1 WHERE id=N 
        #    * UPDATEs on non-index column: (non_index_updates)
        #      UPDATE sbtest SET c=N WHERE id=M 
        #    * DELETE queries:  (complex)
        #      DELETE FROM sbtest WHERE id=N 
        #    * INSERT queries:  (complex)
        #      INSERT INTO sbtest VALUES (...
        self.testList=['point_selects-client', 'point_selects-server',
                       'simple_ranges-client','simple_ranges-server',
                       'sum_ranges-client','sum_ranges-server',
                       'order_ranges-client','order_ranges-server',
                       'distinct_ranges-client','distinct_ranges-server',
                       'index_updates-client','index_updates-server',
                       'non_index_updates-client','non_index_updates-server',
                       'complex-client','complex-server']

        # Maximum allowable time for the benchmark to run in minutes, beyond 
        # this the benchmark will be terminated
        self.maxRunTimeOut=25

        # the sleep time between check fo the compltion of the benchmark process
        self.sleepTimeSeconds=10

        self.bmTimeout = self.maxRunTimeOut
        self.log.debug("%s-->return" % (mod))
        
    # ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    # @brief getTestList()
    #
    def getTestList(self):
        mod = sys._getframe().f_code.co_name
        self.log.debug("%s<-- Called." % (mod))
        self.log.debug("%s-->return:%r" % (mod, self.testList))
        
        return self.testList
    

    # ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    # @brief seroTestParams() - initializes the arguments used for the commad
    #        line.
    #
    def zeroTestParams(self):
        mod = sys._getframe().f_code.co_name
        self.log.debug("%s<-- Called." % (mod))

        self.testParams['point_selects-client']             = 0
        self.testParams['point_selects-server']             = 0
        self.testParams['simple_ranges-client']             = 0
        self.testParams['simple_ranges-server']             = 0
        self.testParams['sum_ranges-client']                = 0
        self.testParams['sum_ranges-server']                = 0
        self.testParams['order_ranges-client']              = 0
        self.testParams['order_ranges-server']              = 0
        self.testParams['distinct_ranges-client']           = 0
        self.testParams['distinct_ranges-server']           = 0
        self.testParams['index_updates-client']             = 0
        self.testParams['index_updates-server']             = 0
        self.testParams['non_index_updates-client']         = 0
        self.testParams['non_index_updates-server']         = 0
        self.log.debug("%s-->return:%r" % (mod, self.testParams))


    # ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    # @brief genricRunTests - run the tests
    #
    def buildCmdLine(self, valhash):
        mod = sys._getframe().f_code.co_name
        self.log.debug("%s<-- Called:%r" % (mod, valhash))
        
        self.zeroTestParams()

        for k,v in valhash.iteritems():
            self.testParams[k] = v
                    

        sbargs="--num-threads=%s" \
                " --max-requests=0" \
                " --max-time=60" \
                " --test=oltp" \
                " --oltp-table-size=100000 " \
                " --oltp-point-selects=%s" \
                " --oltp-simple-ranges=%s" \
                " --oltp-sum-ranges=%s" \
                " --oltp-order-ranges=%s" \
                " --oltp-distinct-ranges=%s" \
                " --oltp-index-updates=%s" \
                " --oltp-non-index-updates=%s" \
                " --mysql-table-engine=%s" \
                " --mysql-host=%s" \
                " --mysql-user=%s" \
                " --mysql-password=%s" \
                " --mysql-db=%s" \
                " --mysql-socket=%s" % (self.numThreads,
                                        self.testParams['point_selects'],
                                        self.testParams['simple_ranges'],
                                        self.testParams['sum_ranges'],
                                        self.testParams['order_ranges'],
                                        self.testParams['distinct_ranges'],
                                        self.testParams['index_updates'],
                                        self.testParams['non_index_updates'],
                                        self.engine,
                                        self.host,
                                        self.user,
                                        self.password,
                                        self.database,
                                        self.socket)

        self.log.debug("%s-->return:%s" % (mod, sbargs))
        return sbargs
    
    # ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    # @brief genricRunTests - run the tests
    #
    def run(self, str='generic', path=None):
        self.log = logging.getLogger("%s" % (self.__class__.__name__))
        self.log.setLevel(self.currentDebugLevel)
        mod = sys._getframe().f_code.co_name
        self.log.debug("%s<-- Called:str=%s:path=%s" % (mod, str,path))
        bm = None

        # 
        # start local monitoring
        #
        cmdLine = self.buildCmdLine(self.test_runs[str][2]) 

                    
        # start the bench mark program running
        # 1st - prepare
        # 2nd - run
        # 3rd - cleanup
        self.log.debug("%s - +++++++++ Running Prepare for %s" %(mod, str))
        cmd = "%s %s prepare" % (self.BENCH_PRGM, cmdLine)
        bm = self.startProcess(cmd)
        retc = self.waitForProcessCompletionWithTimer(bm, 2)
        if ( retc ): # if we have run beyound alotted time kill it!
            os.kill(bm.pid, signal.SIGKILL)
        
        self.log.debug("%s - +++++++++ Running Run for %s" %(mod, str))
        cmd = "%s %s run" % (self.BENCH_PRGM, cmdLine)
        self.log.debug("%s - executing %s" % (mod, cmd))
        bm = self.startProcess(cmd)
        retc = self.waitForProcessCompletionWithTimer(bm, self.bmTimeout)        
        if ( retc ): # if we have run beyond alotted time kill it!
            os.kill(bm.pid, signal.SIGKILL)

        # write the outputs to the various files
        # -- write the desc.txt ----------
        dfd = file(path+os.sep+"desc.txt", "w")
        dfd.write(cmd+" comment: "+str+"\n")
        dfd.close()
            
        # -- write the benchmark data out
        bfd = file(path+os.sep+"sysbench.out", "w")        
        self.saveOutputData(bm.fromchild, bfd)
        bm.fromchild.close()
        bfd.close()

        self.log.debug("%s - +++++++++ Running Cleanup for %s)" %(mod, str))
        cmd = "%s %s cleanup" % (self.BENCH_PRGM, cmdLine)
        bm = self.startProcess(cmd)
        retc = self.waitForProcessCompletionWithTimer(bm, 5)        
        if ( retc ): # if we have run beyound alotted time kill it!
            os.kill(bm.pid, signal.SIGKILL)

        
        self.log.debug("%s-->return" % (mod))


    # ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    # @brief readUtilPrompt() - read from the telnet stream until the prompt appears
    #        and returns the data read without the prompt. Relies on the
    #        RMT_PROMPT and RMT_TIMEOUT
    #
    def getProgramName(self, s): 
        mod = sys._getframe().f_code.co_name
        self.log.debug("%s<-- Called: %s" % (mod, s))
        
        rtnname = None

        pname = os.path.basename(s.split()[0])
        if (pname.find('.') > 0):
            rtnname = pname[:pname.find('.')]
        else:
            rtnname = pname
            
        self.log.debug("%s-->return:%s" % (mod, rtnname))
        return rtnname

    # ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    # @brief readUtilPrompt() - read from the telnet stream until the prompt appears
    #        and returns the data read without the prompt. Relies on the
    #        RMT_PROMPT and RMT_TIMEOUT
    #
    def readUntilPrompt(self, c): 
        mod = sys._getframe().f_code.co_name
        self.log.debug("%s<-- Called" % (mod))
        
        rtnstr = None
        s = c.read_until(self.RMT_PROMPT, self.RMT_TIMEOUT)
        
        if (s):
            rtnstr = s[:s.find(self.RMT_PROMPT)].strip()
        self.log.debug("%s-->return:%s" % (mod, rtnstr))
        return rtnstr
        
    # ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    # @brief saveOutputData() - copy one fd to another. Simple yea?
    #
    def saveOutputData(self, ifd, ofd):
        mod = sys._getframe().f_code.co_name
        self.log.debug("%s<-- Called" % (mod))

        lcnt = 0
        for s in ifd.readlines():
            lcnt += lcnt
            ofd.write(s)
        
        self.log.debug("%s-->return:lines=%d" % (mod, lcnt))
        return lcnt

    # ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    # @brief waitForCompletionWithTimer() - waits for a child top complete, with
    #        the exception that it doesn't exceed the timeout in minutes.
    #
    def waitForProcessCompletionWithTimer(self, p=None, t=10):
        mod = sys._getframe().f_code.co_name
        self.log.debug("%s<-- Called" % (mod))
        start = time.gmtime(time.time())[4:6]
        retc = 0
        sleepTime = 5
        try:
            sleepTime = int(t)
        except:
            pass
            
        while (p.poll() == -1):
            # check to see if we have exhausted our time limit
            #self.log.debug("%s - return from poll = %d" % (mod, p.poll()))
            new_time=time.gmtime(time.time())[4:6]
            if (new_time[0]-start[0])>=sleepTime and new_time[1]>=start[1]:
                break

            time.sleep(sleepTime)
            
        if (p.poll() == -1):
            retc = -1
        else:
            retc = 0
            
        self.log.debug("%s-->return:%d" % (mod, retc))
        return retc

    # ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    # @brief makeNonBlocking() - makes the stream nonblocking and thus avoid
    #        deadlocks!
    #
    def makeNonBlocking(self, fd):
        mod = sys._getframe().f_code.co_name
        self.log.debug("%s<-- Called" % (mod))

        fl = fcntl.fcntl(fd, FCNTL.F_GETFL)
        try:
            fcntl.fcntl(fd, FCNTL.F_SETFL, fl | FCNTL.O_NDELAY)
        except:
            fcntl.fcntl(fd, FCNTL.F_SETFL, fl | FCNTL.FNDELAY)
            
        self.log.debug("%s-->return" % (mod))
        

    # ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    # @brief startProcess() - starts a process and returns the child.
    #
    def startProcess(self, cmd, closein=True):
        mod = sys._getframe().f_code.co_name
        self.log.debug("%s<-- Called:CMD->%s" % (mod, cmd))
        child = popen2.Popen3(cmd, True)
        if (closein):
            child.tochild.close()
            
        if (child.poll() != -1):
            self.log.fatal("%s - %s is not running: " % (mod, cmd))
            for l in child.fromchild.readlines():
                self.log.error("%s - \t%s: " % (mod, l))

            for l in child.childerr.readlines():
                self.log.fatal("%s - \t%s: " % (mod, l))
            early_exit(-1)
                
            
        self.log.debug("%s-->return" % (mod))
        return child


    # ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    # @brief getRunTypeNum - this will check in the path for the file .runtype
    #        if it exist it will read it and return the directory for which
    #        the run will need, else it will create the file .runtype and
    #        initialize it withe the number 2 - one based.
    def getRunTypePath(self, path, runtype):
        mod = sys._getframe().f_code.co_name
        self.log.debug("%s<-- Called:path=%s, runtype=%s" % (mod, path, runtype))
        rtnpath = None
##         rfile = path+os.sep+'.'+runtype
##         num = 0
##         fd = None

##         print rfile
##         if (os.path.exists(rfile) == True):  # doesn't exist so create it
##             fd = file(rfile,"r")
##             numstr = fd.readline()
##             fd.close()
##             if (numstr):
##                 num = int(numstr)
            
##         num = num + 1

##         fd = file(rfile,"w")
##         fd.writelines("%d" % (num))
##         fd.close()

        rtnpath = "%s-%d" % (runtype, self.runNumber)
        
        self.log.debug("%s-->return:%s" % (mod, rtnpath))

        return rtnpath
        
        


# ******************************************************************************
# @brief early_exit() - terminate the program
#
def early_exit(ret):
    #signal.signal(signal.SIGALRM, signal.SIG_IGN)
    sys.exit(ret)

