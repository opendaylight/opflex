#
# agent-ovs: Opflex agent for integrating Group-based policy with OVS
# Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v1.0 which accompanies this distribution,
# and is available at http://www.eclipse.org/legal/epl-v10.html
#
###########
#
# Process this file with automake to produce a Makefile.in

ACLOCAL_AMFLAGS = -I m4

OVS_ADDL_CFLAGS = \
	-D__STDC_LIMIT_MACROS \
	-D__STDC_CONSTANT_MACROS \
	-fpermissive

AM_CPPFLAGS = $(BOOST_CPPFLAGS) \
	-I$(top_srcdir)/src/include \
	-DLOCALSTATEDIR='"$(localstatedir)"'

AM_LDFLAGS = $(BOOST_LDFLAGS)

noinst_LTLIBRARIES = libagent_ovs.la
libagent_ovs_la_CFLAGS = \
	$(libopenvswitch_CFLAGS) \
	$(libofproto_CFLAGS)
libagent_ovs_la_CXXFLAGS = \
	$(libopflex_CFLAGS) \
	$(libmodelgbp_CFLAGS) \
	$(OVS_ADDL_CFLAGS) \
	$(libopenvswitch_CFLAGS) \
	$(libofproto_CFLAGS) \
	$(libnfct_CFLAGS) \
	$(rapidjson_CFLAGS)
libagent_ovs_la_LIBADD = \
	$(libopflex_LIBS) \
	$(libmodelgbp_LIBS) \
	$(libopenvswitch_LIBS) \
	$(libofproto_LIBS) \
	$(libnfct_LIBS) \
	$(BOOST_SYSTEM_LIB) \
	$(BOOST_FILESYSTEM_LIB) \
	$(BOOST_ASIO_LIB)

noinst_HEADERS = \
	src/include/AgentLogHandler.h \
	src/include/PolicyListener.h \
	src/include/PolicyManager.h \
	src/include/FSWatcher.h \
	src/include/Endpoint.h \
	src/include/EndpointListener.h \
	src/include/EndpointManager.h \
	src/include/EndpointSource.h \
	src/include/FSEndpointSource.h \
	src/include/ModelEndpointSource.h \
	src/include/Service.h \
	src/include/ServiceListener.h \
	src/include/ServiceSource.h \
	src/include/ServiceManager.h \
	src/include/FSServiceSource.h \
	src/include/ExtraConfigListener.h \
	src/include/ExtraConfigManager.h \
	src/include/FSRDConfigSource.h \
	src/include/RDConfig.h \
	src/include/Renderer.h \
	src/include/StitchedModeRenderer.h \
	src/include/TunnelEpManager.h \
	src/include/FlowExecutor.h \
	src/include/FlowReader.h \
	src/include/SwitchStateHandler.h \
	src/include/IntFlowManager.h \
	src/include/AccessFlowManager.h \
	src/include/ovs-shim.h \
	src/include/ovs-ofputil.h \
	src/include/ovs-ofpbuf.h \
	src/include/TableState.h \
	src/include/ActionBuilder.h \
	src/include/FlowBuilder.h \
	src/include/SwitchConnection.h \
	src/include/SwitchManager.h \
	src/include/PortMapper.h \
	src/include/InterfaceStatsManager.h \
	src/include/PolicyStatsManager.h \
	src/include/ContractStatsManager.h \
	src/include/SecGrpStatsManager.h \
	src/include/Agent.h \
	src/include/IdGenerator.h \
	src/include/CtZoneManager.h \
	src/include/RangeMask.h \
	src/include/Packets.h \
	src/include/PacketInHandler.h \
	src/include/AdvertManager.h \
	src/include/KeyedRateLimiter.h \
	src/include/NotifServer.h \
	src/include/MulticastListener.h \
	src/include/TaskQueue.h \
	src/include/FlowUtils.h \
	src/include/FlowConstants.h \
	src/include/Network.h \
	src/include/udp.h \
	src/include/dhcp.h \
	src/include/arp.h \
	src/include/eth.h \
	src/include/cmd.h \
	src/include/logging.h

