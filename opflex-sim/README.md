
The CommandServer & Service Documentation
=========================================

Background:
----------
The CommandServer.py program is basically a generic muti-threaded server that
can easily be exteded. The Server class is defines a multi-threaded server 
that provides services by implementations of a nested Service interface. It can
provide multiple services (defined by multiple service objects) on multiple
ports, and has the ability to dynamically load and instantiate Service classes
and add (and remove) new services at runtime. It logs its actions to a
specified stream and limits the number of concurrent connections to specified
maximum.


The Server class uses a number of nested classes. The Listener class is a thread
that waits for connections on a given port. There is one Listener object for each
service the Server is providing. The ConnectionManager class manages the list of
current connections to all services. There is one ConnectionManager shared by
all services. When a Listener gets a connection from a client, it passes it
to the ConnectionManage, which rejects it if connection limit has been reached.
If the ConnectionManager does not reject a client, it creates a Connection
object to handle the connection. Connection is a thread sub-class, so each
service can handle multiple connections at a time, makingthis a multi-threaded
server. Each Connection object is passed a Service object and invoked its
server() method, which is actually what provides the service.

The Service interface is a psuedo python implementation of an interface. There
are a number of test implementations in the CommandServer file (echo, time,
etc.). These are for testing and demonstration purposes only. The Control 
class, however, is a non-trivial Service . This service provides password-
protected runtime access to the server, and allows remote administrator to
add and remove services, check the server status, and change the current
connection limit.

Finally, the main() method of Server is a standalone program that creates
and runs the Server. By specifiing the -c (--control) argument on the 
commandline, you can tell this program to create an instance of the Control
service so that the server can be administered at runtime. The following
commandline arguments are necessary to run the genericService, which is what
is used to provide remote monitoring on remote nodes:

./CommandServer.py -c b:7770 -s opflexService:7777 -s consoleService:8888

Where, -c start the Control service on port 7770 and will require 'b' and the
password inorder to gain access, and the opflexService will be started 
listening on port 7777 and consoleServer on port 8888.

The Program is intended to be completely self contained.

Installation:
-------------
The main program file is the CommandService.py. It is also necessary to provide
the service's are are instended to be provided in the same directory as the 
CommandServer.py file. This is done such that python loader can find them with 
minimal effort. To run the program a cmdserve #!/usr/bin/env python, which
searches $PATH for python and runs it. (Usually env is used to set some
environment variables for a program, e.g. 'env PYTHONPATH=whatever python', but
one need not specify any environment variables), this so you don't have to type
python then the program you wish the interpreter to run - details..........


The CommandServer.py is put whereever you desire, but I would recommend putting it in your path somewhere.

In addition, there is included a start/stop script cmdServer, which contains the necessary script code 
to support the /etc/init.d/ code to run the CommandServer.py. 


Program Options
===============

Using telnet to connect ot the Control service it will provide the following session:

[dkehn@halt60p src] > telnet mars 7770
Trying 192.168.100.105...
Connected to mars.
Escape character is '^]'.
Server:Control> help
Supported commands:
    PassWd password  - enter password
    STATus           - display current services, connections, and connection limit
    Quit             - quit this service
    Shutdown         - shut the program down
    Max              - sets the maximum allow connections
    Help (?)         - displays the menu
    Remove port      - dynamically remove the service running on a specified port
    Add service:port - dynamically add a named service on a specified port

Server:Control>
