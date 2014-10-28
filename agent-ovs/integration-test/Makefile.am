# Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v1.0 which accompanies this distribution,
# and is available at http://www.eclipse.org/legal/epl-v10.html
#
###########
#
# Process this file with automake to produce a Makefile.in

noinst_PROGRAMS = integration_test

AM_CPPFLAGS = $(BOOST_CPPFLAGS) -DBOOST_TEST_DYN_LINK \
    -I$(top_srcdir)/include \
    -I$(top_srcdir)/src/enforcer/include \
    -I/usr/include/ovs/include \
    -fpermissive
AM_LDFLAGS = $(BOOST_LDFLAGS)

integration_test_SOURCES = \
    main.cpp \
    connection_test.cpp

integration_test_LDADD = $(top_builddir)/src/enforcer/libenforcer.la \
    $(libopflex_LIBS) \
    $(BOOST_UNIT_TEST_FRAMEWORK_LIB) \
    $(OVS_LIB) \
    $(UV_LIBS) \
    $(libglog_LIBS)

