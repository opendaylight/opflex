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
#
# @File pyTestDriverclient.py
#
# @Author   don@noironetworks.com
# @date 15-APR-2008
# @version: 1.0
#
# @brief The is a generic test driver. Is command driven.
# 
# @example:
#
# Revision History:  (latest changes at top)
#    don@noironetworks.com - 05-APR-2008 - created
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
#from dbPool import import *

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

# *****************************************************************************
# @brief pyBenMPlot main class
#
class testDriver:
    DB_LEVEL = logging.INFO
    RMT_PROMPT = "rmt_dstat> "
    RMT_TIMEOUT = 2

    numThreads=4
    engine="ndbcluster"
    host="localhost"
    user="root"
    password=""
                 
    def __init__(self, opth):
        self.log = logging.getLogger("%s" % (self.__class__.__name__))
        self.op = opth
        self.log.setLevel(self.DB_LEVEL)
        self.currentDebugLevel = self.DB_LEVEL
        if (self.op.verbose == True):
            self.log.setLevel(logging.DEBUG)
            self.currentDebugLevel = logging.DEBUG
        mod = sys._getframe().f_code.co_name
        self.log.debug("%s<-- Called." % (mod))
        self.testPyWrapperClass = None
        self.testPyWrapperInstance = None
        self.scriptFile = False

        if (self.op.testPyWrapper):
            if (self.isPythonFile(self.testPyWrapper)):
                try:
                    print self.op.testPyWrapper
                    p = self.op.testPyWrapper.split()[0]
                    self.testPyWrapperClass = __import__(p)
                    self.scriptFile = False
                
                except Exception, e:
                    self.log.fatal("%s - exception=%s\n\ttype=%s\n\tinstance=%s" %
                                   (mod, Exception, type(e), e.args))
                    self.log.fatal("%s can't import %s terminating" % (mod, self.op.testProgram))
                    sys.exit(-1)
            else:
                self.scriptFile = True
                
