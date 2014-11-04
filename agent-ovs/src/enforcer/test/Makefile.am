#
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
    -I$(top_srcdir)/include \
    -I$(top_srcdir)/src/enforcer \
    -I$(top_srcdir)/src/enforcer/include \
    $(OVS_CFLAGS)
AM_LDFLAGS = $(BOOST_LDFLAGS)

TESTS = enforcer_test
enforcer_test_SOURCES = \
    mockModb.cpp \
    modbFixture.h \
    main.cpp \
    inventory_test.cpp \
    renderer_test.cpp \
    portmapper_test.cpp

enforcer_test_LDADD = ../libenforcer.la \
    $(libopflex_LIBS) \
    $(BOOST_UNIT_TEST_FRAMEWORK_LIB) \
    $(OVS_LIBS) \
    $(UV_LIBS) \
    $(libglog_LIBS)

noinst_PROGRAMS = $(TESTS)
