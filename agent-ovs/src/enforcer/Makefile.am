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
	-DBOOST_LOG_DYN_LINK \
	-I$(srcdir)/include \
	-I$(top_srcdir)/src/include \
	$(OVS_CFLAGS)

noinst_LTLIBRARIES = libenforcer.la

libenforcer_la_CXXFLAGS = $(UV_CFLAGS)

libenforcer_la_LIBADD = \
	$(BOOST_LOG_LIB) \
	$(BOOST_LOG_SETUP_LIB) \
	$(BOOST_SYSTEM_LIB) \
	$(BOOST_THREAD_LIB)

libenforcer_la_SOURCES = \
	include/flowExecutor.h \
	include/flowManager.h \
	include/ovs.h \
	include/flow/tableState.h \
	include/flow/actionBuilder.h \
	include/internal/model.h \
	include/internal/modb.h \
	include/switchConnection.h \
	include/portMapper.h \
	flowManager.cpp \
	tableState.cpp \
	flowExecutor.cpp \
	actionBuilder.cpp \
	switchConnection.cpp \
	portMapper.cpp
