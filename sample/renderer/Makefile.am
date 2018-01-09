#
# renderer-sample: Sample OpFlex agent renderer plugin
# Copyright (c) 2018 Cisco Systems, Inc. and others.  All rights reserved.
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v1.0 which accompanies this distribution,
# and is available at http://www.eclipse.org/legal/epl-v10.html
#
###########
#
# Process this file with automake to produce a Makefile.in

ACLOCAL_AMFLAGS = -I m4

# Create a convenience library containing our plugin sources
noinst_LTLIBRARIES = librenderer_sample.la
librenderer_sample_la_CXXFLAGS = \
	$(libopflex_agent_CFLAGS)
librenderer_sample_la_LIBADD = \
	$(libopflex_agent_LIBS)
librenderer_sample_la_SOURCES = \
	SampleRenderer.h \
	SampleRenderer.cpp

# Link the convenience library into an installable module
lib_LTLIBRARIES = libopflex_agent_renderer_sample.la
libopflex_agent_renderer_sample_la_LIBADD = \
	librenderer_sample.la
libopflex_agent_renderer_sample_la_SOURCES =
libopflex_agent_renderer_sample_la_LDFLAGS = \
	-avoid-version -module -shared

# Install the default plugin configuration file
pluginconfdir=$(sysconfdir)/opflex-agent-ovs/plugins.conf.d
pluginconf_DATA = plugin-renderer-sample.conf
EXTRA_DIST=$(pluginconf_DATA)

# Create a unit test driver that links to the plugin convenience
# library
TESTS = sample_test
noinst_PROGRAMS = $(TESTS)
sample_test_CXXFLAGS = \
	$(libopflex_agent_CFLAGS) \
	-DBOOST_TEST_DYN_LINK
sample_test_LDADD = \
	$(libopflex_agent_LIBS) \
	$(BOOST_UNIT_TEST_FRAMEWORK_LIB) \
	librenderer_sample.la
sample_test_SOURCES = \
	sample_test.cpp \
	SampleRenderer_test.cpp

CWD=`pwd`
RPMFLAGS=--define "_topdir ${CWD}/rpm"
ARCH=x86_64
SOURCE_FILE=${PACKAGE}-${VERSION}.tar.gz
RPMDIRS=rpm/BUILD rpm/SOURCES rpm/RPMS rpm/SRPMS
srpm: dist rpm/opflex-agent-renderer-sample.spec
	mkdir -p ${RPMDIRS}
	cp ${SOURCE_FILE} rpm/SOURCES/
	rpmbuild ${RPMFLAGS} -bs rpm/opflex-agent-renderer-sample.spec
	cp rpm/SRPMS/*.rpm .
	rm -rf ${RPMDIRS}
