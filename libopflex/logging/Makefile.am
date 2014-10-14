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

noinst_LTLIBRARIES  =
noinst_LTLIBRARIES += liblogging.la

AM_CPPFLAGS = \
	-I$(srcdir)/include \
	-I$(top_srcdir)/include

liblogging_la_SOURCES = \
	include/opflex/logging/internal/logging.hpp \
	OFLogHandler.cpp \
	StdOutLogHandler.cpp \
	logging.cpp

