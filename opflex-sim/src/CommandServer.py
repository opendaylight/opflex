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
# @File CommandServer.py
#
# @Author   dkehn@noironetworks.com 
# @date 02-APR-06
# @version: 1.0
#
# @brief Communications command server module.
# 
# @example:
#
# Revision History:  (latest changes at top)
#    dkehn@scion-eng.com - 02-APR-06 - created
#
#

__author__ = 'dkehn@noironetworks.com'
__version__ = '1.0'
__date__ = '02-APR-06'


# ****************************************************************************
# Ensure booleans exist.  If not, "create" them
#
try:
    True

except NameError:
    False = 0
    True = not False

import sys
import time
import os
import re
import commands, getopt
import logging
import threading, thread
from threading import Thread
import socket
import traceback
import types

# Default daemon parameters.
# File mode creation mask of the daemon.
UMASK = 0

# Default working directory for the daemon.
WORKDIR = "/"

# Default maximum for the number of available file descriptors.
MAXFD = 1024

class CommandServer:

    services = {}     # services
    cm = None
    tPool = None
    DB_LEVEL = logging.INFO
    
    def __init__(self, log, maxconnection):        
        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)
        self.log = logging.getLogger("%s" % (self.__class__.__name__))
        self.log.setLevel(self.DB_LEVEL)

        self.passwd = None
        self.cmdPort = '7777'
        self.maxConnections = maxconnection
                
        self.log = log

        self.log.debug("[%s] Called (%s,%s)." % (mod, log, maxconnection))
        self.threadGroup = "CommandServer"
        
        #self.connectionManager = Thread(target=ConnectionManager, args=[self.maxConnections, log])
        #self.connectionManager.setDaemon(1)
        self.connectionManager = ConnectionManager(self.maxConnections,log) 
        self.connectionManager.start()
        self.services = {}
        
        
    def addService(self, service, port):
        """ add a service via its class on the port specified. """
        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)
        self.log.debug("[%s] Called %s,%s, %s)." % (mod, service, port, self.log))
                    
        if (self.services.has_key(port)):
            raise RuntimeError, ("Port %s already in use." % (port))
        
        #listener = Thread(target=Listener,
        #                  args=[port,service,self.connectionManager,self.log])
        listener = Listener(port,
                            service,
                            self.connectionManager,
                            self.log)
        
        #listener.setDaemon(1)
        self.services[port] = listener
        
        listener.start()
        
        
    # *************************************************************************
    # This method shuts the server down
    # 
    def shutdown(self):        
        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)
        self.log.debug("[%s] Called." % (mod))
        for port, l in self.services.iteritems():
            self.log.info("[%s] in iter port=%s service=%s." % (mod, port, l))
            l.pleaseStop()

        self.services.clear()
        self.connectionManager.shutdown()
        self.connectionManager = None
        sys.exit(0)
            

# *****************************************************************************
# Service - metaClass something for the service implmentor to follow.
#        
class Service:
    def __init__(self, server, passwd, log=None): pass
    def serve(self, conn=None): pass
    def close(self): pass

