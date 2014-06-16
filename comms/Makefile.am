# /*
#  * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
#  *
#  * This program and the accompanying materials are made available under the
#  * terms of the Eclipse Public License v1.0 which accompanies this distribution,
#  * and is available at http://www.eclipse.org/legal/epl-v10.html
#  */

lib_LTLIBRARIES = libcomms.la
METASOURCES = AUTO

AM_CFLAGS  =
AM_CFLAGS += -Wall
AM_CFLAGS += -O1
AM_CFLAGS += -fPIC
AM_CFLAGS += -DPIC
AM_CFLAGS += -ggdb3
AM_CFLAGS += -I.
AM_CFLAGS += -I$(top_builddir)/include
AM_CFLAGS += -I$(top_builddir)/third-party
AM_CFLAGS += -I$(top_builddir)/deps/libuv/include
AM_CFLAGS += -std=gnu99

AM_CPPFLAGS=-DBSD_COMP=1 

AM_LDFLAGS  =
AM_LDFLAGS += -L/usr/lib
AM_LDFLAGS += -lnsl
AM_LDFLAGS += -lm
AM_LDFLAGS += -lpthread
if HAVE_TCMALLOC
AM_LDFLAGS += -ltcmalloc
endif
AM_LDFLAGS += -L$(top_builddir)/deps/libuv/.libs -luv
AM_LDFLAGS += -L$(top_builddir)/common/.libs -lcommon.so

libcomms_la_SOURCES  =
libcomms_la_SOURCES += active_connection.c
libcomms_la_SOURCES += passive_listener.c
libcomms_la_SOURCES += common.c
libcomms_la_SOURCES += opflex-identity.c

lib_libcomms_la = lib/libcomms.la
