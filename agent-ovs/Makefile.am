#
# opflex-agent: OpFlex agent for group-based policy
# Copyright (c) 2014-2018 Cisco Systems, Inc. and others.  All rights reserved.
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v1.0 which accompanies this distribution,
# and is available at http://www.eclipse.org/legal/epl-v10.html
#
###########
#
# Process this file with automake to produce a Makefile.in

ACLOCAL_AMFLAGS = -I m4

if HAVE_STATIC_BOOST
$(info Static boost enabled)
$(info BOOST_STATIC_LIB_DIR $(BOOST_STATIC_LIB_DIR))
USE_BOOST_SYSTEM_LIB=$(BOOST_STATIC_LIB_DIR)/libboost_system.a
USE_BOOST_FILESYSTEM_LIB=$(BOOST_STATIC_LIB_DIR)/libboost_filesystem.a
USE_BOOST_PROGRAM_OPTIONS_LIB=$(BOOST_STATIC_LIB_DIR)/libboost_program_options.a
USE_BOOST_UNIT_TEST_FRAMEWORK_LIB=$(BOOST_STATIC_LIB_DIR)/libboost_test_exec_monitor.a \
				  $(BOOST_STATIC_LIB_DIR)/libboost_unit_test_framework.a
USE_BOOST_IOSTREAMS_LIB=$(BOOST_STATIC_LIB_DIR)/libboost_iostreams.a
USE_TEST_FLAGS=-DBoost_USE_STATIC_LIBS=ON
RPM_BUILD_FLAGS=--without-dynboost
else
$(info Static boost disabled)
USE_BOOST_SYSTEM_LIB=$(BOOST_SYSTEM_LIB)
USE_BOOST_FILESYSTEM_LIB=$(BOOST_FILESYSTEM_LIB)
USE_BOOST_ASIO_LIB=$(BOOST_ASIO_LIB)
USE_BOOST_PROGRAM_OPTIONS_LIB=$(BOOST_PROGRAM_OPTIONS_LIB)
USE_BOOST_UNIT_TEST_FRAMEWORK_LIB=$(BOOST_UNIT_TEST_FRAMEWORK_LIB)
USE_BOOST_IOSTREAMS_LIB=$(BOOST_IOSTREAMS_LIB)
USE_TEST_FLAGS=-DBOOST_TEST_DYN_LINK
endif

OVS_ADDL_CFLAGS = \
	-D__STDC_LIMIT_MACROS \
	-D__STDC_CONSTANT_MACROS \
	-fpermissive

AM_CPPFLAGS = $(BOOST_CPPFLAGS) \
	-I$(top_srcdir)/ovs/include \
	-I$(top_srcdir)/lib/include \
	-DLOCALSTATEDIR='"$(localstatedir)"'

AM_LDFLAGS = $(BOOST_LDFLAGS)

noinst_LTLIBRARIES =
lib_LTLIBRARIES = libopflex_agent.la

libopflex_agent_la_CXXFLAGS = -pthread \
	$(libopflex_CFLAGS) \
	$(libmodelgbp_CFLAGS) \
	$(rapidjson_CFLAGS) \
	$(libnfct_CFLAGS)
libopflex_agent_la_LIBADD = \
	$(libopflex_LIBS) \
	$(libmodelgbp_LIBS) \
	$(libnfct_LIBS) \
	$(USE_BOOST_SYSTEM_LIB) \
	$(USE_BOOST_FILESYSTEM_LIB) \
	$(USE_BOOST_ASIO_LIB)