# *****************************************************************************    
# This is a non-trivial service.  It implements a command-based protocol
#    that gives password-protected runtime control over the operation of the
#    server.  See the main() method of the Server class to see how this
#    service is started.
#   
#    The recognized commands are:
#      password:   give password; authorization is required for most commands
#      add:        dynamically add a named service on a specified port
#      remove:     dynamically remove the service running on a specified port
#      max:        change the current maximum connection limit.
#      status:     display current services, connections, and connection limit
#      help:       display a help message
#      quit:       disconnect
#   
#    This service displays a prompt, and sends all of its output to the user
#    in capital letters.  Only one client is allowed to connect to this service
#    at a time.
##
class Control(Service):
    DB_LEVEL = logging.INFO

    def __init__(self, server, passwd, log):       
        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)
        #self.log = log
        self.log = logging.getLogger("%s" % (self.__class__.__name__))
        CommandServer.DB_LEVEL
        self.log.setLevel(CommandServer.DB_LEVEL)
        self.log.debug("[%s] Called (%s, %s)" % (mod, passwd, log))
            
        self.server = server              #  The server we control
        self.password = passwd            # The password we require
        self.connected = False          # Whether a client is already connected to us
        self.syncLock = threading.Condition(threading.Lock())
        self.authorized = False
         
         
        
    def serve(self, connTuple):
        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)
        self.log.debug("[%s] Called." % (mod))
        self.syncLock.acquire()     # there can only be one Control at a time
        c = connTuple[0]
        cLine = ""
        
        if (self.connected == True):
            c.send("ONLY ONE CONTROL CONNECTION ALLOWED AT A TIME.")
            c.close()
            self.syncLock.release()
            return
        else:
            self.connect = True

        # passwd override
        self.authorized = True
        c.send("OK. logged in.\n")
            
        # infinite loop at this point, main command processor 
        while True:
            c.send("Server:Control> ")
            cmd = c.recv(512)
            self.log.debug("[%s] cmd recv: %s" % (mod, cmd))
            if (len(cmd) == 0):
                break
            cLine = cmd.split()
            self.log.debug("[%s] cLine = %s" % (mod, cLine))
            
            # passwd change command
            if (cLine[0].upper() == "PASSWD" or cLine[0] == 'pw'):
                if (cLine[1] == self.password):
                    self.authorized = True
                    c.send("OK. logged in.\n")
                else:
                    c.send("INVALID PASSWORD\n")
                    
            # terminate this service
            elif (cLine[0].upper() == 'SHUTDOWN' or 
                  cLine[0].upper() == 'STOP'):
                if (self.authorized == False):
                    c.send("PASSWORD REQUIRED\n")
                else:
                    c.send("shutdown requested.\n")
                    
                    c.send("Server is terminating\n")
                    self.log.debug("[%s] Server shutdown command has been recieved. Starting Shutdown." % (mod))
                    c.close()
                    self.connected = False
                    self.server.shutdown()                    
                    self.log.debug("[%s] Terminating." % (mod))
                    return

            # help or ?
            elif(cLine[0].upper() == 'HELP'or cLine[0] == '?'):
                c.send("Supported commands:\n" \
                       "\tPassWd password  - enter password\n" \
                       "\tSTATus           - display current services, connections, and connection limit\n" \
                       "\tQuit             - quit this service\n" \
                       "\tShutdown         - shut the program down\n" \
                       "\tMax              - sets the maximum allow connections\n" \
                       "\tHelp (?)         - displays the menu\n" \
                       "\tRemove port      - dynamically remove the service running on a specified port\n" \
                       "\tAdd service:port - dynamically add a named service on a specified port\n" \
                       "\t\n")
            
            # quit handler
            elif (cLine[0].upper() == 'QUIT' or 
                  cLine[0].upper() == 'Q'):
                if (self.authorized == False):
                    c.send("PASSWORD REQUIRED\n")
                else:
                    self.syncLock.release()     # we can now release it.
                    break

            # add a service
            elif (cLine[0].upper() == 'ADD' or
                  cLine[0].upper() == 'A'): 
                if (self.authorized == False):
                    c.send("PASSWORD REQUIRED\n")
                else:
                    self.log.debug("[%s] Adding a new service: %s." % (mod, cLine[1]))
                    ns, newPort = cLine[1].split(':')
                    try:
                        nsObj = eval(ns)(self.log)
                        self.server.addService(nsObj, newPort)
                        self.log.debug("[%s] %s SERVICE ADDED ON PORT: %s." % (mod, 
                                                                               ns, 
                                                                               newPort))
                        c.send("%s SERVICE ADDED ON PORT: %s.\n" % (ns, newPort))
                    except:
                            try:    
                                t, v, tb = sys.exc_info()   
                                if t is not None:
                                    msg = ("[%s] %s : %s" % (mod, t, v))    
                                    self.log.error(msg)
                                    c.send(msg+"\n")
                            except:
                                pass
                            
                            self.log.error("[%s] Failed adding newservice: %s" % (mod, cLine[1]))
                            c.send("ERROR: Failed adding newservice, check that name is accurate: %s\n" % (cLine[1]))
                    
            # status command handler
            elif (cLine[0].upper() == 'STATUS' or
                  cLine[0].upper() == 'S' or cLine[0].upper() == 'STAT'):
                if (self.authorized == False):
                    c.send("PASSWORD REQUIRED\n")
                else:
                    #print self.server
                    #print self.server.services
                    #print type(self.server.services)
                    
                    #keys = self.server.services.keys()
                    #print "keys=%r" % (keys)
                    #print len(self.server.services)
                    if (len(self.server.services)):
                        items = self.server.services.items()
                        for port, list in items:
                            #print "keys=%s s=%r" % (port, list)
                            s= list.service
                            #print s
                            #print s.__class__
                            #print s.__class__.__name__
                            outStr = ("SERVICE %s On Port: %s \r\n" % (s.__class__.__name__, str(port)))
                            cm = self.server.connectionManager
                            outStr += cm.printConnections()
                            outStr += ("\n\rMAX CONNECTIONS: %d\n\r" % (cm.maxconnections))
                            c.send(outStr)
                    else:
                        c.send("No connections to report status on\n")

            # unrecognized command
            else:
                self.log.warn("[%s] unregogized command: %s" % (mod, cLine))
                c.send("UNRECOGNIZED COMMAND\n")
        
        c.close()
        c.connected = False
        self.log.debug("[%s] Terminating" % (mod))
        
        return
    
