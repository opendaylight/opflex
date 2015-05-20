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

SUBDIRS = . test

AM_CPPFLAGS = $(BOOST_CPPFLAGS) \
        -I$(srcdir)/include -I$(top_srcdir)/include \
	-I$(top_srcdir)/modb/include \
	-I$(top_srcdir)/util/include \
	-I$(top_srcdir)/logging/include \
	-I$(top_srcdir)/engine/include \
	-I$(top_srcdir)/comms/include \
	-I$(top_srcdir)/comms \
	-DSDK_PVERSION='@SDK_PVERSION@' \
	-DSDK_SVERSION='@SDK_SVERSION@' \
	-DSDK_IVERSION='@SDK_IVERSION@' \
	-DSDK_BVERSION='"@SDK_BVERSION@"' \
	-DSDK_FULL_VERSION='"@SDK_FULL_VERSION@"'

noinst_LTLIBRARIES = libcore.la

libcore_la_CXXFLAGS = $(UV_CFLAGS) $(RAPIDJSON_CFLAGS)
libcore_la_LIBADD = $(UV_LIBS)
libcore_la_SOURCES = \
	OFFramework.cpp \
	MO.cpp
