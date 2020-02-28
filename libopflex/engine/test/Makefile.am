#
# libopflex: a framework for developing opflex-based policy agents
# Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v1.0 which accompanies this distribution,
# and is available at http://www.eclipse.org/legal/epl-v10.html
#
###########
#
# Process this file with automake to produce a Makefile.in

AM_CPPFLAGS = $(BOOST_CPPFLAGS) -DBOOST_TEST_DYN_LINK \
	-Wall \
	-Werror \
	-DSRCDIR="\"$(abs_top_srcdir)\"" \
	-I$(srcdir)/../include \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/logging/include \
	-I$(top_srcdir)/modb/include \
	-I$(top_srcdir)/modb/test \
	-I$(top_srcdir)/comms/include \
	-I$(top_srcdir)/util/include

if ENABLE_TSAN
  AM_CPPFLAGS += -fsanitize=thread
endif

if ENABLE_ASAN
  AM_CPPFLAGS += -fsanitize=address
endif

if ENABLE_COVERAGE
  AM_CPPFLAGS += --coverage
endif

AM_LDFLAGS = $(BOOST_LDFLAGS)

if ENABLE_TSAN
  AM_LDFLAGS += -fsanitize=thread
endif

if ENABLE_ASAN
  AM_LDFLAGS += -fsanitize=address
endif

if ENABLE_COVERAGE
  AM_LDFLAGS += --coverage
endif

TESTS = engine_test
engine_test_SOURCES = \
	main.cpp \
	MOSerialize_test.cpp \
	Processor_test.cpp \
	OpflexPool_test.cpp
engine_test_CXXFLAGS = $(UV_CFLAGS) $(RAPIDJSON_CFLAGS)
engine_test_LDADD = \
	../libengine.la \
	../../util/libutil.la \
	../../modb/libmodb.la \
	../../comms/libcomms.la \
	../../logging/liblogging.la \
	$(BOOST_UNIT_TEST_FRAMEWORK_LIB) \
        $(BOOST_ASIO_LIB) \
        $(BOOST_SYSTEM_LIB) \
        $(BOOST_FILESYSTEM_LIB)

if MAKE_ALL_TESTS
    noinst_PROGRAMS = $(TESTS)
else
    check_PROGRAMS = $(TESTS)
endif
