#------------------------------------------------------------------
# automake.mk
#
# May 2014, Abilash Menon
#
# Copyright (c) 2013-2014 by Cisco Systems, Inc.
#
# All rights reserved.
#------------------------------------------------------------------

##############################################
## Setup the OVS lib/ code as a static library
noinst_LIBRARIES += lib_libovs.a

lib_libovs_a_LIBADD = $(SSL_LIBS)

lib_libovs_a_SOURCES = \
	$(OPENVSWITCH_SRCDIR)/lib/util.h \
	$(OPENVSWITCH_SRCDIR)/lib/util.c \
	$(OPENVSWITCH_SRCDIR)/lib/vlog.h \
	$(OPENVSWITCH_SRCDIR)/lib/vlog.c \
	$(OPENVSWITCH_SRCDIR)/lib/route-table-stub.c \
	$(OPENVSWITCH_SRCDIR)/lib/byte-order.h \
	$(OPENVSWITCH_SRCDIR)/lib/cfm.h \
	$(OPENVSWITCH_SRCDIR)/lib/cfm.c \
	$(OPENVSWITCH_SRCDIR)/lib/classifier.h \
	$(OPENVSWITCH_SRCDIR)/lib/classifier.c \
	$(OPENVSWITCH_SRCDIR)/lib/command-line.h \
	$(OPENVSWITCH_SRCDIR)/lib/command-line.c \
	$(OPENVSWITCH_SRCDIR)/lib/compiler.h \
	$(OPENVSWITCH_SRCDIR)/lib/daemon.h \
	$(OPENVSWITCH_SRCDIR)/lib/daemon.c \
	$(OPENVSWITCH_SRCDIR)/lib/dirs.h \
	$(OPENVSWITCH_SRCDIR)/lib/dirs.c \
	$(OPENVSWITCH_SRCDIR)/lib/dynamic-string.h \
	$(OPENVSWITCH_SRCDIR)/lib/dynamic-string.c \
	$(OPENVSWITCH_SRCDIR)/lib/fat-rwlock.h \
	$(OPENVSWITCH_SRCDIR)/lib/fat-rwlock.c \
	$(OPENVSWITCH_SRCDIR)/lib/flow.h \
	$(OPENVSWITCH_SRCDIR)/lib/flow.c \
	$(OPENVSWITCH_SRCDIR)/lib/hash.h \
	$(OPENVSWITCH_SRCDIR)/lib/hash.c \
	$(OPENVSWITCH_SRCDIR)/lib/hindex.h \
	$(OPENVSWITCH_SRCDIR)/lib/hindex.c \
	$(OPENVSWITCH_SRCDIR)/lib/hmap.h \
	$(OPENVSWITCH_SRCDIR)/lib/hmap.c \
	$(OPENVSWITCH_SRCDIR)/lib/list.h \
	$(OPENVSWITCH_SRCDIR)/lib/list.c \
	$(OPENVSWITCH_SRCDIR)/lib/match.h \
	$(OPENVSWITCH_SRCDIR)/lib/match.c \
	$(OPENVSWITCH_SRCDIR)/lib/meta-flow.h \
	$(OPENVSWITCH_SRCDIR)/lib/meta-flow.c \
	$(OPENVSWITCH_SRCDIR)/lib/netdev.h \
	$(OPENVSWITCH_SRCDIR)/lib/netdev.c \
	$(OPENVSWITCH_SRCDIR)/lib/nx-match.h \
	$(OPENVSWITCH_SRCDIR)/lib/nx-match.c \
	$(OPENVSWITCH_SRCDIR)/lib/odp-util.h \
	$(OPENVSWITCH_SRCDIR)/lib/odp-util.c \
	$(OPENVSWITCH_SRCDIR)/lib/ofp-actions.h \
	$(OPENVSWITCH_SRCDIR)/lib/ofp-actions.c \
	$(OPENVSWITCH_SRCDIR)/lib/ofp-errors.h \
	$(OPENVSWITCH_SRCDIR)/lib/ofp-errors.c \
	$(OPENVSWITCH_SRCDIR)/lib/ofp-msgs.h \
	$(OPENVSWITCH_SRCDIR)/lib/ofp-msgs.c \
	$(OPENVSWITCH_SRCDIR)/lib/ofp-print.h \
	$(OPENVSWITCH_SRCDIR)/lib/ofp-print.c \
	$(OPENVSWITCH_SRCDIR)/lib/ofp-util.h \
	$(OPENVSWITCH_SRCDIR)/lib/ofp-util.c \
	$(OPENVSWITCH_SRCDIR)/lib/ofp-version-opt.h \
	$(OPENVSWITCH_SRCDIR)/lib/ofp-version-opt.c \
	$(OPENVSWITCH_SRCDIR)/lib/ofpbuf.h \
	$(OPENVSWITCH_SRCDIR)/lib/ofpbuf.c \
	$(OPENVSWITCH_SRCDIR)/lib/ovs-atomic-pthreads.h \
	$(OPENVSWITCH_SRCDIR)/lib/ovs-atomic-pthreads.c \
	$(OPENVSWITCH_SRCDIR)/lib/ovs-atomic.h \
	$(OPENVSWITCH_SRCDIR)/lib/ovs-thread.h \
	$(OPENVSWITCH_SRCDIR)/lib/ovs-thread.c \
	$(OPENVSWITCH_SRCDIR)/lib/packets.h \
	$(OPENVSWITCH_SRCDIR)/lib/packets.c \
	$(OPENVSWITCH_SRCDIR)/lib/pcap-file.h \
	$(OPENVSWITCH_SRCDIR)/lib/unixctl.h \
	$(OPENVSWITCH_SRCDIR)/lib/unixctl.c \
	$(OPENVSWITCH_SRCDIR)/lib/connectivity.h \
	$(OPENVSWITCH_SRCDIR)/lib/connectivity.c \
	$(OPENVSWITCH_SRCDIR)/lib/seq.h \
	$(OPENVSWITCH_SRCDIR)/lib/seq.c \
	$(OPENVSWITCH_SRCDIR)/lib/timer.h \
	$(OPENVSWITCH_SRCDIR)/lib/timer.c \
	$(OPENVSWITCH_SRCDIR)/lib/fatal-signal.h \
	$(OPENVSWITCH_SRCDIR)/lib/fatal-signal.c \
	$(OPENVSWITCH_SRCDIR)/lib/lockfile.h \
	$(OPENVSWITCH_SRCDIR)/lib/lockfile.c \
	$(OPENVSWITCH_SRCDIR)/lib/process.h \
	$(OPENVSWITCH_SRCDIR)/lib/process.c \
	$(OPENVSWITCH_SRCDIR)/lib/coverage.h \
	$(OPENVSWITCH_SRCDIR)/lib/coverage.c \
	$(OPENVSWITCH_SRCDIR)/lib/jhash.h \
	$(OPENVSWITCH_SRCDIR)/lib/jhash.c \
	$(OPENVSWITCH_SRCDIR)/lib/shash.h \
	$(OPENVSWITCH_SRCDIR)/lib/shash.c \
	$(OPENVSWITCH_SRCDIR)/lib/unaligned.h \
	$(OPENVSWITCH_SRCDIR)/lib/smap.h \
	$(OPENVSWITCH_SRCDIR)/lib/smap.c \
	$(OPENVSWITCH_SRCDIR)/lib/dpfi.h \
	$(OPENVSWITCH_SRCDIR)/lib/dpif.c \
	$(OPENVSWITCH_SRCDIR)/lib/dpif-provider.h \
	$(OPENVSWITCH_SRCDIR)/lib/netdev-vport.h \
	$(OPENVSWITCH_SRCDIR)/lib/netdev-vport.c \
	$(OPENVSWITCH_SRCDIR)/lib/svec.h \
	$(OPENVSWITCH_SRCDIR)/lib/svec.c \
	$(OPENVSWITCH_SRCDIR)/lib/netlink.h \
	$(OPENVSWITCH_SRCDIR)/lib/netlink.c \
	$(OPENVSWITCH_SRCDIR)/lib/netlink-protocol.h \
	$(OPENVSWITCH_SRCDIR)/lib/simap.h \
	$(OPENVSWITCH_SRCDIR)/lib/simap.c \
	$(OPENVSWITCH_SRCDIR)/lib/bundle.h \
	$(OPENVSWITCH_SRCDIR)/lib/bundle.c \
	$(OPENVSWITCH_SRCDIR)/lib/learn.h \
	$(OPENVSWITCH_SRCDIR)/lib/learn.c \
	$(OPENVSWITCH_SRCDIR)/lib/multipath.h \
	$(OPENVSWITCH_SRCDIR)/lib/multipath.c \
	$(OPENVSWITCH_SRCDIR)/lib/crc32c.h \
	$(OPENVSWITCH_SRCDIR)/lib/crc32c.c \
	$(OPENVSWITCH_SRCDIR)/lib/json.h \
	$(OPENVSWITCH_SRCDIR)/lib/json.c \
	$(OPENVSWITCH_SRCDIR)/lib/jsonrpc.h \
	$(OPENVSWITCH_SRCDIR)/lib/jsonrpc.c \
	$(OPENVSWITCH_SRCDIR)/lib/stream.h \
	$(OPENVSWITCH_SRCDIR)/lib/stream.c \
	$(OPENVSWITCH_SRCDIR)/lib/latch.h \
	$(OPENVSWITCH_SRCDIR)/lib/latch.c \
	$(OPENVSWITCH_SRCDIR)/lib/signals.h \
	$(OPENVSWITCH_SRCDIR)/lib/signals.c \
	$(OPENVSWITCH_SRCDIR)/lib/odp-execute.c \
	$(OPENVSWITCH_SRCDIR)/lib/odp-execute.h \
	$(OPENVSWITCH_SRCDIR)/lib/socket-util.h \
	$(OPENVSWITCH_SRCDIR)/lib/socket-util.c \
	$(OPENVSWITCH_SRCDIR)/lib/timeval.c \
	$(OPENVSWITCH_SRCDIR)/lib/random.c \
	$(OPENVSWITCH_SRCDIR)/lib/sset.c \
	$(OPENVSWITCH_SRCDIR)/lib/reconnect.c \
	$(OPENVSWITCH_SRCDIR)/lib/entropy.c \
	$(OPENVSWITCH_SRCDIR)/lib/poll-loop.c \
	$(OPENVSWITCH_SRCDIR)/lib/byteq.c \
	$(OPENVSWITCH_SRCDIR)/lib/stream-tcp.c \
	$(OPENVSWITCH_SRCDIR)/lib/csum.c \
	$(OPENVSWITCH_SRCDIR)/lib/stream-ssl.c \
	$(OPENVSWITCH_SRCDIR)/lib/vconn.c \
	$(OPENVSWITCH_SRCDIR)/lib/tag.c \
	$(OPENVSWITCH_SRCDIR)/lib/stream-fd.c \
	$(OPENVSWITCH_SRCDIR)/lib/ofp-parse.c \
	$(OPENVSWITCH_SRCDIR)/lib/pcap-file.c \
	$(OPENVSWITCH_SRCDIR)/lib/async-append-aio.c \
	$(OPENVSWITCH_SRCDIR)/lib/bitmap.c \
	$(OPENVSWITCH_SRCDIR)/lib/token-bucket.c \
	$(OPENVSWITCH_SRCDIR)/lib/unicode.c \
	$(OPENVSWITCH_SRCDIR)/lib/vconn-stream.c \
	$(OPENVSWITCH_SRCDIR)/lib/stream-unix.c \
	$(OPENVSWITCH_SRCDIR)/lib/dpif-netdev.c \
	$(OPENVSWITCH_SRCDIR)/lib/dhparams.c

