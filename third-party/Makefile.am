# /* Copyright (c) 2014 Cisco Systems, Inc.
#  *
#  * Licensed under the Apache License, Version 2.0 (the "License");
#  * you may not use this file except in compliance with the License.
#  * You may obtain a copy of the License at:
#  *
#  *     http://www.apache.org/licenses/LICENSE-2.0
#  *
#  * Unless required by applicable law or agreed to in writing, software
#  * distributed under the License is distributed on an "AS IS" BASIS,
#  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  * See the License for the specific language governing permissions and
#  * limitations under the License.
#  */
lib_LTLIBRARIES = lib3party.so.la 
lib_LTLIBRARIES += libdbug.so.la 
METASOURCES = AUTO

psep = ":"

# run_python = PYTHONPATH=$(top_srcdir)/python$(psep)$$PYTHONPATH \
# 	PYTHONDONTWRITEBYTECODE=yes \
# 	$(PYTHON)

run_python = 

# Compiler Flags: 
# ===============
# -O0: Reduce compilation time and make debugging produce the expected results. 
#      This is the default, NOTE: Change to -O2 for final.
# -fPIC: If supported for the target machine, emit position-independent code,
#        suitable for dynamic linking and avoiding any limit on the size of 
#        the global offset table.
# -DPIC: Set the PIC ifdef variable.
# -Wall: This enables all the warnings about constructions that some users 
#        consider questionable, and that are easy to avoid (or modify to prevent
#        the warning), even in conjunction with macros. 
#  -Wall turns on the following warning flags:
#          -Waddress   
#          -Warray-bounds (only with -O2)  
#          -Wc++0x-compat  
#          -Wchar-subscripts  
#          -Wimplicit-int  
#          -Wimplicit-function-declaration  
#          -Wcomment  
#          -Wformat   
#          -Wmain (only for C/ObjC and unless -ffreestanding)  
#          -Wmissing-braces  
#          -Wnonnull  
#          -Wparentheses  
#          -Wpointer-sign  
#          -Wreorder   
#          -Wreturn-type  
#          -Wsequence-point  
#          -Wsign-compare (only in C++)  
#          -Wstrict-aliasing  
#          -Wstrict-overflow=1  
#          -Wswitch  
#          -Wtrigraphs  
#          -Wuninitialized  
#          -Wunknown-pragmas  
#          -Wunused-function  
#          -Wunused-label     
#          -Wunused-value     
#          -Wunused-variable  
#          -Wvolatile-register-var 
# -g: Produce debugging information in the operating system's native format
#     (stabs, COFF, XCOFF, or DWARF 2). GDB can work with this debugging information. 

AM_CFLAGS=-Wall -O0 -fPIC -DPIC -I -g -I. -I$(top_builddir)/include -I$(top_builddir)/third-party -I$(top_builddir)/lib
AM_CFLAGS += -std=gnu11
AM_CPPFLAGS=-DBSD_COMP=1 
if HAVE_TCMALLOC
AM_LDFLAGS=-L/usr/lib -lnsl -lm -lpthread -ltcmalloc
else
AM_LDFLAGS=-L/usr/lib -lnsl -lm -lpthread
endif

libdbug_so_la_SOURCES = dbug.c sanity.c