libagent_ovs_la_SOURCES = \
	src/AgentLogHandler.cpp \
	src/PolicyManager.cpp \
	src/FSWatcher.cpp \
	src/Endpoint.cpp \
	src/EndpointManager.cpp \
	src/EndpointSource.cpp \
	src/FSEndpointSource.cpp \
	src/ModelEndpointSource.cpp \
	src/Service.cpp \
	src/ServiceSource.cpp \
	src/ServiceManager.cpp \
	src/FSServiceSource.cpp \
	src/ExtraConfigManager.cpp \
	src/RDConfig.cpp \
	src/FSRDConfigSource.cpp \
	src/Renderer.cpp \
	src/TunnelEpManager.cpp \
	src/cmd.cpp \
	src/logging.cpp \
	src/Agent.cpp \
	src/IdGenerator.cpp \
	src/CtZoneManager.cpp \
	src/NotifServer.cpp \
	src/MulticastListener.cpp \
	src/TaskQueue.cpp \
	src/Network.cpp \
	src/FlowConstants.cpp

if RENDERER_OVS
  libagent_ovs_la_SOURCES += \
	src/StitchedModeRenderer.cpp \
	src/SwitchStateHandler.cpp \
	src/IntFlowManager.cpp \
	src/AccessFlowManager.cpp \
	src/TableState.cpp \
	src/FlowExecutor.cpp \
	src/FlowReader.cpp \
	src/ActionBuilder.cpp \
	src/FlowBuilder.cpp \
	src/SwitchConnection.cpp \
	src/SwitchManager.cpp \
	src/PortMapper.cpp \
	src/PolicyStatsManager.cpp \
	src/InterfaceStatsManager.cpp \
	src/ContractStatsManager.cpp \
	src/SecGrpStatsManager.cpp \
	src/RangeMask.cpp \
	src/Packets.cpp \
	src/PacketInHandler.cpp \
	src/AdvertManager.cpp \
	src/FlowUtils.cpp \
	src/ovs-shim.c
endif

bin_PROGRAMS=agent_ovs gbp_inspect mcast_daemon
agent_ovs_CXXFLAGS = -DSYSCONFDIR='"$(sysconfdir)"' \
	$(libopflex_CFLAGS) \
	$(libmodelgbp_CFLAGS)
agent_ovs_SOURCES = \
	src/main.cpp
agent_ovs_LDADD = \
	$(BOOST_PROGRAM_OPTIONS_LIB) \
	libagent_ovs.la

gbp_inspect_CXXFLAGS = -DSYSCONFDIR='"$(sysconfdir)"'
gbp_inspect_SOURCES = \
	src/gbp_inspect.cpp
gbp_inspect_LDADD = \
	$(BOOST_IOSTREAMS_LIB) \
	$(BOOST_PROGRAM_OPTIONS_LIB) \
	libagent_ovs.la

mcast_daemon_CXXFLAGS = -DLOCALSTATEDIR='"$(localstatedir)"'
mcast_daemon_SOURCES = \
	src/mcast_daemon.cpp
mcast_daemon_LDADD = \
	$(BOOST_PROGRAM_OPTIONS_LIB) \
	libagent_ovs.la

TESTS = agent_test
noinst_PROGRAMS = $(TESTS) mock_server integration_test

agent_test_CFLAGS = \
	$(libopenvswitch_CFLAGS) \
	$(libofproto_CFLAGS)
agent_test_CXXFLAGS = \
	-I$(top_srcdir)/test/include \
	$(libopflex_CFLAGS) $(libmodelgbp_CFLAGS) \
	$(OVS_ADDL_CFLAGS) \
	$(libopenvswitch_CFLAGS) \
	$(libofproto_CFLAGS) \
	-DBOOST_TEST_DYN_LINK

