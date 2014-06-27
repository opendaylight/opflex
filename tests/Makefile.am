# /*
#  * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
#  *
#  * This program and the accompanying materials are made available under the
#  * terms of the Eclipse Public License v1.0 which accompanies this distribution,
#  * and is available at http://www.eclipse.org/legal/epl-v10.html
#  */

bin_PROGRAMS = test-modb
bin_PROGRAMS += test-uri-parser
bin_PROGRAMS += test-eventq
bin_PROGRAMS += test-ring-buffer
bin_PROGRAMS += test-pe_workers
#bin_PROGRAMS += test-pe_monitor
bin_PROGRAMS += test-pipe-utils
bin_PROGRAMS += test-pol-mgmt
METASOURCES = AUTO

test_modb_SOURCES = test-modb.c
test_ring_buffer_SOURCES = test-ring-buffer.c
test_pe_workers_SOURCES = test-pe_workers.c
#test_pe_monitor_SOURCES = test-pe_monitor.c
test_pipe_utils_SOURCES = test-pipe-utils.c
test_uri_parser_SOURCES = test-uri-parser.c
test_pol_mgmt_SOURCES = test-pol-mgmt.c
test_eventq_SOURCES = test-eventq.c


# set the include path found by configure

AM_CFLAGS=-g -O0 -Wall -DUSE_TEST_CONFIG -I/usr/local/include -I$(top_builddir)/include -I$(top_builddir)/third-party -I$(top_builddir)/modb -I$(top_builddir)/lib -I$(srcdir) -I$(top_builddir)/policy_enforcer -I$(top_builddir)/policy-mgmt
AM_CFLAGS += -I$(top_builddir)/third-party/ovs/lib -I$(top_builddir)/third-party/ovs/ovsdb
AM_CPPFLAGS=-DBSD_COMP=1 

ovs_libs = $(top_builddir)/third-party/ovs/ovsdb/libovsdb.la $(top_builddir)/third-party/ovs/lib/libopenvswitch.la

# the library search path.
# pagentd_LDFLAGS = $(all_libraries)
test_modb_LDFLAGS = -L/usr/lib -L$(srcdir)/../lib -lssl -lcrypto -lrt -lreadline -lnsl -lm -lpthread -lcmocka
test_modb_LDFLAGS += $(ovs_libs) $(ovs_libs)
if HAVE_TCMALLOC
test_modb_LDFLAGS += -ltcmalloc
endif 
test_ring_buffer_LFLAGS= $(test_modb_LDFLAGS)
test_pe_workers_LFLAGS= $(test_modb_LDFLAGS)
test_pipe_utils_LFLAGS= $(test_modb_LDFLAGS)
test_uri_parser_LFLAGS= $(test_modb_LDFLAGS)
test_pol_mgmt_LFLAGS= $(test_modb_LDFLAGS)
test_eventq_LFLAGS= $(test_modb_LDFLAGS)

test_modb_LDADD = $(top_builddir)/third-party/libdbug.so.la $(ovs_libs) $(top_builddir)/lib/libpag.so.la $(top_builddir)/modb/libmodb.so.la $(ovs_libs) 
test_ring_buffer_LDADD = $(top_builddir)/third-party/libdbug.so.la $(top_builddir)/lib/libpag.so.la $(ovs_libs)
test_pe_workers_LDADD = $(top_builddir)/third-party/libdbug.so.la $(top_builddir)/lib/libpag.so.la $(top_builddir)/policy_enforcer/libpe.so.la $(ovs_libs)
#test_pe_monitor_LDADD = $(top_builddir)/third-party/libdbug.so.la $(top_builddir)/lib/libpag.so.la $(top_builddir)/policy_enforcer/libpe.so.la $(ovs_libs)
test_pipe_utils_LDADD = $(top_builddir)/third-party/libdbug.so.la $(top_builddir)/lib/libpag.so.la $(ovs_libs)
test_uri_parser_LDADD = $(top_builddir)/third-party/libdbug.so.la $(top_builddir)/lib/libpag.so.la $(ovs_libs)
test_pol_mgmt_LDADD = $(top_builddir)/third-party/libdbug.so.la $(top_builddir)/lib/libpag.so.la $(top_builddir)/policy-mgmt/libpm.so.la $(ovs_libs)

test_eventq_LDADD = $(test_uri_parser_LDADD)

bin_test_modb = $(srcdir)/tests
bin_test_ring_buffer = $(srcdir)/tests
bin_test_pe_workers = $(srcdir)/tests
#bin_test_pe_monitor = $(srcdir)/tests
bin_test_pipe_utils = $(srcdir)/tests
bin_test_uri_parser = $(srcdir)/tests
bin_test_eventq = $(srcdir)/tests
bin_test_pol_mgmt = $(srcdir)/tests
