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
#lib_LTLIBRARIES = lib3party.so.la 
#lib_LTLIBRARIES += libdbug.so.la 
lib_LTLIBRARIES = libdbug.so.la 
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
AM_CFLAGS += -I$(top_builddir)/third-party/ovs/lib -I$(top_builddir)/third-party/ovs/ovsdb
AM_CPPFLAGS=-DBSD_COMP=1 
if HAVE_TCMALLOC
AM_LDFLAGS=-L/usr/lib -lnsl -lm -lpthread -ltcmalloc
else
AM_LDFLAGS=-L/usr/lib -lnsl -lm -lpthread
endif


libdbug_so_la_SOURCES = dbug.c sanity.c

$(srcdir)/third-party/ofp-errors.inc: \
	ofp-errors.h 
	$(run_python) $(builddir)/../scripts/extract-ofp-errors \
		$(abs_builddir)/ofp-errors.h \
		$(abs_builddir)/../include/openflow/openflow-common.h > ../$@.tmp
	mv ../$@.tmp ../$@
ofp-errors.c: $(srcdir)/third-party/ofp-errors.inc

$(srcdir)/third-party/ofp-msgs.inc: \
	ofp-msgs.h 
	$(run_python) $(builddir)/../scripts/extract-ofp-msgs \
		$(abs_builddir)/ofp-msgs.h ../$@ > ../$@.tmp && mv ../$@.tmp ../$@
ofp-msgs.c: $(srcdir)/third-party/ofp-msgs.inc

lib_libdbug_so_la = third-party/libdbug.so.la
#lib_lib3party_so_la = third-party/lib3party.so.la

clean-local:
	rm -f *.inc
