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

# only recurse for installing the headers, otherwise use non-recursive make
SUBDIRS = include

lib_LTLIBRARIES =
noinst_LTLIBRARIES  =
noinst_LTLIBRARIES += librpcperfect.la
noinst_LTLIBRARIES += libcommstrace.la
if SEPARATE_COMMS
  lib_LTLIBRARIES += libcomms.la
else
  noinst_LTLIBRARIES += libcomms.la
endif

AM_CPPFLAGS  = $(BOOST_CPPFLAGS)
AM_CPPFLAGS += -I$(srcdir)/include
AM_CPPFLAGS += -I$(top_srcdir)/include
AM_CPPFLAGS += -I$(top_srcdir)/logging/include
AM_CPPFLAGS += -I$(top_srcdir)/3rdparty/codes/trunk/codes.cpp

libcommstrace_la_SOURCES  =
libcommstrace_la_SOURCES += debug_trace.cpp

libcommstrace_la_CXXFLAGS  =

libcomms_la_CXXFLAGS  =
libcomms_la_CXXFLAGS += $(UV_CFLAGS)
if TRACING_COMMS
  libcomms_la_CXXFLAGS += -finstrument-functions
endif

libcomms_la_LIBADD  =
libcomms_la_LIBADD += librpcperfect.la
libcomms_la_LIBADD += $(UV_LIBS)

libcomms_la_SOURCES  =
libcomms_la_SOURCES += active_connection.cpp
libcomms_la_SOURCES += passive_listener.cpp
libcomms_la_SOURCES += common.cpp
libcomms_la_SOURCES += rpc.cpp
libcomms_la_SOURCES += peer.cpp

# inbound REQUESTs
libcomms_la_SOURCES += rpc/handlers/request/echo.cpp
libcomms_la_SOURCES += rpc/handlers/request/unknown.cpp

# inbound ERROR RESPONSEs
libcomms_la_SOURCES += rpc/handlers/response/error/echo.cpp
libcomms_la_SOURCES += rpc/handlers/response/error/unknown.cpp

# inbound SUCCESS RESPONSEs
libcomms_la_SOURCES += rpc/handlers/response/result/echo.cpp
libcomms_la_SOURCES += rpc/handlers/response/result/unknown.cpp

# message factory
libcomms_la_SOURCES     += rpc/message/factory/inboundmessage.cpp

librpcperfect_la_SOURCES  =
librpcperfect_la_SOURCES += rpc/message/factory/lookup_method.cpp
librpcperfect_la_SOURCES += rpc/message/factory/inboundrequest.cpp
librpcperfect_la_SOURCES += rpc/message/factory/inboundresult.cpp
librpcperfect_la_SOURCES += rpc/message/factory/inbounderror.cpp

librpcperfect_la_CXXFLAGS  = $(libcomms_la_CXXFLAGS)
librpcperfect_la_CXXFLAGS += $(OUR_CXXFLAGS_COMPILE_TIME_HASH)

if SEPARATE_COMMS
  libcomms_la_LIBADD += ../logging/liblogging.la
endif

# tests

TESTS = comms_test
comms_test_SOURCES  =
comms_test_SOURCES += test/main.cpp
comms_test_SOURCES += test/comms_test.cpp
comms_test_SOURCES += test/handlers.cpp

comms_test_CPPFLAGS  = $(AM_CPPFLAGS)
comms_test_CPPFLAGS += -DBOOST_TEST_DYN_LINK
comms_test_CPPFLAGS += $(UV_CFLAGS)

comms_test_LDFLAGS  = $(AM_LDFLAGS)
comms_test_LDFLAGS += $(BOOST_LDFLAGS)
comms_test_LDFLAGS += $(UV_LIBS)

comms_test_LDADD  =
comms_test_LDADD += libcomms.la
comms_test_LDADD += ../logging/liblogging.la
if TRACING_COMMS
  comms_test_LDADD += libcommstrace.la
endif
comms_test_LDADD += $(BOOST_UNIT_TEST_FRAMEWORK_LIB)

check_PROGRAMS = $(TESTS)
