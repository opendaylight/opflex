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
if SEPARATE_COMMS
  lib_LTLIBRARIES += libcomms.la
else
  noinst_LTLIBRARIES += libcomms.la
endif

AM_CPPFLAGS  =
AM_CPPFLAGS += $(BOOST_CPPFLAGS)
AM_CPPFLAGS += -I$(top_builddir)
AM_CPPFLAGS += -I$(srcdir)/include
AM_CPPFLAGS += -I$(top_srcdir)/include
AM_CPPFLAGS += -I$(top_srcdir)/logging/include
AM_CPPFLAGS += $(OPENSSL_CFLAGS) -DYAJR_HAS_OPENSSL

libcomms_la_CPPFLAGS  = $(AM_CPPFLAGS)

libcomms_la_CXXFLAGS  = $(AM_CXXFLAGS)
libcomms_la_CXXFLAGS += $(UV_CFLAGS)
libcomms_la_CXXFLAGS += $(RAPIDJSON_CFLAGS)

libcomms_la_CPPFLAGS += -DBOOST_EXCEPTION_DISABLE
libcomms_la_CXXFLAGS += -fno-exceptions

libcomms_la_LIBADD  =
libcomms_la_LIBADD += librpcperfect.la
libcomms_la_LIBADD += $(UV_LIBS)
libcomms_la_LIBADD += $(OPENSSL_LIBS)

libcomms_la_SOURCES  =
libcomms_la_SOURCES += active_connection.cpp
libcomms_la_SOURCES += passive_listener.cpp
libcomms_la_SOURCES += common.cpp
libcomms_la_SOURCES += transport/PlainText.cpp
libcomms_la_SOURCES += transport/ZeroCopyOpenSSL.cpp
libcomms_la_SOURCES += rpc.cpp
libcomms_la_SOURCES += peer.cpp
libcomms_la_SOURCES += ActivePeer.cpp
libcomms_la_SOURCES += ActiveTcpPeer.cpp
libcomms_la_SOURCES += ActiveUnixPeer.cpp
libcomms_la_SOURCES += CommunicationPeer.cpp
libcomms_la_SOURCES += ListeningPeer.cpp
libcomms_la_SOURCES += loopdata.cpp

# inbound REQUESTs
libcomms_la_SOURCES += rpc/handlers/request/echo.cpp
libcomms_la_SOURCES += rpc/handlers/request/unknown.cpp

# inbound ERROR RESPONSEs
libcomms_la_SOURCES += rpc/handlers/error_response/echo.cpp
libcomms_la_SOURCES += rpc/handlers/error_response/unknown.cpp

# inbound SUCCESS RESPONSEs
libcomms_la_SOURCES += rpc/handlers/result_response/echo.cpp
libcomms_la_SOURCES += rpc/handlers/result_response/unknown.cpp

# message factory
libcomms_la_SOURCES     += rpc/message/factory/inboundmessage.cpp

librpcperfect_la_SOURCES  =
librpcperfect_la_SOURCES += rpc/message/factory/lookup_method.cpp
librpcperfect_la_SOURCES += rpc/message/factory/inboundrequest.cpp
librpcperfect_la_SOURCES += rpc/message/factory/inboundresult.cpp
librpcperfect_la_SOURCES += rpc/message/factory/inbounderror.cpp

librpcperfect_la_CXXFLAGS  = $(libcomms_la_CXXFLAGS)
librpcperfect_la_CXXFLAGS += $(OUR_CXXFLAGS_COMPILE_TIME_HASH)

librpcperfect_la_CPPFLAGS  = $(libcomms_la_CPPFLAGS)

if SEPARATE_COMMS
  libcomms_la_LIBADD += ../logging/liblogging.la
endif

# tests

TESTS                =
TESTS               += test/stable_tests.sh
if MAKE_ALL_TESTS
    noinst_PROGRAMS  = comms_test
else
    check_PROGRAMS   = comms_test
endif
dist_noinst_SCRIPTS  =
dist_noinst_SCRIPTS += test/stable_tests.sh
dist_noinst_SCRIPTS += test/server.pem
dist_noinst_SCRIPTS += test/ca.pem