#        self.bmTimeout = int(self.conf.testDriver.maxRunTimeOut)
        self.log.debug("%s-->return" % (mod))
        
 
    # ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    # @brief genricRunTests - run the tests
    #
    def genericRunTests(self, str='generic',  path=None):
        self.log = logging.getLogger("%s" % (self.__class__.__name__))
        self.log.setLevel(self.currentDebugLevel)
        mod = sys._getframe().f_code.co_name
        self.log.debug("%s<-- Called:str=%s:path=%s" % (mod, str,path))
        doRemote = False
        remoteTelnetClients = {}
        rmtMonPrgm = None
        nodeList = []
        tc = None
        bm = None

        # 
        # start local monitoring
        #
        if (self.op.localMonitorProgram):
            mname = os.path.basename(self.op.localMonitorProgram).split('.')[0]
            cmd = self.op.localMonitorProgram
            doLocal = True
            self.log.debug("%s - executing %s" % (mod, cmd))
            # create a sub-process and capture the output.
            m_child = self.startProcess(cmd)

        #
        # start the remote monitoring
        #
        if (self.op.nodeList and self.op.monCommandLine):
            nodeList = self.op.nodeList.split(',')
            doRemote = True
            self.log.debug("%s - executing remotely %s@%s" % (mod, rmtMonPrgm, self.op.nodeList))


        # start up the monitor(s)
        if (doRemote):

            for n in nodeList:
                h, p = n.split(':')
                tc = None
                try:
                    tc = telnetlib.Telnet(h,int(p))
                except Exception, e:
                    self.log.error("%s - exception=%s\n\ttype=%s\n\tinstance=%s" %
                                   (mod, Exception, type(e), e.args))
                    
                if (tc):
                    self.log.debug("%s - remote execution of %s on %s:%s" % (mod, self.op.monCommandLine, h, p))
                    self.readUntilPrompt(tc)
                    tc.write("run "+self.op.monCommandLine+"\n")
                    s = self.readUntilPrompt(tc)
                    tc.write('stat\n')
                    s = self.readUntilPrompt(tc)

                    # check to make sure its running
                    if (s):
                        if (s.find('not running') != -1) :
                            self.log.error("%s Error in starting on remote node %s status indicates its not running!" %
                                             (mod, n))
                            tc.write('quit\n')
                        else:
                            remoteTelnetClients[tc] = n                    
                    else:
                        self.log.error("%s Error - Null return when checking the stat from %s!" %
                                         (mod, n))
                        # - this might be a bad idea but we will assume it is running????
                        remoteTelnetClients[tc] = n                    
                        
                else:
                    self.log.error("%s Error in starting on remote node %s" % (mode, n))
                    
        # check for the stop monitor program, we either do this or the test.
        if (self.op.stopMonitorProgram):
            # we will loop until the stop file appears!
            while (True):
                if (os.path.exists(self.op.stopMonitorProgram) == False):
                    time.sleep(1)
                else:
                    # delete the file and terminate the loop
                    os.remove("self.op.stopMonitorProgram")
                    break                    

        # -- if there is an indication to run a test program this is where we do it.
        elif (self.op.testPyWrapper):
            if (self.scriptFile):
                # if its just a cm,d run it!
                # note: we wait here until it completes!
                os.system(self.op.testPyWrapper)
            else:
                # if this is a python script we instantiate it and execute.
                if (self.testPyWrapperInstance):
                    self.testPyWrapperInstance.run(str, path)

        if (doRemote):
            if (len(remoteTelnetClients) > 0):
                for tc, val in remoteTelnetClients.iteritems():
                    tc.write('stop\n')
                    s = self.readUntilPrompt(tc)
                    tc.write("quit\n");
                    tc.close()
                    # figure our the name for the remote output                    
                    remoteName = self.getProgramName(self.op.monCommandLine)
                    rmfd = file(path+os.sep+remoteName+'-'+val.split(':')[0]+".out", "w")
                    rmfd.write(s)
                    rmfd.close()                    

        if (self.op.localMonitorProgram):        
            # cleanup from the run, kill the local monitors
            os.kill(m_child.pid, signal.SIGKILL)

            localOutputName = os.path.basename(self.op.localMonitorProgram.split()[0].split('.')[0])
            mfd = file(path+os.sep+localOutputName+".out", "w")
            self.saveOutputData(m_child.fromchild, mfd)
            mfd.close()

        
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
        
        self.log.debug("%s - remote sent:%s" % (mod, s))
        
        if (s):
            rtnstr = s[:s.find(self.RMT_PROMPT)].strip()
        self.log.debug("%s-->return:%s" % (mod, rtnstr))
        return rtnstr
    
    # ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    # @brief isPythonFile() - determines if the specified file is a 
    # python script.
    #
    def isPythonFile(self, file):
        mod = sys._getframe().f_code.co_name
        self.log.debug("%s<-- Called:file=%s" % (mod, file))
        retB = False

        fname = os.path.basename(file.split()[0])
        if (fname.find('.py') > -1):
            retB = True

        self.log.debug("%s-->return:retB=%r" % (mod, retB))
        return retB

    
        
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
        start = time.time()
        retc = 0
        sleepTime = 10
        try:
            ststr = self.conf.testDriver.sleepTimeSeconds
            sleepTime = int(ststr)
        except:
            sleepTime = 10
            
        while (p.poll() == -1):
            # check to see if we have exhausted our time limit
            if (time.gmtime(time.time() - start)[4] >= t):
                break

            sleep(sleepTime)
            
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

    # ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    # @brief main portion of the processer. This is responsible for kicking
    #     off each test
    #
    def main(self):
        mod = sys._getframe().f_code.co_name
        self.log.debug("%s<-- Called." % (mod))

        #
        # make sure that the base directory is there, in addition
        # ensure that the run name (dir) is created.
        #
        basedir = self.op.baseDir
        self.log.debug("%s - testing for %s." % (mod, basedir))
        
        if (os.path.exists(basedir) == False):  #create it
            self.log.debug("%s - %s created." % (mod, basedir))
            os.makedirs(basedir)
        else:
            self.log.debug("%s - %s exists?." % (mod, basedir))

        if (os.path.isdir(basedir) == False):
            self._log.fatal("%s file item %s exists and is not a directory!" % (mod, basedir))
            early_exit(-1)

        rundir = self.op.runName
        runpath = basedir+os.sep+rundir.replace('"','')
        if (os.path.exists(runpath) == False):
            self.log.debug(":%s - creating %s" %(mod, runpath))
            os.makedirs(runpath)

        #testList = self.conf.testDriver.tests.replace('"','').split(',')
        if (self.testPyWrapperClass):
            try:
                c = self.testPyWrapperClass.__dict__[self.op.testPyWrapper]
                self.testPyWrapperInstance = c(basedir, 
                                               runpath, 
                                               self.op.engine, 
                                               self.op.dbconnectString)                
            except Exception, e:
                    self.log.fatal("%s - exception=%s\n\ttype=%s\n\tinstance=%s" %
                                   (mod, Exception, type(e), e.args))
                    self.log.fatal("%s can't import %s terminating" % (mod, self.op.testPyWrapper))
                    sys.exit(-1)
                    
            

        # -- get the global run number
        self.runNumber = self.getRunNumber(runpath)

        # -- let set if we have a test program if so, set it up.
        if (self.scriptFile):
            testList = self.testPyWrapperInstance.getTestList()
        else:
            testList= [self.op.testRunName] 
        
        # -- execute the monitors
        for t in testList:
            run_type_dir = self.getRunTypePath(runpath, t)
            self.log.info("%s - executing %s test." % (mod, t))
            self.log.debug("%s - running the %s test." % (mod, run_type_dir))
            this_run_dir = runpath+os.sep+run_type_dir
            try:
                os.makedirs(this_run_dir)
            except OSError, e:
                self.log.info("%s - %s already exists" % (mod, this_run_dir))
