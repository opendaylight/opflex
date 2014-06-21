# /*
#  * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
#  *
#  * This program and the accompanying materials are made available under the
#  * terms of the Eclipse Public License v1.0 which accompanies this distribution,
#  * and is available at http://www.eclipse.org/legal/epl-v10.html
#  */

lib_LTLIBRARIES = libpm.so.la
METASOURCES = AUTO


AM_CFLAGS=-Wall -O0 -fPIC -DPIC -I -g -I. -I$(top_builddir)/include -I$(top_builddir)/third-party -I$(top_builddir)/lib
AM_CPPFLAGS=-DBSD_COMP=1 
if HAVE_TCMALLOC
AM_LDFLAGS=-L/usr/lib -lnsl -lm -lpthread -L$(top_builddir)/lib -L$(top_builddir)/third-party -ltcmalloc
else
AM_LDFLAGS=-L/usr/lib -lnsl -lm -lpthread -L$(top_builddir)/lib -L$(top_builddir)/third-party
endif

libpm_so_la_SOURCES = pol-mgmt.c pol-mgmt.h
# libpm_so_la_SOURCES += index-util.c index-util.h

lib_libpm_so_la = policy-mgmt/libpm.so.la