# ****************************************************************************
# Time - asimple time service
#
class timeService(Service):
#    DB_LEVEL = logging.INFO

    def __init__(self, log):       
        self.log = logging.getLogger("%s" % (self.__class__.__name__))
        self.log.setLevel(CommandServer.DB_LEVEL)
        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)
        self.log.debug("[%s] Called." % (mod))
        
    def serve(self,connTuple):
        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)
        self.log.debug("[%s] Called.(%s)" % (mod, connTuple))
        
        c=connTuple[0]
        c.send("Time-->%s\n\n" % (time.ctime()))
        c.close()
        return
 
    def close(self):
        pass
        

# ****************************************************************************
# Echo server - a simple server that echos what is sent to it
#
class echoService(Service):
#    DB_LEVEL = logging.INFO

    def __init__(self, log):       
        self.log = logging.getLogger("%s" % (self.__class__.__name__))
        self.log.setLevel(CommandServer.DB_LEVEL)
        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)
        self.log.debug("[%s] Called." % (mod))
        
    def serve(self,connTuple):
        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)
        self.log.debug("[%s] Called.(%s)" % (mod, connTuple))
        
        c = connTuple[0]
        while True:
            c.send("echoServer> ")
            cmd = c.recv(512)
            if (cmd.upper().find("QUIT") != -1):
                break

            c.send("echo-> %s\n" % (cmd))
            
        
        c.send("Terminating echoServer\n\n")
        c.close()
        return
 
    def close(self):
        pass
        

