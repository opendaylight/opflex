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

AM_CPPFLAGS = $(BOOST_CPPFLAGS) \
	-I$(top_srcdir)/src/include \
	-DBOOST_LOG_DYN_LINK
AM_LDFLAGS = $(BOOST_LDFLAGS)

noinst_LTLIBRARIES = libagent_ovs.la
libagent_ovs_la_CXXFLAGS = \
	$(libopflex_CFLAGS) \
	$(libmodelgbp_CFLAGS)
libagent_ovs_la_LIBADD = \
	$(libopflex_LIBS) \
	$(libmodelgbp_LIBS) \
	$(libvirt_LIBS) \
	$(BOOST_LOG_LIB) \
	$(BOOST_LOG_SETUP_LIB) \
	$(BOOST_SYSTEM_LIB) \
	$(BOOST_FILESYSTEM_LIB) \
	$(BOOST_THREAD_LIB) 

libagent_ovs_la_SOURCES = \
	src/include/BoostLogHandler.h \
	src/include/PolicyListener.h \
	src/include/PolicyManager.h \
	src/include/Endpoint.h \
	src/include/EndpointListener.h \
	src/include/EndpointManager.h \
	src/include/EndpointSource.h \
	src/include/FSEndpointSource.h \
	src/include/Agent.h \
	src/BoostLogHandler.cpp \
	src/PolicyManager.cpp \
	src/Endpoint.cpp \
	src/EndpointManager.cpp \
	src/EndpointSource.cpp \
	src/FSEndpointSource.cpp \
	src/Agent.cpp

if HAVE_OVS
libagent_ovs_la_CXXFLAGS += $(OVS_CFLAGS)

libagent_ovs_la_SOURCES += \
	src/include/FlowExecutor.h \
	src/include/FlowManager.h \
	src/include/ovs.h \
	src/include/TableState.h \
	src/include/ActionBuilder.h \
	src/include/SwitchConnection.h \
	src/include/PortMapper.h \
	src/FlowManager.cpp \
	src/TableState.cpp \
	src/FlowExecutor.cpp \
	src/ActionBuilder.cpp \
	src/SwitchConnection.cpp \
	src/PortMapper.cpp
endif

bin_PROGRAMS=agent_ovs
agent_ovs_CXXFLAGS = -DSYSCONFDIR='"$(sysconfdir)"'
agent_ovs_SOURCES = \
	src/main.cpp
agent_ovs_LDADD = \
	libagent_ovs.la \
	$(BOOST_PROGRAM_OPTIONS_LIB)

TESTS = agent_test
agent_test_CXXFLAGS = \
	-I$(top_srcdir)/test/include \
	$(libopflex_CFLAGS) $(libmodelgbp_CFLAGS) \
	-DBOOST_TEST_DYN_LINK
agent_test_SOURCES = \
	test/include/BaseFixture.h \
	test/main.cpp \
	test/PolicyManager_test.cpp \
	test/EndpointManager_test.cpp
agent_test_LDADD = \
	libagent_ovs.la \
	$(BOOST_UNIT_TEST_FRAMEWORK_LIB)

if HAVE_OVS
agent_test_CXXFLAGS += $(OVS_CFLAGS)

agent_test_SOURCES += \
	test/include/ModbFixture.h \
	test/FlowManager_test.cpp \
	test/PortMapper_test.cpp \
	test/FlowExecutor_test.cpp

agent_test_LDADD += $(OVS_LIBS)
endif

noinst_PROGRAMS = $(TESTS)

agentconfdir=$(sysconfdir)/opflex-agent-ovs
agentconf_DATA = opflex-agent-ovs.conf

defwatchdir=${localstatedir}/run/opflex-agent-ovs
opflex-agent-ovs.conf: $(top_srcdir)/opflex-agent-ovs.conf.in
	sed -e "s|FILESYSTEM_WATCH_DIR|${defwatchdir}|" \
	$(top_srcdir)/opflex-agent-ovs.conf.in > opflex-agent-ovs.conf

if HAVE_DOXYGEN
  noinst_DATA = \
	doc/html
endif

if HAVE_OVS
noinst_PROGRAMS += integration_test

integration_test_CXXFLAGS = $(BOOST_CPPFLAGS) -DBOOST_TEST_DYN_LINK \
	$(OVS_CFLAGS)
integration_test_LDFLAGS = $(BOOST_LDFLAGS)

integration_test_SOURCES = \
	integration-test/main.cpp \
	integration-test/connection_test.cpp

integration_test_LDADD = libagent_ovs.la \
	$(libopflex_LIBS) \
	$(BOOST_UNIT_TEST_FRAMEWORK_LIB) \
	$(BOOST_LOG_LIB) \
	$(BOOST_LOG_SETUP_LIB) \
	$(OVS_LIBS)

endif

check-integration: agent_ovs
	$(top_builddir)/integration_test
doc/html: $(model_include_HEADERS) doc/Doxyfile 
	cd doc && ${DOXYGEN} Doxyfile
doc: doc/html

clean-doc:
	rm -rf doc/html doc/latex
clean-local: clean-doc
	rm -f *.rpm
