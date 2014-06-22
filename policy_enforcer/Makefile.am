# /*
#  * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
#  *
#  * This program and the accompanying materials are made available under the
#  * terms of the Eclipse Public License v1.0 which accompanies this distribution,
#  * and is available at http://www.eclipse.org/legal/epl-v10.html
#  */

lib_LTLIBRARIES = libpe.so.la
METASOURCES = AUTO


AM_CFLAGS=-Wall -O0 -fPIC -DPIC -I -g -I. -I$(top_builddir)/include -I$(top_builddir)/third-party -I$(top_builddir)/lib
AM_CFLAGS += -I$(top_builddir)/third-party/ovs/lib -I$(top_builddir)/third-party/ovs/ovsdb
AM_CPPFLAGS=-DBSD_COMP=1 
if HAVE_TCMALLOC
AM_LDFLAGS=-L/usr/lib -lnsl -lm -lpthread -L$(top_builddir)/lib -L$(top_builddir)/third-party -ltcmalloc
else
AM_LDFLAGS=-L/usr/lib -lnsl -lm -lpthread -L$(top_builddir)/lib -L$(top_builddir)/third-party
endif

libpe_so_la_SOURCES = pe_start.c policy_enforcer.h
libpe_so_la_SOURCES += pe_workers.c pe_push.c pe_translator.c
libpe_so_la_SOURCES += pe_monitor.c pe_scan.c

lib_libpe_so_la = policy_enforcer/libpe.so.la