# *****************************************************************************
# Listener
#
class Listener(Thread):
#    DB_LEVEL = logging.INFO

    def __init__(self, port, service, conman, log):
        """The Listener constructor creates a thread for itself in the specified
        threadgroup.  It creates a ServerSocket to listen for connections
        on the specified port.  It arranges for the ServerSocket to be
        interruptible, so that services can be removed from the server."""
        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)
        self.log = logging.getLogger("%s" % (self.__class__.__name__))
        self.log.setLevel(CommandServer.DB_LEVEL)
        #self.log = log
    
        if hasattr(Thread, '__init__'):
            Thread.__init__(self)
        #self.log = log
        self.log.debug("[%s] Called:(%s,%s,%s)" % (mod,
                                                   port,
                                                   service,
                                                   conman))

        self.listen_socket = socket.socket(socket.AF_INET, 
                                             socket.SOCK_STREAM)
        
        self.listen_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        
        self.listen_socket.settimeout(2)
        self.port = int(port)
        self.listen_socket.bind(('', self.port))
        self.service = service
        self.cm = conman
        self.stop = False
        #self.log.debug("[%s] returning." % (mod))

    def pleaseStop(self):
        """This is the nice way to get a Listener to stop accepting connections."""
        self.stop = True
        #self.interrupt_main()

    def run(self):
        """A Listener is a Thread, and this is its body.
        Wait for connection requests, accept them, and pass the socket on
        to the ConnectionManager object of this server."""
        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)
        self.log.debug("[%s] Called -- listeing on port %d." % (mod, self.port))                
        self.listen_socket.listen(5)
        while (self.stop == False):
            try:
                client = self.listen_socket.accept()
                client[0].settimeout(None)
                self.log.debug("[%s] connection!: %s" % (mod, client))
                self.cm.addConnection(client, self.service)
            except Exception, inst:
                #self.log.error("[%s] stopFlag=%s exception=%s\n\ttype=%s\n\tinstance=%s" %
                #                (mod, self.stop, Exception, type(inst), inst.args))                
                #if (self.stop == True):
                #    self.service.close()
                #    self.listen_socket.close()
                #    return                                           
                    #break
                #else:
                pass

        if (type(self.service) != types.StringType):
            self.service.close()
            self.listen_socket.close()
        return    

    def interrupt_main(self):
        """ KeyboardInterrupt handler """
        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)
        self.log.debug("[%s] Called -- listeing on port %d." % (mod, self.port))
        self.service.close()
    
# *****************************************************************************
# Connection
# This constructor just saves some state and calls the superclass
#         constructor to create a thread to handle the connection.  Connection
#         objects are created by Listener threads.  These threads are part of
#         the server's ThreadGroup, so all Connection threads are part of that
#         group, too.
#
class Connection(Thread):         
#    DB_LEVEL = logging.INFO

    def __init__(self, clientConnection, service, ConnectionManager, log=None):
        """This constructor just saves some state and calls the superclass
         constructor to create a thread to handle the connection.  Connection
         objects are created by Listener threads.  These threads are part of
         the server's ThreadGroup, so all Connection threads are part of that
         group, too."""
         
        if hasattr(Thread, '__init__'):
            Thread.__init__(self)

        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)
        self.log = logging.getLogger("%s" % (self.__class__.__name__))
        self.log.setLevel(CommandServer.DB_LEVEL)
        #self.log = log
    
        self.log.debug("[%s] Called(%s,%s,%s)." % (mod,
                                                     clientConnection,
                                                     service,
                                                     ConnectionManager))
        self.client = clientConnection
        self.service = service
        self.cm = ConnectionManager

           
    def run(self):
        """ This is the body of each and every Connection thread.
        All it does is pass the client input and output streams to the
        serve() method of the specified Service object.  That method
        is responsible for reading from and writing to those streams to
        provide the actual service.  Recall that the Service object has been
        passed from the Server.addService() method to a Listener object
        to the ConnectionManager.addConnection() to this Connection object,
        and is now finally getting used to provide the service.
        Note that just before this thread exits it calls the
        ConnectionManager.endConnection() method to wake up the
        ConnectionManager thread so that it can remove this Connection
        from its list of active connections.        """
        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)
        self.log.debug("[%s] Called: %s:tid=%d" % (mod, 
                                                   self.getName(), 
                                                   thread.get_ident()))        
        
        try:
           if (type(self.service) == types.StringType):
               nsClass = __import__(self.service)
               nsObj = nsClass.__dict__[self.service](None)
               nsObj.serve(self.client)
           else:
               self.service.serve(self.client)
        except Exception, inst:
           self.log.critical("exception=%s\n\ttype=%s\n\tinstance=%s" %
                           (Exception, type(inst), inst.args))                                
        finally:
            #self.log.exception("[%s] error-->" % (mod))
            self.cm.endConnection()
            self.log.debug("[%s] Terminatingg." % (mod))        
            return
    
# *****************************************************************************
# ConnectionManager
#
class ConnectionManager(Thread):
#    DB_LEVEL = logging.INFO
        
