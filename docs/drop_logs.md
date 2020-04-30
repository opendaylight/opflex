# Drop logging

This feature sets up logging for every packet that did not hit any match in a table and would subsequently have been dropped.  
Some explicit drop conditions, for eg. uplink tunnel not up and packet is intended to be sent to uplink are also captured.  
Every dropped packet is encapsulated in a geneve header and sent to the loopback address and the local-port as configured. 
Following are the configuration steps for droplog feature:

 1. Populate these sections in the opflex-agent config file.  

```
    "drop-log": {  
        // Encapsulate drop-log traffic with GENEVE.  
        "geneve" : {  
           // The name of the drop-log tunnel integration interface in OVS  
           "int-br-iface": "gen1",  
           // The name of the drop-log tunnel access interface in OVS  
           "access-br-iface": "gen2",  
           // Remote IP address that the packet should be sent to.  
           "remote-ip" : "192.168.1.2"  
        }   
    },  

    "drop-log-config-sources": {
            // Filesystem path to monitor for drop log control
            // Default: no drop log service
            "filesystem": ["/var/lib/opflex-agent-ovs/droplog"]
    } 
```  
   Ensure  the directory mentioned above is present.  
   /var/lib/opflex-agent-ovs/droplog

 2. Add droplog geneve tunnel ports to both bridges.  

    > sudo ovs-vsctl add-port br-int gen1 -- set interface gen1 type=geneve options:remote_ip=flow options:key=1  
    >
    > sudo ovs-vsctl add-port br-access gen2 -- set interface gen2 type=geneve options:remote_ip=flow options:key=2  

 3. Create a file with the name (a.droplogcfg) in the directory above(/var/lib/opflex-agent-ovs/droplog)  
    with the following contents  

```
    {  
        "drop-log-enable" : true  
    }   
```  
  This will enable logging for all dropped packets and can de disabled at runtime by changing the enable value to false.

 4. Add an iptables rule to redirect droplogs to local port  
> sudo iptables -t nat -A OUTPUT -p udp --dport 6081 -j DNAT --to 127.0.0.1:50000

5. An exporter thread publishes the dropped packets as events with the packet tuple to a unix datagram socket which can be configured, if needed as follows  

```  
    "packet-event-notif": {
        "socket-name": ["/usr/local/var/run/packet-event-notification.sock"]
    }
```  


## Test DropLogs  
Create a drop condition for one of the endpoints by changing their mac address in the ep file( which would create a security table drop).  
Ping another endpoint in the same EPG from this endpoint.  
Observe the dropped packet being logged in opflex_agent log file.  
eg:  
```
[2020-Jan-03 11:57:54.416092] [info] [ovs/PacketLogHandler.cpp:196:parseLog] : Int-PORT_SECURITY_TABLE MAC=0a:ff:ae:02:97:80:ee:42:6b:1a:3c:d6:IPv4 SRC=101.0.0.3 DST=101.0.0.5 LEN=84 DSCP=0 TTL=64 ID=52995 FLAGS=2 FRAG=0 PROTO=ICMP TYPE=8 CODE=0 ID=16920 SEQ=3  

```

Create two endpoints in two separate EPGs and don't configure any contracts between them.  Setup the default route in each endpoint's netnamespace to the virtual router IP and try to ping one endpoint from the other.  
Observe the dropped packet being logged in opflex_agent log file.  
eg:  
```
[2020-Apr-29 19:31:31.278324] [info] [ovs/PacketLogHandler.cpp:196:parseLog] Int-POL_TABLE  MAC=36:0e:d6:11:0d:db:00:22:bd:f8:19:ff:IPv4 SRC=11.0.0.4 DST=12.0.0.4 LEN=84 DSCP=0 TTL=63 ID=46838 FLAGS=2 FRAG=0 PROTO=ICMP TYPE=8 CODE=0 ID=1688 SEQ=1  

```
