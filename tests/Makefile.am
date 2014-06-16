# /*
#  * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
#  *
#  * This program and the accompanying materials are made available under the
#  * terms of the Eclipse Public License v1.0 which accompanies this distribution,
#  * and is available at http://www.eclipse.org/legal/epl-v10.html
#  */

bin_PROGRAMS = test-modb
bin_PROGRAMS += test-uri-parser
# bin_PROGRAMS += test_pag_selector
# bin_PROGRAMS += test_pag_dispatcher
bin_PROGRAMS += test-eventq
bin_PROGRAMS += test-comms
bin_PROGRAMS += test-ring-buffer
bin_PROGRAMS += test-pe_workers
bin_PROGRAMS += test-pipe-utils
METASOURCES = AUTO

test_comms_SOURCES = test-comms.c
test_modb_SOURCES = test-modb.c
test_ring_buffer_SOURCES = test-ring-buffer.c
test_pe_workers_SOURCES = test-pe_workers.c
test_pipe_utils_SOURCES = test-pipe-utils.c
test_uri_parser = test-uri-parser.c
# test_pag_selector = test_pag_selector.c
# test_pag_dispatcher = test_pag_dispatcher.c
test_eventq = test-eventq.c


# set the include path found by configure

CFLAGS=-g -O0 -Wall -DUSE_TEST_CONFIG -I/usr/local/include -I$(top_builddir)/include -I$(top_builddir)/third-party -I$(top_builddir)/modb -I$(top_builddir)/lib -I$(srcdir) -I$(top_builddir)/policy_enforcer
CFLAGS += -std=gnu11
CFLAGS += -I$(top_builddir)/deps/libuv/include
CPPFLAGS=-DBSD_COMP=1 

# the library search path.
# pagentd_LDFLAGS = $(all_libraries)
LDFLAGS = -L/usr/lib -L$(srcdir)/../lib -lssl -lcrypto -lrt -lreadline -lnsl -lm -lpthread -lcmocka
if HAVE_TCMALLOC
LDFLAGS += -ltcmalloc
endif 

test_comms_LDADD = $(top_builddir)/third-party/libdbug.so.la $(top_builddir)/third-party/lib3party.so.la $(top_builddir)/lib/libpag.so.la $(top_builddir)/modb/libmodb.so.la $(top_builddir)/comms/libcomms.la $(top_builddir)/deps/libuv/.libs/libuv.so
test_modb_LDADD = $(top_builddir)/third-party/libdbug.so.la $(top_builddir)/third-party/lib3party.so.la $(top_builddir)/lib/libpag.so.la $(top_builddir)/modb/libmodb.so.la 
test_ring_buffer_LDADD = $(top_builddir)/third-party/libdbug.so.la $(top_builddir)/third-party/lib3party.so.la $(top_builddir)/lib/libpag.so.la 
test_pe_workers_LDADD = $(top_builddir)/third-party/libdbug.so.la $(top_builddir)/third-party/lib3party.so.la $(top_builddir)/lib/libpag.so.la $(top_builddir)/policy_enforcer/libpe.so.la
test_pipe_utils_LDADD = $(top_builddir)/third-party/libdbug.so.la $(top_builddir)/third-party/lib3party.so.la $(top_builddir)/lib/libpag.so.la 
test_uri_parser_LDADD = $(top_builddir)/third-party/libdbug.so.la $(top_builddir)/third-party/lib3party.so.la $(top_builddir)/lib/libpag.so.la
# test_pag_selector_LDADD = $(top_builddir)/third-party/libdbug.so.la $(top_builddir)/third-party/lib3party.so.la $(top_builddir)/lib/libpag.so.la
# test_pag_dispatcher_LDADD = $(top_builddir)/third-party/libdbug.so.la $(top_builddir)/third-party/lib3party.so.la $(top_builddir)/lib/libpag.so.la 

test_eventq_LDADD = $(test_uri_parser_LDADD)

bin_test_comms = $(srcdir)/tests
bin_test_modb = $(srcdir)/tests
bin_test_ring_buffer = $(srcdir)/tests
bin_test_pe_workers = $(srcdir)/tests
bin_test_pipe_utils = $(srcdir)/tests
bin_test_uri_parser = $(srcdir)/tests
# bin_test_pag_selector = $(srcdir)/tests
# bin_test_pag_dispatcher = $(srcdir)/tests
bin_test_eventq = $(srcdir)/tests