libopflex_agent_la_includedir = $(includedir)/opflexagent
libopflex_agent_la_include_HEADERS = \
	lib/include/opflexagent/AgentLogHandler.h \
	lib/include/opflexagent/PolicyListener.h \
	lib/include/opflexagent/PolicyManager.h \
	lib/include/opflexagent/FSWatcher.h \
	lib/include/opflexagent/Endpoint.h \
	lib/include/opflexagent/EndpointListener.h \
	lib/include/opflexagent/EndpointManager.h \
	lib/include/opflexagent/EndpointSource.h \
	lib/include/opflexagent/FSEndpointSource.h \
	lib/include/opflexagent/FSExternalEndpointSource.h \
	lib/include/opflexagent/ModelEndpointSource.h \
	lib/include/opflexagent/Service.h \
	lib/include/opflexagent/ServiceListener.h \
	lib/include/opflexagent/ServiceSource.h \
	lib/include/opflexagent/ServiceManager.h \
	lib/include/opflexagent/FSServiceSource.h \
	lib/include/opflexagent/LearningBridgeIface.h \
	lib/include/opflexagent/LearningBridgeListener.h \
	lib/include/opflexagent/LearningBridgeSource.h \
	lib/include/opflexagent/LearningBridgeManager.h \
	lib/include/opflexagent/FSLearningBridgeSource.h \
	lib/include/opflexagent/ExtraConfigListener.h \
	lib/include/opflexagent/ExtraConfigManager.h \
	lib/include/opflexagent/FSRDConfigSource.h \
	lib/include/opflexagent/RDConfig.h \
	lib/include/opflexagent/Renderer.h \
	lib/include/opflexagent/TunnelEpManager.h \
	lib/include/opflexagent/Agent.h \
	lib/include/opflexagent/IdGenerator.h \
	lib/include/opflexagent/KeyedRateLimiter.h \
	lib/include/opflexagent/MulticastListener.h \
	lib/include/opflexagent/TaskQueue.h \
	lib/include/opflexagent/NotifServer.h \
	lib/include/opflexagent/Network.h \
	lib/include/opflexagent/cmd.h \
	lib/include/opflexagent/logging.h \
	lib/include/opflexagent/SimStats.h \
	lib/include/opflexagent/SpanManager.h

noinst_HEADERS = \
	ovs/include/OVSRenderer.h \
	ovs/include/FlowExecutor.h \
	ovs/include/FlowReader.h \
	ovs/include/SwitchStateHandler.h \
	ovs/include/IntFlowManager.h \
	ovs/include/AccessFlowManager.h \
	ovs/include/ovs-shim.h \
	ovs/include/ovs-ofputil.h \
	ovs/include/ovs-ofpbuf.h \
	ovs/include/TableState.h \
	ovs/include/ActionBuilder.h \
	ovs/include/FlowBuilder.h \
	ovs/include/SwitchConnection.h \
	ovs/include/SwitchManager.h \
	ovs/include/PortMapper.h \
	ovs/include/InterfaceStatsManager.h \
	ovs/include/PolicyStatsManager.h \
	ovs/include/ContractStatsManager.h \
	ovs/include/SecGrpStatsManager.h \
	ovs/include/CtZoneManager.h \
	ovs/include/RangeMask.h \
	ovs/include/Packets.h \
	ovs/include/PacketInHandler.h \
	ovs/include/AdvertManager.h \
	ovs/include/FlowUtils.h \
	ovs/include/FlowConstants.h \
	ovs/include/udp.h \
	ovs/include/dhcp.h \
	ovs/include/arp.h \
	ovs/include/eth.h \
	ovs/include/SpanRenderer.h

libopflex_agent_la_SOURCES = \
	lib/AgentLogHandler.cpp \
	lib/PolicyManager.cpp \
	lib/FSWatcher.cpp \
	lib/Endpoint.cpp \
	lib/EndpointManager.cpp \
	lib/EndpointSource.cpp \
	lib/FSEndpointSource.cpp \
	lib/FSExternalEndpointSource.cpp \
	lib/ModelEndpointSource.cpp \
	lib/Service.cpp \
	lib/ServiceSource.cpp \
	lib/ServiceManager.cpp \
	lib/FSServiceSource.cpp \
	lib/LearningBridgeIface.cpp \
	lib/LearningBridgeSource.cpp \
	lib/LearningBridgeManager.cpp \
	lib/FSLearningBridgeSource.cpp \
	lib/ExtraConfigManager.cpp \
	lib/RDConfig.cpp \
	lib/FSRDConfigSource.cpp \
	lib/Renderer.cpp \
	lib/TunnelEpManager.cpp \
	lib/cmd.cpp \
	lib/logging.cpp \
	lib/Agent.cpp \
	lib/IdGenerator.cpp \
	lib/CtZoneManager.cpp \
	lib/NotifServer.cpp \
	lib/MulticastListener.cpp \
	lib/TaskQueue.cpp \
	lib/Network.cpp \
	lib/SimStats.cpp \
	lib/SpanManager.cpp
libopflex_agent_la_LDFLAGS = -shared -version-info ${VERSION_INFO}

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libopflex_agent.pc

