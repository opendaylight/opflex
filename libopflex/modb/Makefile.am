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
	-I$(top_srcdir)/logging/include \
	-I$(top_srcdir)/util/include 

noinst_LTLIBRARIES = libmodb.la

libmodb_la_CXXFLAGS = $(UV_CFLAGS)
libmodb_la_LIBADD = $(UV_LIBS)
libmodb_la_SOURCES = \
	include/opflex/modb/internal/ObjectStore.h \
	include/opflex/modb/internal/Region.h \
	include/opflex/modb/internal/URIQueue.h \
	include/opflex/modb/internal/ClassIndex.h \
	URI.cpp \
	URIBuilder.cpp \
	URIQueue.cpp \
	PropertyInfo.cpp \
	ClassInfo.cpp \
	ModelMetadata.cpp \
	ClassIndex.cpp \
	Mutator.cpp \
	Region.cpp \
	ObjectInstance.cpp \
	ObjectStore.cpp \
	StoreClient.cpp
