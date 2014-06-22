# /*
#  * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
#  *
#  * This program and the accompanying materials are made available under the
#  * terms of the Eclipse Public License v1.0 which accompanies this distribution,
#  * and is available at http://www.eclipse.org/legal/epl-v10.html
#  */

bin_PROGRAMS = pagentd
METASOURCES = AUTO

pagentd_SOURCES = pagentd.c pagent.h
pagentd_SOURCES += pagentd_cli.c pagentd_cli.h

# set the include path found by configure

pagentd_CFLAGS=-g -O0 -Wall -DUSE_TEST_CONFIG -I/usr/local/include -I$(top_builddir)/include -I$(top_builddir)/third-party -I$(top_builddir)/modb -I$(top_builddir)/lib -I$(top_builddir)/policy_enforcer -I$(top_builddir)/policy-mgmt -I$(srcdir)
pagentd_CFLAGS += -I$(top_builddir)/third-party/ovs/lib -I$(top_builddir)/third-party/ovs/ovsdb
pagentd_CPPFLAGS=-DBSD_COMP=1 

# the library search path.
# pagentd_LDFLAGS = $(all_libraries)
pagentd_LDFLAGS = -L/usr/lib -L$(srcdir)/../lib -lssl -lcrypto -lrt -lreadline -lnsl -lm -lpthread $(top_builddir)/third-party/ovs/lib/libopenvswitch.la $(top_builddir)/third-party/ovs/ovsdb/libovsdb.la
if HAVE_TCMALLOC
pagentd_LDFLAGS += -ltcmalloc
endif

pagentd_LDADD = $(top_builddir)/third-party/libdbug.so.la $(top_builddir)/lib/libpag.so.la  $(top_builddir)/modb/libmodb.so.la $(top_builddir)/policy_enforcer/libpe.so.la $(top_builddir)/policy-mgmt/libpm.so.la

bin_pagentd = $(srcdir)/pagentd