if RENDERER_OVS
  lib_LTLIBRARIES += libopflex_agent_renderer_openvswitch.la
  libopflex_agent_renderer_openvswitch_la_SOURCES =
  noinst_LTLIBRARIES += librenderer_openvswitch.la
  librenderer_openvswitch_la_SOURCES = \
	ovs/OVSRenderer.cpp \
	ovs/SwitchStateHandler.cpp \
	ovs/IntFlowManager.cpp \
	ovs/AccessFlowManager.cpp \
	ovs/TableState.cpp \
	ovs/FlowExecutor.cpp \
	ovs/FlowReader.cpp \
	ovs/ActionBuilder.cpp \
	ovs/FlowBuilder.cpp \
	ovs/SwitchConnection.cpp \
	ovs/SwitchManager.cpp \
	ovs/PortMapper.cpp \
	ovs/PolicyStatsManager.cpp \
	ovs/InterfaceStatsManager.cpp \
	ovs/ContractStatsManager.cpp \
	ovs/SecGrpStatsManager.cpp \
	ovs/RangeMask.cpp \
	ovs/Packets.cpp \
	ovs/PacketInHandler.cpp \
	ovs/AdvertManager.cpp \
	ovs/FlowUtils.cpp \
	ovs/FlowConstants.cpp \
	ovs/ovs-shim.c \
	ovs/SpanRenderer.cpp
  librenderer_openvswitch_la_CFLAGS = \
	$(libopenvswitch_CFLAGS) \
	$(libofproto_CFLAGS)
  librenderer_openvswitch_la_CXXFLAGS = \
	$(libopflex_CFLAGS) \
	$(libmodelgbp_CFLAGS) \
	$(OVS_ADDL_CFLAGS) \
	$(libopenvswitch_CFLAGS) \
	$(libofproto_CFLAGS)
  librenderer_openvswitch_la_LIBADD = \
	$(libopflex_LIBS) \
	$(libmodelgbp_LIBS) \
	$(libopenvswitch_LIBS) \
	$(libofproto_LIBS) \
	$(USE_BOOST_SYSTEM_LIB) \
	$(USE_BOOST_FILESYSTEM_LIB) \
	$(USE_BOOST_ASIO_LIB)
  libopflex_agent_renderer_openvswitch_la_LIBADD = \
	librenderer_openvswitch.la
  libopflex_agent_renderer_openvswitch_la_LDFLAGS = \
	-avoid-version -module -shared
endif

bin_PROGRAMS=opflex_agent gbp_inspect mcast_daemon mock_server
opflex_agent_CXXFLAGS = -DSYSCONFDIR='"$(sysconfdir)"' \
	$(libopflex_CFLAGS) \
	$(libmodelgbp_CFLAGS)
opflex_agent_SOURCES = \
	cmd/opflex_agent.cpp
opflex_agent_LDADD = \
	$(libopflex_LIBS) \
	$(USE_BOOST_PROGRAM_OPTIONS_LIB) \
	$(USE_BOOST_FILESYSTEM_LIB) \
	$(USE_BOOST_SYSTEM_LIB) \
	libopflex_agent.la

gbp_inspect_CXXFLAGS = -DSYSCONFDIR='"$(sysconfdir)"'
gbp_inspect_SOURCES = \
	cmd/gbp_inspect.cpp
gbp_inspect_LDADD = \
	$(libopflex_LIBS) \
	$(libmodelgbp_LIBS) \
	$(USE_BOOST_IOSTREAMS_LIB) \
	$(USE_BOOST_PROGRAM_OPTIONS_LIB) \
	$(USE_BOOST_SYSTEM_LIB) \
	libopflex_agent.la

mcast_daemon_CXXFLAGS = -DLOCALSTATEDIR='"$(localstatedir)"'
mcast_daemon_SOURCES = \
	cmd/mcast_daemon.cpp
mcast_daemon_LDADD = \
	$(USE_BOOST_FILESYSTEM_LIB) \
	$(USE_BOOST_SYSTEM_LIB) \
	$(USE_BOOST_PROGRAM_OPTIONS_LIB) \
	libopflex_agent.la

TESTS = agent_test
noinst_PROGRAMS = $(TESTS) integration_test policy_repo_stress

agent_test_CFLAGS =
agent_test_CXXFLAGS = \
	-I$(top_srcdir)/lib/include \
	-I$(top_srcdir)/cmd/test/include \
	$(libopflex_CFLAGS) $(libmodelgbp_CFLAGS) \
	$(USE_TEST_FLAGS)

agent_test_includedir = $(includedir)/opflexagent/test
agent_test_include_HEADERS = \
	lib/include/opflexagent/test/BaseFixture.h \
	lib/include/opflexagent/test/ModbFixture.h

agent_test_SOURCES = \
	lib/test/PolicyManager_test.cpp \
	lib/test/PolicyManagerIvLeaf_test.cpp \
	lib/test/EndpointManager_test.cpp \
	lib/test/ModelEndpointSource_test.cpp \
	lib/test/LearningBridgeManager_test.cpp \
	lib/test/IdGenerator_test.cpp \
	lib/test/KeyedRateLimiter_test.cpp \
	lib/test/NotifServer_test.cpp \
	lib/test/Network_test.cpp \
	cmd/test/agent_test.cpp