agent_test_SOURCES = \
	test/include/BaseFixture.h \
	test/include/ModbFixture.h \
	test/include/MockSwitchConnection.h \
	test/include/MockPortMapper.h \
	test/include/MockFlowExecutor.h \
	test/include/MockFlowReader.h \
	test/include/MockSwitchManager.h \
	test/include/FlowManagerFixture.h \
	test/include/PolicyStatsManagerFixture.h \
	test/PolicyManager_test.cpp \
	test/EndpointManager_test.cpp \
	test/ModelEndpointSource_test.cpp \
	test/IdGenerator_test.cpp \
	test/KeyedRateLimiter_test.cpp \
	test/NotifServer_test.cpp \
	test/Network_test.cpp \
	test/main.cpp

if RENDERER_OVS
  agent_test_SOURCES += \
	test/MockFlowExecutor.cpp \
	test/FlowManagerFixture.cpp \
	test/IntFlowManager_test.cpp \
	test/AccessFlowManager_test.cpp \
	test/PacketInHandler_test.cpp \
	test/PacketInHandler_test_shim.c \
	test/AdvertManager_test.cpp \
	test/PortMapper_test.cpp \
	test/FlowExecutor_test.cpp \
	test/RangeMask_test.cpp \
	test/Packets_test.cpp \
	test/InterfaceStatsManager_test.cpp \
	test/ContractStatsManager_test.cpp \
	test/SecGrpStatsManager_test.cpp \
	test/TableState_test.cpp
endif

agent_test_LDADD = \
	$(BOOST_UNIT_TEST_FRAMEWORK_LIB) \
	libagent_ovs.la

mock_server_CXXFLAGS = \
	-I$(top_srcdir)/test/include \
	$(libopflex_CFLAGS) $(libmodelgbp_CFLAGS)
mock_server_SOURCES = \
	test/include/Policies.h \
	test/Policies.cpp \
	test/mock_server.cpp
mock_server_LDADD = \
	$(BOOST_PROGRAM_OPTIONS_LIB) \
	libagent_ovs.la

agentconfdir=$(sysconfdir)/opflex-agent-ovs
agentconf_DATA = opflex-agent-ovs.conf

flowidcachedir=${localstatedir}/lib/opflex-agent-ovs/ids
defepwatchdir=${localstatedir}/lib/opflex-agent-ovs/endpoints
defservwatchdir=${localstatedir}/lib/opflex-agent-ovs/services
mcastgroupfile=${localstatedir}/lib/opflex-agent-ovs/mcast/opflex-groups.json
inspectsock=${localstatedir}/run/opflex-agent-ovs-inspect.sock
notifsock=${localstatedir}/run/opflex-agent-ovs-notif.sock
cacertdir=${sysconfdir}/ssl/certs
clientcertpath=${agentconfdir}/opflex-agent-cert.pem
opflex-agent-ovs.conf: $(top_srcdir)/opflex-agent-ovs.conf.in
	sed -e "s|DEFAULT_FS_ENDPOINT_DIR|${defepwatchdir}|" \
	    -e "s|DEFAULT_FS_SERVICE_DIR|${defservwatchdir}|" \
	    -e "s|DEFAULT_FLOWID_CACHE_DIR|${flowidcachedir}|" \
	    -e "s|DEFAULT_MCAST_GROUP_FILE|${mcastgroupfile}|" \
	    -e "s|DEFAULT_INSPECTOR_SOCKET|${inspectsock}|" \
	    -e "s|DEFAULT_NOTIF_SOCKET|${notifsock}|" \
	    -e "s|DEFAULT_CA_CERT_DIR|${cacertdir}|" \
	    -e "s|DEFAULT_CLIENT_CERT_PATH|${clientcertpath}|" \
	$(top_srcdir)/opflex-agent-ovs.conf.in > opflex-agent-ovs.conf

if HAVE_DOXYGEN
  noinst_DATA = \
	doc/html
endif

integration_test_CXXFLAGS = $(BOOST_CPPFLAGS) -DBOOST_TEST_DYN_LINK \
	-I$(top_srcdir)/integration-test/include \
	$(OVS_ADDL_CFLAGS) \
	$(libopenvswitch_CFLAGS) \
	$(libofproto_CFLAGS)

integration_test_SOURCES = \
	integration-test/include/ConnectionFixture.h \
	integration-test/main.cpp \
	integration-test/ctzone_test.cpp