lib3party_so_la_SOURCES =  aes128.c aes128.h
lib3party_so_la_SOURCES += async-append.h
lib3party_so_la_SOURCES += byte-order.h
lib3party_so_la_SOURCES += bitmap.c bitmap.h
lib3party_so_la_SOURCES += bundle.c bundle.h
lib3party_so_la_SOURCES += byteq.c byteq.h
lib3party_so_la_SOURCES += classifier.c classifier.h
lib3party_so_la_SOURCES += command-line.c command-line.h
lib3party_so_la_SOURCES += compiler.h
lib3party_so_la_SOURCES += connectivity.c connectivity.h
lib3party_so_la_SOURCES += coverage.c coverage.h
lib3party_so_la_SOURCES += crc32c.c crc32.h
lib3party_so_la_SOURCES += csum.c csum.h
lib3party_so_la_SOURCES += daemon.c daemon.h
lib3party_so_la_SOURCES += dpif-netdev.c
lib3party_so_la_SOURCES += dpif-provider.h
lib3party_so_la_SOURCES += dpif.c dpif.h
lib3party_so_la_SOURCES += dummy.c dummy.h
lib3party_so_la_SOURCES += dynamic-string.c dynamic-string.h
lib3party_so_la_SOURCES += entropy.c entropy.h
lib3party_so_la_SOURCES += fat-rwlock.c fat-rwlock.h
lib3party_so_la_SOURCES += fatal-signal.c fatal-signal.h
lib3party_so_la_SOURCES += flow.c flow.h
lib3party_so_la_SOURCES += hash.c hash.h
lib3party_so_la_SOURCES += hindex.c hindex.h
lib3party_so_la_SOURCES += hmap.c hmap.h
lib3party_so_la_SOURCES += hmapx.c hmapx.h
lib3party_so_la_SOURCES += jhash.c jhash.h
lib3party_so_la_SOURCES += json.c json.h
lib3party_so_la_SOURCES += jsonrpc.c jsonrpc.h
lib3party_so_la_SOURCES += latch-unix.c latch.h
lib3party_so_la_SOURCES += learn.c learn.h
lib3party_so_la_SOURCES += list.c list.h
lib3party_so_la_SOURCES += lockfile.c lockfile.h
lib3party_so_la_SOURCES += match.c match.h
lib3party_so_la_SOURCES += meta-flow.c meta-flow.h
lib3party_so_la_SOURCES += multipath.c multipath.h
lib3party_so_la_SOURCES += netdev.c netdev.h
lib3party_so_la_SOURCES += netdev-dummy.c
lib3party_so_la_SOURCES += netdev-provider.h
lib3party_so_la_SOURCES += netdev-vport.c netdev-vport.h
lib3party_so_la_SOURCES += netlink.c netlink.h
lib3party_so_la_SOURCES += netlink-notifier.c netlink-notifier.h
lib3party_so_la_SOURCES += netlink-socket.c netlink-socket.h
lib3party_so_la_SOURCES += nx-match.c nx-match.h
lib3party_so_la_SOURCES += odp-execute.c odp-execute.h
lib3party_so_la_SOURCES += odp-util.c odp-util.h
lib3party_so_la_SOURCES += ofpbuf.c ofpbuf.h
lib3party_so_la_SOURCES += ofp-actions.c ofp-actions.h
lib3party_so_la_SOURCES += ofp-errors.c ofp-errors.h 
lib3party_so_la_SOURCES += ofp-msgs.c ofp-msgs.h
lib3party_so_la_SOURCES += ofp-parse.c ofp-parse.h
lib3party_so_la_SOURCES += ofp-print.c ofp-print.h
lib3party_so_la_SOURCES += ofp-util.c ofp-util.def ofp-util.h
lib3party_so_la_SOURCES += ofp-version-opt.c
lib3party_so_la_SOURCES += packets.c packets.h
lib3party_so_la_SOURCES += pag-atomic.h
lib3party_so_la_SOURCES += pag-atomic-pthreads.c pag-atomic-pthreads.h
lib3party_so_la_SOURCES += pag-atomic-flag-gcc4.7+.h pag-atomic-gcc4.7+.h
lib3party_so_la_SOURCES += pag_dispatcher.c pag_dispatcher.h 
lib3party_so_la_SOURCES += pag_selector.c pag_selector.h 
lib3party_so_la_SOURCES += pag-thread.c pag-thread.h
lib3party_so_la_SOURCES += pag_tpool.c pag_tpool.h 
lib3party_so_la_SOURCES += pcap-file.c pcap-file.h
lib3party_so_la_SOURCES += poll-loop.c poll-loop.h
lib3party_so_la_SOURCES += process.c process.h
lib3party_so_la_SOURCES += random.c random.h
lib3party_so_la_SOURCES += reconnect.c reconnect.h
lib3party_so_la_SOURCES += route-table.c route-table.h
lib3party_so_la_SOURCES += rtnetlink-link.c rtnetlink-link.h
lib3party_so_la_SOURCES += sat-math.h
lib3party_so_la_SOURCES += signals.c signals.h
lib3party_so_la_SOURCES += seq.c seq.h
lib3party_so_la_SOURCES += sha1.c sha1.h
lib3party_so_la_SOURCES += shash.c shash.h
lib3party_so_la_SOURCES += simap.c simap.h
lib3party_so_la_SOURCES += smap.c smap.h
lib3party_so_la_SOURCES += socket-util.c socket-util.h
lib3party_so_la_SOURCES += sset.c sset.h
lib3party_so_la_SOURCES += stream-fd.c stream-fd.h
lib3party_so_la_SOURCES += stream-provider.h
lib3party_so_la_SOURCES += stream-ssl.h
lib3party_so_la_SOURCES += stream-tcp.c
lib3party_so_la_SOURCES += stream-unix.c
lib3party_so_la_SOURCES += stream.c stream.h
lib3party_so_la_SOURCES += svec.c svec.h
lib3party_so_la_SOURCES += tag.c tag.h
lib3party_so_la_SOURCES += timer.c timer.h
lib3party_so_la_SOURCES += timeval.c timeval.h
lib3party_so_la_SOURCES += token-bucket.c token-bucket.h
lib3party_so_la_SOURCES += type-props.h
lib3party_so_la_SOURCES += unaligned.h
lib3party_so_la_SOURCES += unicode.c unicode.h
lib3party_so_la_SOURCES += unixctl.c unixctl.h
lib3party_so_la_SOURCES += util.c util.h
lib3party_so_la_SOURCES += uuid.c uuid.h
lib3party_so_la_SOURCES += valgrind.h
lib3party_so_la_SOURCES += vlandev.c vlandev.h
lib3party_so_la_SOURCES += vconn.c vconn.h vconn-provider.h vconn-stream.c
lib3party_so_la_SOURCES += vlog.c vlog.h 

if HAVE_OPENSSL
lib3party_so_la_SOURCES += stream-ssl.c 
else
lib3party_so_la_SOURCES += stream-nossl.c 
endif

if HAVE_POSIX_AIO
lib3party_so_la_SOURCES += async-append-aio.c
else
lib3party_so_la_SOURCES += async-append-null.c
endif
$(srcdir)/third-party/ofp-errors.inc: \
	ofp-errors.h 
	$(run_python) $(builddir)/../scripts/extract-ofp-errors \
		$(abs_builddir)/ofp-errors.h \
		$(abs_builddir)/../include/openflow/openflow-common.h > ../$@.tmp
	mv ../$@.tmp ../$@
ofp-errors.c: $(srcdir)/third-party/ofp-errors.inc
lib3party_so_la_SOURCES += ofp-errors.inc

$(srcdir)/third-party/ofp-msgs.inc: \
	ofp-msgs.h 
	$(run_python) $(builddir)/../scripts/extract-ofp-msgs \
		$(abs_builddir)/ofp-msgs.h ../$@ > ../$@.tmp && mv ../$@.tmp ../$@
ofp-msgs.c: $(srcdir)/third-party/ofp-msgs.inc
lib3party_so_la_SOURCES += ofp-msgs.inc

lib_lib3party_so_la = third-party/lib3party.so.la

lib_libdbug_so_la = third-party/libdbug.so.la

clean-local:
	rm -f *.inc