agent_test_LDADD = \
	$(libopflex_LIBS) \
	$(libmodelgbp_LIBS) \
	$(USE_BOOST_FILESYSTEM_LIB) \
	$(USE_BOOST_SYSTEM_LIB) \
	$(USE_BOOST_UNIT_TEST_FRAMEWORK_LIB) \
	libopflex_agent.la

if RENDERER_OVS
  agent_test_CFLAGS += \
	-I$(top_srcdir)/ovs/test/include \
	$(librenderer_openvswitch_la_CFLAGS)
  agent_test_CXXFLAGS += \
	-I$(top_srcdir)/ovs/test/include \
	$(librenderer_openvswitch_la_CXXFLAGS)
  agent_test_LDADD += \
	librenderer_openvswitch.la
  agent_test_SOURCES += \
	ovs/test/include/MockSwitchConnection.h \
	ovs/test/include/MockPortMapper.h \
	ovs/test/include/MockFlowExecutor.h \
	ovs/test/include/MockFlowReader.h \
	ovs/test/include/MockSwitchManager.h \
	ovs/test/include/FlowManagerFixture.h \
	ovs/test/include/PolicyStatsManagerFixture.h \
	ovs/test/MockFlowExecutor.cpp \
	ovs/test/FlowManagerFixture.cpp \
	ovs/test/IntFlowManager_test.cpp \
	ovs/test/AccessFlowManager_test.cpp \
	ovs/test/PacketInHandler_test.cpp \
	ovs/test/AdvertManager_test.cpp \
	ovs/test/PortMapper_test.cpp \
	ovs/test/FlowExecutor_test.cpp \
	ovs/test/RangeMask_test.cpp \
	ovs/test/Packets_test.cpp \
	ovs/test/InterfaceStatsManager_test.cpp \
	ovs/test/ContractStatsManager_test.cpp \
	ovs/test/SecGrpStatsManager_test.cpp \
	ovs/test/TableState_test.cpp
endif

mock_server_CXXFLAGS = \
	-I$(top_srcdir)/cmd/test/include \
	$(libopflex_CFLAGS) $(libmodelgbp_CFLAGS)
mock_server_SOURCES = \
	cmd/test/include/Policies.h \
	cmd/test/Policies.cpp \
	cmd/test/mock_server.cpp
mock_server_LDADD = \
	$(libopflex_LIBS) \
	$(libmodelgbp_LIBS) \
	$(USE_BOOST_PROGRAM_OPTIONS_LIB) \
	$(USE_BOOST_FILESYSTEM_LIB) \
	$(USE_BOOST_SYSTEM_LIB) \
	libopflex_agent.la

policy_repo_stress_CXXFLAGS = \
	$(libopflex_CFLAGS) \
	$(libmodelgbp_CFLAGS)
policy_repo_stress_SOURCES = \
	cmd/test/policy_repo_stress.cpp
policy_repo_stress_LDADD = \
	$(libopflex_LIBS) \
	$(USE_BOOST_PROGRAM_OPTIONS_LIB) \
	$(USE_BOOST_FILESYSTEM_LIB) \
	$(USE_BOOST_SYSTEM_LIB) \
	libopflex_agent.la

agentconfdir=$(sysconfdir)/opflex-agent-ovs
agentconf_DATA = opflex-agent-ovs.conf
pluginconfdir=$(sysconfdir)/opflex-agent-ovs/plugins.conf.d
pluginconf_DATA = plugin-renderer-openvswitch.conf

defepwatchdir=${localstatedir}/lib/opflex-agent-ovs/endpoints
defservwatchdir=${localstatedir}/lib/opflex-agent-ovs/services
inspectsock=${localstatedir}/run/opflex-agent-inspect.sock
notifsock=${localstatedir}/run/opflex-agent-notif.sock
cacertdir=${sysconfdir}/ssl/certs
clientcertpath=${agentconfdir}/opflex-agent-cert.pem
opflex-agent-ovs.conf: $(top_srcdir)/opflex-agent-ovs.conf.in
	sed -e "s|DEFAULT_FS_ENDPOINT_DIR|${defepwatchdir}|" \
	    -e "s|DEFAULT_FS_SERVICE_DIR|${defservwatchdir}|" \
	    -e "s|DEFAULT_INSPECTOR_SOCKET|${inspectsock}|" \
	    -e "s|DEFAULT_NOTIF_SOCKET|${notifsock}|" \
	    -e "s|DEFAULT_CA_CERT_DIR|${cacertdir}|" \
	    -e "s|DEFAULT_CLIENT_CERT_PATH|${clientcertpath}|" \
	$< > $@