#    def __init__(self, group, maxconnections, log):
    def __init__(self, maxconnections, log):
        if hasattr(Thread, '__init__'):
            Thread.__init__(self) 

        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)
        self.log = logging.getLogger("%s" % (self.__class__.__name__))
        self.log = log
        self.log.setLevel(CommandServer.DB_LEVEL)

        self.log.debug("[%s] Called." % (mod))

        self.server = None
        self.maxconnections = maxconnections                
        self.connections = []
        self.terminateFlag = False      
        self.setDaemon(True)
        
        self.log.info("Starting connection manager.  Max connections: %d" % (maxconnections))

        # --- setup all the sync locks and Conditional Variables (CV)
        self.addConnectionSyncLock = threading.Condition(threading.Lock())
        self.setMaxConnSyncLock = threading.Condition(threading.Lock())
        self.endConnectionSyncLock = threading.Condition(threading.Lock())
        self.endConnectionCV = threading.Condition(threading.Lock())
        self.shutdownSyncLock = threading.Condition(threading.Lock())


    def getConnectionManagerInstance(self):
        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)
        self.log.debug("[%s] Called." % (mod)) 
        return self.connectionManagerInstance

    # **************************************************************************
    # addConnection
    #     This is the method that Listener objects call when they accept a
    #     connection from a client.  It either creates a Connection object
    #     for the connection and adds it to the list of current connections,
    #     or, if the limit on connections has been reached, it closes the
    #     connection     
    #
    # @param0 (socket,address) - of the client that just connected
    # @param1 service - the that will handle the connection.
    #
    def addConnection(self, newConnection, service):
        """ This is the method that Listener objects call when they accept a
         connection from a client.  It either creates a Connection object
         for the connection and adds it to the list of current connections,
         or, if the limit on connections has been reached, it closes the
         connection."""
         
        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)

        self.addConnectionSyncLock.acquire()
        self.log.debug("[%s] Called." % (mod))
        
        
        if (len(self.connections) >= self.maxconnections):
            # the max number of connections has been reach, reject connection
            self.log.warn("Connection refused; Server has reached maximum number of clients.")
            s.close()
        else:
            # Create a Connection thread to handle this connection
            c = Connection(newConnection, service, self, self.log)
            
            # Add it to the list of current connections
            
            self.connections.append(c)   
            self.log.info("[%s] Connected to %s for service %s" % (mod,
                                                                     newConnection[0].getsockname(),
                                                                     service.__class__.__name__))
            c.start()
                    
        self.addConnectionSyncLock.release()

    def setMaxConnections(self, maxVal):
        """Change the current connection limit"""
        self.setMaxConnSyncLock.acquire()
        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)

        self.log.debug("[%s] Called(%d)." % (mod, maxVal)) 
        self.maxConnections = maxVal
        self.setMaxConnSyncLock.release()

    def endConnection(self):
        """ A Connection object calls this method just before it exits.
        This method uses notify() to tell the ConnectionManager thread
        to wake up and delete the thread that has exited. """
        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)
        self.log.debug("[%s] Called." % (mod))
        self.endConnectionCV.acquire()
        self.log.debug("[%s] notify---->." % (mod))
        self.endConnectionCV.notify()
        self.endConnectionCV.release()
    
    def printConnections(self):
        """ returns the current list of connections to the caller. This 
        service is used by the Control service only."""
        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)
        self.log.debug("[%s] Called." % (mod))
        rtnStr = None
        i = 0        
        msg = ""
        
        while (i < len(self.connections)):
            c = self.connections[i]
            if (c.isAlive()):
                ipaddr, port = c.client[0].getpeername()
                msg = "CONNECTED TO %s:%d ON PORT %d" % (ipaddr, 
                                                       c.client[1][1],
                                                       port
                                                       ) 
                #msg += "%r\n" % (c)
                self.log.info("[%s] - %s" % (mod, msg))
            i += 1
            
        
        self.log.debug("[%s] return" % (mod))
        return msg        
        
    def shutdown(self):
        """ Shutdown all the connections. """
        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)
        self.log.debug("[%s] Called." % (mod))

        for c in self.connections:
            self.log.debug("[%s] Closing connections: %s." % (mod, c))            
            #c.close()            
            c = None
            
        self.terminateFlag = True

    def run(self):
        """ The ConnectionManager is a thread, and this is the body of that
        thread.  While the ConnectionManager methods above are called by other
        threads, this method is run in its own thread.  The job of this thread
        is to keep the list of connections up to date by removing connections
        that are no longer alive.  It uses wait() to block until notify()'d by
        the endConnection() method."""
        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)
        self.log.debug("[%s] Called." % (mod))

        while 1:
            #print "1--------------len - %d--------" % (len(self.connections))    
            for c in self.connections:
                if (c.isAlive() == False):
                   self.connections.remove(c)
                   self.log.warn("[%s] Connection to %s closed" % (mod, c))
                
            #print "--------------len - %d--------" % (len(self.connections))    

            if self.terminateFlag == True:
                self.log.debug("[%s] TerminateFlag -  terminating" % (mod))
                return
                #sys.exit()
                    
            # -- end of loop
           
            # Now wait to be notify()'d that a connection has exited
            # When we wake up we'll check the list of connections again.
            try:
                self.waitForConnectionEnd()
            except:
                self.log.warn("[%s] terminating" % (mod))
                return
            
    def waitForConnectionEnd(self):
        mod = "%s:%s" % (self.__class__.__name__, sys._getframe().f_code.co_name)
        self.log.debug("[%s] Called." % (mod))
        if (self.terminateFlag == False):
            self.endConnectionCV.acquire()
            self.endConnectionCV.wait()
            self.endConnectionCV.release()

        else:
            self.log.debug("[%s] Raising termination exception." % (mod))
            raise RunTimeError, "Terminating"
        
        self.log.debug("[%s] Returning." % (mod))
                         