#                
            self.genericRunTests(t, this_run_dir)
            
        ## TODO add the parsers here - run the parsers after the testing is complete.
            
        self.log.debug("%s-->return" % (mod))

    # ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    # @brief getRunNumber - this will check in the path for the file .run_number
    #        if it exist it will read it and return that number back,
    #        else it will create the file .run_number and
    #        initialize it withe the number 2 - one based.
    #
    def getRunNumber(self, path):
        mod = sys._getframe().f_code.co_name
        self.log.debug("%s<-- Called:path=%s" % (mod, path))
        rfile = path+os.sep+'.run_number'
        num = 0
        fd = None

        if (os.path.exists(rfile) == True):  # doesn't exist so create it
            fd = file(rfile,"r")
            numstr = fd.readline()
            fd.close()
            if (numstr):
                num = int(numstr)
            
        num = num + 1

        fd = file(rfile,"w")
        fd.writelines("%d" % (num))
        fd.close()
        self.log.debug("%s-->return:%d" % (mod, num))
        return num
        

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
        
        

# *****************************************************************************
# @brief Options - handler for the options
#
class Options:
    DB_LEVEL = logging.DEBUG
    configFile = None
    
    def __init__(self, args):
        import os
        self.log = logging.getLogger("%s" % (self.__class__.__name__))
        self.log.setLevel(self.DB_LEVEL)
        mod = sys._getframe().f_code.co_name
        self.log.debug("%s<-- Called.(%s)" % (mod, args))
        
        self.args = args
        
        # command line arguments 
        self.baseDir = None
        self.debug = 0
        self.runName = None
        self.verbose = False
        helpFlag = False
        versionFlag = False
        self.monCommandLine = None
        self.nodeList = None
        self.updateInterval = 1
        self.dbconnectString = "localhost:e1:root::3306:/tmp/mysql.sock"
        self.engine = 'NDBCLUSTER'
        self.parser = None
        self.parserArgs = None
        self.testPyWrapper = None
        self.stopMonitorProgram = None
        self.testRunName = None

        try:
            import getopt
            opts, args = getopt.getopt(sys.argv[1:], "dhvc:r:b:e:p:n:m:l:t:c:s:u:",
                                       ["debug","help","basedir="])
        except getopt.GetoptError:
            # print help information and exit:
            self.log.error("%s - Invalid parameters: %s" % (mod, sys.argv))
            self.usage()
            early_exit(1)
            
        for opt, arg in opts:    
            #print ("opt=%s" % (opt))
            if opt in ("-d", "--debug", "--verbose"):
                self.verbose = True
                self.log.setLevel(logging.DEBUG)
                self.log.debug("%s - verbose on" % (mod))
                
            elif opt in ['-h', '--help']:
                self.help()
                helpFlag = True
                
            elif opt in ['-v', '--version']:
                self.version()
                versionFlag = True

            elif opt in ['-s', '--stopMonitorFile']:
                if (os.path.exists(arg)):
                    os.remove(arg)                
                self.stopMonitorFileName = arg

            elif opt in ['-b', '--basedir']:
                self.baseDir = arg

            elif opt in ['-m', '--mon']:
                self.monCommandLine = arg

            elif opt in ['-l', '--localMonitoring']:
                self.localMonitorProgram = arg

            elif opt in ['-n', '--nodeList']:
                self.nodeList = arg

            elif opt in ['-c', '--dbconnect']:
                self.dbconnectString = arg
                
            elif opt in ['-e', '--engine']:
                self.engine = arg

            elif opt in ['-t', '--testPythonWrapper']:
                # if this in not a python wrapper then treat is as a normal executable                
                self.testPyWrapper = arg             ## Note this will include the args if any
                
            elif opt in ['-u', '--TestRunName']:
                self.testRunName = arg                

            elif opt in ['-p', '--parser']:
                self.parser = arg.split()[0]
                self.parserArgs = arg[arg.find(' '):]

            elif opt in ['-r', '--run']:
                # 07-MAR-2008 dkehn@noironetworks.com if 'use_date' is entered the date will be used
                #                             for the rundir.
                if (arg == 'use_date' or arg == 'USE-DATE'):
                    fmt = '%Y-%m-%d %H:%M:%S %Z'
                    self.runName = time.strftime(fmt).replace(' ','_')
                else:
                    self.runName = arg
              
