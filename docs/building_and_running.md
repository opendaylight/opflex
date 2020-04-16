# Building and Running

## Building

The project is organized into four main sections.  You'll need to build each in the right order

You can check out opflex repository with https as follows:
 - git clone https://git.opendaylight.org/gerrit/opflex

OR

You can check out opflex repository with ssh as follows:

 - Have a login on the ODL side and [setup ssh keys](https://docs.opendaylight.org/en/stable-boron/developer-guide/getting-started-with-git-and-gerrit.html)
 - git clone ssh://yourusername@git.opendaylight.org:29418/opflex.git


### libopflex

libopflex requires
- libuv >= 1.20
- rapidjson 1.1
- Boost 1.49


Install Boost base and the Boost unit testing library, plus libuv1, doxygen, openssl and rapidjson-dev from your preferred distro.

If rapidjson-dev is not available, you can install from source:
 - git clone https://github.com/miloyip/rapidjson.git --branch version1.1.0 --depth 1
 - pushd rapidjson
 - cmake .
 - make
 - sudo make install
 - popd

Now, you can build libopflex:
 - pushd opflex/libopflex
 - ./autogen.sh
 - ./configure
 - make -j4
 - make check
 - sudo make install
 - popd

If you get an error about a missing dependency, when you run configure you may need to set your PKG_CONFIG_PATH variable, pass in a --with-boost option , or set the CPPFLAGS variable.

As part of this build, fairly extensive documentation on using libopflex will be generated into the doc/html directory using Doxygen.

### genie and libmodelgbp
Genie is a java project, but will generate the C++-based libmodelgbp (in the future, it will generate other model libraries as well).  libmodelgbp depends on libopflex.

First, run the code generator:
 - pushd opflex/genie
 - mvn compile exec:java
 - popd

Next build and install the libmodelgbp library:
 - pushd opflex/genie/target/libmodelgbp
 - bash autogen.sh
 - ./configure
 - make
 - sudo make install
 - popd

### OVS
The agent requires the Open vSwitch 2.12.0 library to connect to OVS and configure OpenFlow on the switch.

 - git clone https://github.com/openvswitch/ovs.git --branch v2.12.0 --depth 1
 - pushd ovs
 - ROOT=/usr/local
 - ./boot.sh
 - ./configure --prefix=$ROOT --enable-shared
 - make -j4
 - sudo rm -rf $ROOT/include/openvswitch
 - sudo make install
 - OVS headers get installed to weird and inconsistent locations. Try to clean things up
    - sudo mkdir -p $ROOT/include/openvswitch/openvswitch
    - sudo mv $ROOT/include/openvswitch/*.h $ROOT/include/openvswitch/openvswitch
    - sudo mv $ROOT/include/openflow $ROOT/include/openvswitch
    - sudo cp -t "$ROOT/include/openvswitch/" include/*.h
    - sudo find lib -name "*.h" -exec cp --parents -t "$ROOT/include/openvswitch/" {} \;
    - popd

If you're running Ubuntu 16.04+ or RHEL7+ you should not need to install the OVS kernel data path module.

### agent-ovs
The OVS agent depends on OVS, libopflex and libmodelgbp. You can build agent-ovs as follows:
 - opflex/agent-ovs
 - ./autogen.sh
 - ./configure
 - make -j4
 - make check
 - sudo make install
 - popd

## Running

To run the agent, you need to supply a configuration file.  A default file is generated as part of the build called opflex-agent-ovs.conf.  You'll need to edit at the the "peers" section to point to the OpFlex server.  A mock server is provided for testing.

To start the mock server, you'll need a policy file for the server.  You can generate this by running:
```
 $ ./mock_server --sample=policy.json
 [2014-Dec-15 10:17:00.684272] [info] [../../ofcore/OFFramework.cpp:104:dumpMODB] Wrote MODB to test.json
```
You can edit this policy as you see fit manually, but for now, start the mock server using your policy file as follows:
```
 $ ./mock_server --policy=policy.json
 [2014-Dec-15 10:16:51.257168] [info] [../../engine/MockOpflexServer.cpp:110:readPolicy] Read 54 managed objects from policy file "policy.json"
 [2014-Dec-15 10:16:51.266641] [info] [../../engine/OpflexListener.cpp:99:listen] Binding to port 8009
```
By default, it will run on the terminal with logging to standard output (use Ctrl+C to kill it).  You can also run it as a daemon in the background:
```
 $ ./mock_server --policy=/tmp/test.json --log=/tmp/server.log --daemon
```
To connect to the mock server, set the peers list in the agent configuration file to 127.0.0.1 on port 8009.  You may need to first create a directory  ${localstatedir}/lib/opflex-agent-ovs/endpoints/ and ${localstatedir}/lib/opflex-agent-ovs/ids/.  Then you can run the agent as:
```
 $ ./opflex_agent -c opflex-agent-ovs.conf[2014-Dec-15 10:21:25.495017] [info] [../src/main.cpp:91:main] Reading configuration from opflex-agent-ovs.conf
 [2014-Dec-15 10:27:49.334882] [info] [../src/main.cpp:91:main] Reading configuration from opflex-agent-ovs.conf
 [2014-Dec-15 10:27:49.336714] [info] [../src/Agent.cpp:122:start] Starting OVS Agent
 [2014-Dec-15 10:27:49.343139] [info] [../src/FSEndpointSource.cpp:202:operator()] Watching "/usr/local/var/lib/opflex-agent-ovs/endpoints/" for endpoint data
 [2014-Dec-15 10:27:49.345655] [info] [../../engine/OpflexClientConnection.cpp:125:connect_cb] [127.0.0.1:8009] New client connection
 [2014-Dec-15 10:27:49.346215] [info] [../../engine/OpflexPEHandler.cpp:118:ready] [127.0.0.1:8009] Handshake succeeded
```
Hit Ctrl+C to kill the agent.  The agent can also run as a daemon.  Run any command with --help to see a list of options.

A convenient way to test the agent is to use an OVS with mininet to create simulated endpoints.  If you have mininet and openvswitch installed correctly, you can simply run (in another terminal):
```
 $ sudo mn --topo=single,6 --controller=none --mac
 *** Creating network
 *** Adding controller
 *** Adding hosts:
 h1 h2 h3 h4 h5 h6 
 *** Adding switches:
 s1 
 *** Adding links:
 (h1, s1) (h2, s1) (h3, s1) (h4, s1) (h5, s1) (h6, s1) 
 *** Configuring hosts
 h1 h2 h3 h4 h5 h6 
 *** Starting controller
 *** Starting 1 switches
 s1 
 *** Starting CLI:
 mininet> 
```
You can add a tunnel interface to your mininet bridge as follows:
```
 $ sudo ovs-vsctl add-port s1 s1_vxlan0 -- set Interface s1_vxlan0 type=vxlan options:remote_ip=flow options:key=flow
 $ sudo ovs-vsctl show
 fbc84eab-3558-40ad-acf1-a0e89e70c6c8
     Bridge "s1"
         Controller "ptcp:6634"
         fail_mode: secure
         Port "s1"
             Interface "s1"
                 type: internal
         Port "s1-eth5"
             Interface "s1-eth5"
         Port "s1-eth6"
             Interface "s1-eth6"
         Port "s1-eth4"
             Interface "s1-eth4"
         Port "s1_vxlan0"
             Interface "s1_vxlan0"
                 type: vxlan
                 options: {key=flow, remote_ip=flow}
         Port "s1-eth2"
            Interface "s1-eth2"
         Port "s1-eth3"
             Interface "s1-eth3"
         Port "s1-eth1"
             Interface "s1-eth1"
     ovs_version: "2.3.90"
```
Now you'll need to configure the agent to connect to this mininet bridge.  Edit your agent configuration by uncommenting the "stitched-mode renderer".  Set it to point to your mininet bridge "s1" with your tunnel interface "s1_vxlan0".  Now you can run the agent again (as root):
```
 $ sudo ./opflex_agent -c opflex-agent-ovs.conf[2014-Dec-15 10:38:38.238758] [info] [../src/main.cpp:91:main] Reading configuration from opflex-agent-ovs.conf
 [2014-Dec-15 10:38:38.240564] [info] [../src/Agent.cpp:122:start] Starting OVS Agent
 [2014-Dec-15 10:38:38.273808] [info] [../../engine/OpflexClientConnection.cpp:125:connect_cb] [127.0.0.1:8009] New client connection
 [2014-Dec-15 10:38:38.273861] [info] [../src/FSEndpointSource.cpp:202:operator()] Watching "/usr/local/var/lib/opflex-agent-ovs/endpoints/" for endpoint data
 [2014-Dec-15 10:38:38.274346] [info] [../../engine/OpflexPEHandler.cpp:118:ready] [127.0.0.1:8009] Handshake succeeded
```
To register an endpoint, drop an endpoint file into the endpoint watch directory.  Here is an example endpoint file that will work with h1 from mininet above.  Create the file ${localstatedir}/lib/opflex-agent-ovs/endpoints/h1.ep as follows:
```
 {
     "policy-space-name": "test",
     "endpoint-group-name": "group1",
     "interface-name": "s1-eth1",
     "ip": [
         "10.0.0.1"
     ],
     "mac": "00:00:00:00:00:01",
     "uuid": "83f18f0b-80f7-46e2-b06c-4d9487b0c754"
 }
```
You should see a log line such as:
```
 [2014-Dec-15 10:46:54.532979] [info] [../src/FSEndpointSource.cpp:160:readEndpoint] Updated endpoint Endpoint[uuid=83f18f0b-80f7-46e2-b06c-4d9487b0c754,ips=[10.0.0.1],eg=group1,mac=00:00:00:00:00:01,iface=s1-eth1 from "/usr/local/var/lib/opflex-agent-ovs/endpoints/h1.ep"
```
And then you can view the flows created:
```
 $ sudo ovs-ofctl -OOpenFlow13 dump-flows s1
 OFPST_FLOW reply (OF1.3) (xid=0x2):
 cookie=0x0, duration=1.359s, table=0, n_packets=0, n_bytes=0, priority=20,in_port=1,dl_src=00:00:00:00:00:01 actions=goto_table:1
 cookie=0x0, duration=1.359s, table=0, n_packets=0, n_bytes=0, priority=40,arp,in_port=1,dl_src=00:00:00:00:00:01,arp_spa=10.0.0.1 actions=goto_table:1
 ...
```
Add another host h2 and then you should be able to run a ping:
```
 mininet> h1 ping h2 -c1
 PING 10.0.0.2 (10.0.0.2) 56(84) bytes of data.
 64 bytes from 10.0.0.2: icmp_seq=1 ttl=64 time=0.304 ms 
 
 --- 10.0.0.2 ping statistics ---
 1 packets transmitted, 1 received, 0% packet loss, time 0ms
 rtt min/avg/max/mdev = 0.304/0.304/0.304/0.000 ms
```