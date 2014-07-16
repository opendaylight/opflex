# /*
#  * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
#  *
#  * This program and the accompanying materials are made available under the
#  * terms of the Eclipse Public License v1.0 which accompanies this distribution,
#  * and is available at http://www.eclipse.org/legal/epl-v10.html
#  */

lib_LTLIBRARIES = libpeovs.so.la
METASOURCES = AUTO


AM_CFLAGS=-Wall -O0 -fPIC -DPIC -I -g -I. -I$(top_builddir)/include -I$(top_builddir)/third-party -I$(top_builddir)/lib
AM_CFLAGS += -I$(top_builddir)/third-party/ovs -I$(top_builddir)/third-party/ovs/lib -I$(top_builddir)/third-party/ovs/ovsdb
AM_CPPFLAGS=-DBSD_COMP=1 
if HAVE_TCMALLOC
AM_LDFLAGS=-L/usr/lib -lnsl -lm -lpthread -L$(top_builddir)/lib -L$(top_builddir)/third-party -ltcmalloc
else
AM_LDFLAGS=-L/usr/lib -lnsl -lm -lpthread -L$(top_builddir)/lib -L$(top_builddir)/third-party
endif

libpeovs_so_la_SOURCES = peovs_start.c peovs.h
libpeovs_so_la_SOURCES += peovs_commands.c
libpeovs_so_la_SOURCES += peovs_monitor.c peovs_monitor.h
libpeovs_so_la_SOURCES += peovs_ring_buffer.c peovs_ring_buffer.h
libpeovs_so_la_SOURCES += peovs_push.c
libpeovs_so_la_SOURCES += peovs_scan.c
libpeovs_so_la_SOURCES += peovs_translator.c
libpeovs_so_la_SOURCES += peovs_workers.c peovs_workers.h

lib_libpeovs_so_la = peovs/libpeovs.so.la