if RENDERER_OVS
  integration_test_SOURCES += \
	integration-test/connection_test.cpp \
	integration-test/flowmod_test.cpp
endif

integration_test_LDFLAGS = $(BOOST_LDFLAGS)
integration_test_LDADD = \
	$(BOOST_UNIT_TEST_FRAMEWORK_LIB) \
	libagent_ovs.la

check-integration: agent_ovs
	$(top_builddir)/integration_test
doc/html: $(model_include_HEADERS) $(noinst_HEADERS) doc/Doxyfile
	cd doc && ${DOXYGEN} Doxyfile
doc: doc/html

clean-doc:
	rm -rf doc/html doc/latex
clean-local: clean-doc
	rm -f *.rpm *.deb

EXTRA_DIST=rpm/agent-ovs.service rpm/mcast-daemon.service \
	rpm/90-agent-ovs-sysctl.conf opflex-agent-ovs.conf.in

CWD=`pwd`
RPMFLAGS=--define "_topdir ${CWD}/rpm"
ARCH=x86_64
SOURCE_FILE=${PACKAGE}-${VERSION}.tar.gz
RPMDIRS=rpm/BUILD rpm/SOURCES rpm/RPMS rpm/SRPMS
rpm: dist rpm/${PACKAGE}.spec
	mkdir -p ${RPMDIRS}
	cp ${SOURCE_FILE} rpm/SOURCES/
	rpmbuild ${RPMFLAGS} -ba rpm/${PACKAGE}.spec
	cp rpm/RPMS/${ARCH}/*.rpm .
	cp rpm/SRPMS/*.rpm .
	rm -rf ${RPMDIRS}
srpm: dist rpm/${PACKAGE}.spec
	mkdir -p ${RPMDIRS}
	cp ${SOURCE_FILE} rpm/SOURCES/
	rpmbuild ${RPMFLAGS} -bs rpm/${PACKAGE}.spec
	cp rpm/SRPMS/*.rpm .
	rm -rf ${RPMDIRS}

EXTRA_DIST += debian/agent-ovs.dirs debian/agent-ovs.install \
	debian/agent-ovs.postinst debian/agent-ovs.preinst \
	debian/agent-ovs.upstart debian/mcast-daemon.conf \
	debian/agent-ovs.service debian/mcast-daemon.service \
	debian/changelog debian/compat debian/conffiles debian/control \
	debian/copyright debian/rules debian/source/format

# Set env var DEB_BUILD_OPTIONS="parallel=<#cores>" to speed up package builds
DEB_PKG_DIR=deb-pkg-build
deb: dist
	- rm -rf  $(DEB_PKG_DIR)
	mkdir -p $(DEB_PKG_DIR)
	cp $(SOURCE_FILE) $(DEB_PKG_DIR)/
	tar -C $(DEB_PKG_DIR)/ -xf $(DEB_PKG_DIR)/$(SOURCE_FILE)
	mv $(DEB_PKG_DIR)/$(SOURCE_FILE) $(DEB_PKG_DIR)/$(PACKAGE)_$(VERSION).orig.tar.gz
	cd $(DEB_PKG_DIR)/$(PACKAGE)-$(VERSION)/; \
		dpkg-buildpackage -us -uc -rfakeroot -b
	cp $(DEB_PKG_DIR)/*.deb .
	rm -rf $(DEB_PKG_DIR)

dsc: dist
	- rm -rf  $(DEB_PKG_DIR)
	mkdir -p $(DEB_PKG_DIR)
	cp $(SOURCE_FILE) $(DEB_PKG_DIR)/
	tar -C $(DEB_PKG_DIR)/ -xf $(DEB_PKG_DIR)/$(SOURCE_FILE)
	mv $(DEB_PKG_DIR)/$(SOURCE_FILE) $(DEB_PKG_DIR)/$(PACKAGE)_$(VERSION).orig.tar.gz
	cd $(DEB_PKG_DIR)/$(PACKAGE)-$(VERSION)/; \
		dpkg-buildpackage -d -us -uc -rfakeroot -S