nodist_lib_libovs_a_SOURCES = \
        $(OPENVSWITCH_SRCDIR)/lib/dirs.c

$(OPENVSWITCH_SRCDIR)/lib/dirs.c: $(OPENVSWITCH_SRCDIR)/lib/dirs.c.in Makefile
        ($(ro_c) && sed < $(OPENVSWITCH_SRCDIR)/lib/dirs.c.in \
                -e 's,[@]srcdir[@],$(srcdir),g' \
                -e 's,[@]LOGDIR[@],"$(LOGDIR)",g' \
                -e 's,[@]RUNDIR[@],"$(RUNDIR)",g' \
                -e 's,[@]DBDIR[@],"$(DBDIR)",g' \
                -e 's,[@]bindir[@],"$(bindir)",g' \
                -e 's,[@]sysconfdir[@],"$(sysconfdir)",g' \
                -e 's,[@]pkgdatadir[@],"$(pkgdatadir)",g') \
	    > $(OPENVSWITCH_SRCDIR)/lib/dirs.c.tmp
	mv $(OPENVSWITCH_SRCDIR)/lib/dirs.c.tmp \
		$(OPENVSWITCH_SRCDIR)/lib/dirs.c

$(OPENVSWITCH_SRCDIR)/lib/ofp-errors.inc: \
        lib/ofp-errors.h include/openflow/openflow-common.h \
        $(OPENVSWITCH_SRCDIR)/build-aux/extract-ofp-errors
	$(run_python) $(OPENVSWITCH_SRCDIR)/build-aux/extract-ofp-errors \
		$(OPENVSWITCH_SRCDIR)/lib/ofp-errors.h \
		$(OPENVSWITCH_SRCDIR)/include/openflow/openflow-common.h > $@.tmp
	mv $@.tmp $@
$(OPENVSWITCH_SRCDIR)/lib/ofp-errors.c: $(OPENVSWITCH_SRCDIR)/lib/ofp-errors.inc
EXTRA_DIST += build-aux/extract-ofp-errors lib/ofp-errors.inc

$(srcdir)/lib/ofp-msgs.inc: \
        lib/ofp-msgs.h $(OPENVSWITCH_SRCDIR)/build-aux/extract-ofp-msgs
	$(run_python) $(OPENVSWITCH_SRCDIR)/build-aux/extract-ofp-msgs \
		$(OPENVSWITCH_SRCDIR)/lib/ofp-msgs.h $@ > $@.tmp && mv $@.tmp $@
$(OPENVSWITCH_SRCDIR)/lib/ofp-msgs.c: $(OPENVSWITCH_SRCDIR)/lib/ofp-msgs.inc
EXTRA_DIST += build-aux/extract-ofp-msgs lib/ofp-msgs.inc
