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
        -Wall \
        -Werror \
        -I$(srcdir)/include -I$(top_srcdir)/include

if ENABLE_TSAN
  AM_CPPFLAGS += -fsanitize=thread
endif

if ENABLE_ASAN
  AM_CPPFLAGS += -fsanitize=address
endif

if ENABLE_COVERAGE
  AM_CPPFLAGS += --coverage
endif

noinst_LTLIBRARIES = libcwrapper.la

libcwrapper_la_SOURCES = \
	ofloghandler.cpp \
	ofobjectlistener.cpp \
	ofuri.cpp \
	ofmutator.cpp \
	offramework.cpp \
	ofpeerstatuslistener.cpp