flowidcachedir=${localstatedir}/lib/opflex-agent-ovs/ids
mcastgroupfile=${localstatedir}/lib/opflex-agent-ovs/mcast/opflex-groups.json
plugin-renderer-openvswitch.conf: $(top_srcdir)/plugin-renderer-openvswitch.conf.in
	sed -e "s|DEFAULT_FLOWID_CACHE_DIR|${flowidcachedir}|" \
	    -e "s|DEFAULT_MCAST_GROUP_FILE|${mcastgroupfile}|" \
	$< > $@

if HAVE_DOXYGEN
  noinst_DATA = \
	doc/html
endif

integration_test_CXXFLAGS = \
	$(BOOST_CPPFLAGS) $(USE_TEST_FLAGS) \
	-I$(top_srcdir)/integration-test/include

integration_test_SOURCES = \
	lib/integration-test/ctzone_test.cpp \
	cmd/integration-test/integration_test.cpp

integration_test_LDFLAGS = $(BOOST_LDFLAGS)
integration_test_LDADD = \
	$(USE_BOOST_SYSTEM_LIB) \
	$(USE_BOOST_UNIT_TEST_FRAMEWORK_LIB) \
	libopflex_agent.la

if RENDERER_OVS
  integration_test_SOURCES += \
	ovs/integration-test/include/ConnectionFixture.h \
	ovs/integration-test/connection_test.cpp \
	ovs/integration-test/flowmod_test.cpp
  integration_test_CXXFLAGS += \
	-I$(top_srcdir)/ovs/integration-test/include \
	$(librenderer_openvswitch_la_CXXFLAGS)
  integration_test_LDADD += \
	$(libopenvswitch_LIBS) \
	$(libofproto_LIBS) \
	librenderer_openvswitch.la
endif

check-integration: integration_test
	$(top_builddir)/integration_test
doc/html: $(model_include_HEADERS) $(noinst_HEADERS) $(libopflex_agent_la_include_HEADERS) $(agent_test_include_HEADERS) doc/Doxyfile
	cd doc && ${DOXYGEN} Doxyfile
doc: doc/html

clean-doc:
	rm -rf doc/html doc/latex
clean-local: clean-doc
	rm -f *.rpm *.deb

EXTRA_DIST = \
	rpm/opflex-agent.service \
	rpm/opflex-agent-openvswitch.conf \
	rpm/mcast-daemon.service \
	rpm/90-opflex-agent-sysctl.conf \
	opflex-agent-ovs.conf.in plugin-renderer-openvswitch.conf.in

CWD=`pwd`
RPMFLAGS=--define "_topdir ${CWD}/rpm"
ARCH=x86_64
SOURCE_FILE=${PACKAGE}-${VERSION}.tar.gz
RPMDIRS=rpm/BUILD rpm/SOURCES rpm/RPMS rpm/SRPMS
rpm: dist rpm/${PACKAGE}.spec
	mkdir -p ${RPMDIRS}
	cp ${SOURCE_FILE} rpm/SOURCES/
	rpmbuild ${RPMFLAGS} ${RPM_BUILD_FLAGS} -ba rpm/${PACKAGE}.spec
	cp rpm/RPMS/${ARCH}/*.rpm .
	cp rpm/SRPMS/*.rpm .
	rm -rf ${RPMDIRS}
srpm: dist rpm/${PACKAGE}.spec
	mkdir -p ${RPMDIRS}
	cp ${SOURCE_FILE} rpm/SOURCES/
	rpmbuild ${RPMFLAGS} ${RPM_BUILD_FLAGS} -bs rpm/${PACKAGE}.spec
	cp rpm/SRPMS/*.rpm .
	rm -rf ${RPMDIRS}

EXTRA_DIST += debian/opflex-agent.dirs \
	debian/libopflex-agent.install \
	debian/libopflex-agent-dev.install \
	debian/opflex-agent.install \
	debian/opflex-agent-renderer-openvswitch.install \
	debian/libopflex-agent.shlibs \
	debian/opflex-agent.postinst debian/opflex-agent.preinst \
	debian/opflex-agent.upstart debian/mcast-daemon.conf \
	debian/opflex-agent.service debian/mcast-daemon.service \
	debian/10-opflex-agent-openvswitch.conf \
	debian/changelog debian/compat debian/control \
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
