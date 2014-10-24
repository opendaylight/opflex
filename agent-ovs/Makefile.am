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
	-I$(top_srcdir)/src/include
AM_LDFLAGS = $(BOOST_LDFLAGS)

noinst_LTLIBRARIES = libagent_ovs.la
libagent_ovs_la_CXXFLAGS = \
	$(libopflex_CFLAGS) \
	$(libmodelgbp_CFLAGS) \
	$(libvirt_CFLAGS) \
	$(libglog_CFLAGS)
libagent_ovs_la_LIBADD = \
	$(libopflex_LIBS) \
	$(libmodelgbp_LIBS) \
	$(libvirt_LIBS) \
	$(libglog_LIBS) \
	$(BOOST_SYSTEM_LIB) \
	$(BOOST_THREAD_LIB) 

libagent_ovs_la_SOURCES = \
	src/include/GLogLogHandler.h \
	src/include/PolicyManager.h \
	src/include/EndpointManager.h \
	src/include/EndpointSource.h \
	src/include/VirtEndpointSource.h \
	src/include/Agent.h \
	src/GLogLogHandler.cpp \
	src/PolicyManager.cpp \
	src/EndpointManager.cpp \
	src/EndpointSource.cpp \
	src/VirtEndpointSource.cpp \
	src/Agent.cpp

SUBDIRS =
if HAVE_OVS
SUBDIRS += src/enforcer
endif

bin_PROGRAMS=agent_ovs
agent_ovs_CXXFLAGS = -DSYSCONFDIR='"$(sysconfdir)"'
agent_ovs_SOURCES = \
	src/main.cpp
agent_ovs_LDADD = \
	libagent_ovs.la \
	$(BOOST_PROGRAM_OPTIONS_LIB)

TESTS = agent_test
noinst_PROGRAMS = $(TESTS)
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

if HAVE_DOXYGEN
  noinst_DATA = \
	doc/html
endif

doc/html: $(model_include_HEADERS) doc/Doxyfile 
	cd doc && ${DOXYGEN} Doxyfile
doc: doc/html

clean-doc:
	rm -rf doc/html doc/latex
clean-local: clean-doc
	rm -f *.rpm