#            elif opt in ['-c', '--config']:
#                self.log.debug("%s - checking for %s" % (mod, arg))
#                if ((os.path.isfile(arg)) and (arg)):
#                    self.configFile = arg
#                    self.log.debug("%s - found %s" % (mod, arg))
#                  
#                else:
#                    self.log.fatal("%s - Can't find the configiuration file!" % (mod, self.basedir))
#                    early_exit(-1)
                  
            else:
                self.log.fatal("%s - (%s %s) Can't find %s" % (mod, o, arg, arg))
                self.usage()
                early_exit(1)

        if (versionFlag or helpFlag):
            early_exit(1)
        else:
            # -- make sure we the minimum number of arguments
#            if (self.configFile == None):
#                self.log.fatal("%s - you MUST specify a configuration file!" % (mod))
#                self.usage()
#                early_exit(1)

            if (self.runName == None):
                self.log.fatal("%s - you MUST specify a run name!" % (mod))
                self.usage()
                early_exit(1)
                
            if (self.monCommandLine == None):
                self.log.fatal("%s - you MUST specify a monitor program to run!" % (mod))
                self.usage()
                early_exit(1)

            if (self.nodeList == None):
                self.log.fatal("%s - you MUST specify a remote servers on which the monitor program will run!" % (mod))
                self.usage()
                early_exit(1)

        self.log.debug("%s-->return" % (mod))
                

            
    def dump(self):
        print ("--- Dump options ---------------------------------------")
        print ("args                = %s" % (self.args))
        print ("config file         = %s" % (self.configFile))
        print ("output dir          = %s" % (self.outDir))
        print ("debug               = %s" % (self.debug))
        
    def version(self):
        print '%s %s' % (PRGM_NAME, VERSION)
        print 'Written by Don Kehn <don@noironetworks.com>'
        print
        print 'Platform %s/%s' % (os.name, sys.platform)
        print 'Kernel %s' % os.uname()[2]
        print 'Python %s' % sys.version
        print
        
        global op
        op = self

    def usage(self):
        print 'Usage: %s -r "program_name options" -b basedir -r runname [other options..] ' % (PRGM_NAME)

    def help(self):
        print '''Tool for collecting the sysbench output data and ploting the results using matplotlib

pyBenMPlot options:
  -d, --debug            enable debugging and logging.
  -v, --version          version information.
  -h, --help             help.
  -r, --run              the run name, this will be the name of the directory, if use_date is
                         entered will create run name based upon the current date-time.
  -b  --baseDir          The Base directory for where the output data will be placed
  -l  --localMonitoring  The local monitoring program
  -m  --mon "cmd-line"   The commandline string that will be used to send to the node for monitoring
  -n  --nodelist host:port,.... The nodes that the CommandServer is running on
  -c  --dbconnection     The connection information to a database host:dbname:user:password:port:socket_file
  -e  --engine           The engine under test i.e. MYISAM, NDBCLUSTER, INNODB
  -p  --parser           The python parser that will be envoked
  -s  --stopMonitorFile  This is the file that is monitored for stopping  
  -t  --testPythonWrapper The python wrapper for the test/benchmark program. This is a python program that is a wrapper for the real test. 
  -u  --testRunName      In the case where there is NOT a python script to run the tests this is the name of the run 

'''



# ******************************************************************************
# @brief early_exit() - terminate the program
#
def early_exit(ret):
    #signal.signal(signal.SIGALRM, signal.SIG_IGN)
    sys.exit(ret)

# ******************************************************************************
# @brief listmodules()
#
def listmodules():
    import glob
    remod = re.compile('.+/pyBenMPlot_(.+).py$')
    for path in sys.path:
        list = []
        for file in glob.glob(path + os.sep + 'pyBenMPlot_*.py'):
            list.append(remod.match(file).groups()[0])
        if not list: continue
        list.sort()
        cols2 = cols - 8
        print '%s:\n\t' % os.path.abspath(path),
        for mod in list:
            cols2 = cols2 - len(mod) - 2
            if cols2 <= 0:
                print '\n\t',
                cols2 = cols - len(mod) - 10
            print mod + ',',
        print
                    
# ******************************************************************************
# main()
#
def main():
    logging.basicConfig()

    print 'Starting -->%s %s' % (PRGM_NAME, VERSION)
    try:
        op = Options(sys.argv[1:])
    except KeyboardInterrupt, e:
        print

    obj = testDriver(op)
    obj.main()
    print 'Complete'

    
        
# ****************************************************************************
# The kicker to get us going.
#

if __name__ == '__main__': main()