# -----------------------------------------------------------------------------------------



   # TODO: dkehn@noironetworks.com - fragment of commented out code should be 
   #     implemented when have time. This snippet is from Java that could be used
   #     to display the connections and what is connected to them, treat this as
   #     psuedo code (non-working in Java).
   # /**
   #  * Output the current list of connections to the specified stream.
   #  * This method is used by the Control service defined below.
   #  **/
   # public synchronized void printConnections(PrintWriter out) {
   #   for(int i = 0; i < connections.size(); i++) {
   #     Connection c = (Connection)connections.elementAt(i);
   #     out.println("CONNECTED TO " +
   #                 c.client.getInetAddress().getHostAddress() + ":" +
   #                 c.client.getPort() + " ON PORT " + c.client.getLocalPort()+
   #                 " FOR SERVICE " + c.service.getClass().getName());
   #   }
   # }


# -----------------------------------------------------------------------------------------
# ******************************************************************************
# main()
# commanline:
#     -d debug on (default=off)
#     -p telnet port for command server default=7777
#
def main():
    logging.basicConfig(format="%(levelname)-8s %(process)d  %(asctime)s [%(thread)d]%(message)s",
                        level = logging.INFO)
    #logging.basicConfig(level = logging.DEBUG)
    logger = logging.getLogger("CommandServer")
    
    #print sys.argv
    # see we are going to log to a file?
    if (sys.argv.count('-l')):
        logFile = sys.argv[sys.argv.index('-l')+1]
        print "logging to file %s " % (logFile)
        hdlr = logging.FileHandler(logFile)
        formatter = logging.Formatter('%(asctime)s %(levelname)s %(message)s')
        logger.addHandler(hdlr) 
        
    elif (sys.argv.count('--logFile')):
        logFile = sys.argv[sys.argv.index('--logFile')+1]
        print "logging to file %s " % (logFile)
        hdlr = logging.FileHandler(logFile)
        formatter = logging.Formatter('%(asctime)s %(levelname)s %(message)s')
        logger.addHandler(hdlr)
        
    else:
        print "logging to stdout"
         
        
    
    logger.debug("Starting...............%s" % (sys.argv))
    
    pid = os.getpid()
    msg = "%d\n" % (pid)
    fd = open("/tmp/cmdServer.pid", "w")
    fd.write(msg)
    fd.close()
           
    if (len(sys.argv) < 2):
        print("Must start at least one service.")
        usage()
        sys.exit()

    #retcode = createDaemon()   
    # Create a server object that uses standard out as its log and
    # has a limit of ten conncurrent connections at once. 
    s = CommandServer(logger, 10)
    debugFlag = False
    
    try:
        opts, args = getopt.getopt(sys.argv[1:], "hc:ds:l:", ["help", "control=", "debug", "service=", "logFile"])
    except getopt.GetoptError:
        # print help information and exit:
        usage()
        sys.exit(2)

    for o, a in opts:
        if o in ("-d", "--debug"):
            debugFlag = True
            CommandServer.DB_LEVEL = logging.DEBUG

        elif o in ("-l", "--logFile"):        # this is used to setup db logging as well
            # if there are ':' in the string we are doing a database log hander else file.
            if (a.find(":") != -1):
                print "Using DBHandler connectString=%s " % (a)
                import DBHandler
                dh = DBHandler(a, 'process_log', False)
                logger = logging.getLogger("")
                logger.setLevel(logging.DEBUG)
                logger.addHandler(dh)
            else:
                print "Using log file %s " % (a)
                hdlr = logging.FileHandler(a)
                formatter = logging.Formatter('%(asctime)s %(levelname)s %(message)s')
                logger.addHandler(hdlr) 
                logging.basicConfig(format="%(levelname)-8s %(process)d  %(asctime)s [%(thread)d]%(message)s",
                                    level = logging.INFO, filename=logFile)
            
            
        elif o in ("-h", "--help"):
            usage()
            sys.exit()

        elif o in ("-c", "--control"):
            try:
                service = s.addService(Control(s, a.split(':')[0], logger), a.split(':')[1])
            except Exception, inst:
                logger.critical("exception=%s\n\ttype=%s\n\tinstance=%s" %
                                (Exception, type(inst), inst.args))
                sys.exit(-1)

        elif o in ("-s", "--service"):
            try:
                print a
                slist = list(a.split(','))
                for ns in slist:
                    nsObjName = ns.split(':')[0]
                    nsClass = __import__(nsObjName)
                    #nsObj = nsClass.__dict__[nsObjName](None)
                    #nsObj = eval(nsObj)(None)
                    newPort = ns.split(':')[1]
                    s.addService(nsObjName, newPort)
                    #s.addService(nsObj, newPort)

                    logger.info("adding service: %s on %s" % (ns.split(':')[0], ns.split(':')[1]))
                    #service = s.addService(ns.split(':')[0], ns.split(':')[1])
                    
            except Exception, inst:
                logger.critical("exception=%s\n\ttype=%s\n\tinstance=%s" %
                                (Exception, type(inst), inst.args))
                sys.exit(-1)
                    
        else:
            # Otherwise start the named service on the specified port.
            # Dynamically load and instantiate a class that implements Service.
            print "in else:" 
            print "o=%s a=%s" % (o,a)
            service = s.addService(a.split(':')[0], a.split(':')[1])

        
# *************************************************************************
# aboutDialog - just what it says
#
def usage():
    print("Usage: CommandServer -c password:port -s service:port -l logfile_or_db_connection" )


# ****************************************************************************
# The kicker to get us going.
#
DAEMONIZE = False

if __name__ == '__main__':
    # do the UNIX double-fork magic, see Stevens' "Advanced 
    # Programming in the UNIX Environment" for details (ISBN 0201563177)
    if (DAEMONIZE == True):
        try: 
            pid = os.fork() 
            if pid > 0:
                # exit first parent
                sys.exit(0) 
        except OSError, e: 
            print >>sys.stderr, "fork #1 failed: %d (%s)" % (e.errno, e.strerror) 
            sys.exit(1)

        # decouple from parent environment
        os.chdir("/") 
        os.setsid() 
        os.umask(0) 
    
        # do second fork
        try: 
            pid = os.fork() 
            if pid > 0:
                # exit from second parent, print eventual PID before
                print "Daemon PID %d" % pid 
                sys.exit(0) 
        except OSError, e: 
            print >>sys.stderr, "fork #2 failed: %d (%s)" % (e.errno, e.strerror) 
            sys.exit(1) 

    
    # start the program
    main()