comms_test_SOURCES  =
comms_test_SOURCES += test/main.cpp
comms_test_SOURCES += test/comms_test.cpp

comms_test_SOURCES += test/handlers/error_response/custom.cpp
comms_test_SOURCES += test/handlers/error_response/endpoint_declare.cpp
comms_test_SOURCES += test/handlers/error_response/endpoint_resolve.cpp
comms_test_SOURCES += test/handlers/error_response/endpoint_undeclare.cpp
comms_test_SOURCES += test/handlers/error_response/endpoint_unresolve.cpp
comms_test_SOURCES += test/handlers/error_response/endpoint_update.cpp
comms_test_SOURCES += test/handlers/error_response/policy_resolve.cpp
comms_test_SOURCES += test/handlers/error_response/policy_unresolve.cpp
comms_test_SOURCES += test/handlers/error_response/policy_update.cpp
comms_test_SOURCES += test/handlers/error_response/send_identity.cpp
comms_test_SOURCES += test/handlers/error_response/state_report.cpp
comms_test_SOURCES += test/handlers/error_response/transact.cpp
comms_test_SOURCES += test/handlers/request/custom.cpp
comms_test_SOURCES += test/handlers/request/endpoint_declare.cpp
comms_test_SOURCES += test/handlers/request/endpoint_resolve.cpp
comms_test_SOURCES += test/handlers/request/endpoint_undeclare.cpp
comms_test_SOURCES += test/handlers/request/endpoint_unresolve.cpp
comms_test_SOURCES += test/handlers/request/endpoint_update.cpp
comms_test_SOURCES += test/handlers/request/policy_resolve.cpp
comms_test_SOURCES += test/handlers/request/policy_unresolve.cpp
comms_test_SOURCES += test/handlers/request/policy_update.cpp
comms_test_SOURCES += test/handlers/request/send_identity.cpp
comms_test_SOURCES += test/handlers/request/state_report.cpp
comms_test_SOURCES += test/handlers/request/transact.cpp
comms_test_SOURCES += test/handlers/result_response/custom.cpp
comms_test_SOURCES += test/handlers/result_response/endpoint_declare.cpp
comms_test_SOURCES += test/handlers/result_response/endpoint_resolve.cpp
comms_test_SOURCES += test/handlers/result_response/endpoint_undeclare.cpp
comms_test_SOURCES += test/handlers/result_response/endpoint_unresolve.cpp
comms_test_SOURCES += test/handlers/result_response/endpoint_update.cpp
comms_test_SOURCES += test/handlers/result_response/policy_resolve.cpp
comms_test_SOURCES += test/handlers/result_response/policy_unresolve.cpp
comms_test_SOURCES += test/handlers/result_response/policy_update.cpp
comms_test_SOURCES += test/handlers/result_response/send_identity.cpp
comms_test_SOURCES += test/handlers/result_response/state_report.cpp
comms_test_SOURCES += test/handlers/result_response/transact.cpp

comms_test_CPPFLAGS  = $(AM_CPPFLAGS)
comms_test_CPPFLAGS += -DBOOST_TEST_DYN_LINK
comms_test_CPPFLAGS += -DSRCDIR="\"$(abs_srcdir)\""
comms_test_CPPFLAGS += $(UV_CFLAGS)
comms_test_CPPFLAGS += $(RAPIDJSON_CFLAGS)
comms_test_CPPFLAGS += -DYAJR_HAS_OPENSSL
comms_test_CPPFLAGS += $(OPENSSL_CFLAGS)

comms_test_CXXFLAGS  = $(AM_CXXFLAGS)

comms_test_LDFLAGS  = $(AM_LDFLAGS)
comms_test_LDFLAGS += $(UV_LIBS)
comms_test_LDFLAGS += $(OPENSSL_LIBS)

comms_test_LDADD  =
comms_test_LDADD += libcomms.la
comms_test_LDADD += ../logging/liblogging.la
comms_test_LDADD += $(BOOST_UNIT_TEST_FRAMEWORK_LIB)

EXTRA_DIST=test/server.pem test/ca.pem
