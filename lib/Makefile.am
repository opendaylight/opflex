# /*
#  * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
#  *
#  * This program and the accompanying materials are made available under the
#  * terms of the Eclipse Public License v1.0 which accompanies this distribution,
#  * and is available at http://www.eclipse.org/legal/epl-v10.html
#  */

lib_LTLIBRARIES = libpag.so.la
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

AM_CFLAGS=-Wall -O0 -fPIC -DPIC -I -g -I. -I$(top_builddir)/include -I$(top_builddir)/third-party
AM_CFLAGS += -std=gnu11
AM_CPPFLAGS=-DBSD_COMP=1 
if HAVE_TCMALLOC
AM_LDFLAGS=-L/usr/lib -lnsl -lm -lpthread -ltcmalloc
else
AM_LDFLAGS=-L/usr/lib -lnsl -lm -lpthread
endif

libpag_so_la_SOURCES = access.c access.h
libpag_so_la_SOURCES += ansi-setup.h
libpag_so_la_SOURCES += cnf-dictionary.c cnf-dictionary.h
libpag_so_la_SOURCES += cnf-parser.c cnf-parser.h
libpag_so_la_SOURCES += config-file.c config-file.h
libpag_so_la_SOURCES += debug.c debug.h
libpag_so_la_SOURCES += dirs.c dirs.h
libpag_so_la_SOURCES += eventq.c eventq.h
libpag_so_la_SOURCES += fnm-util.c fnm-util.h
libpag_so_la_SOURCES += gendcls.h
libpag_so_la_SOURCES += get-util.c get-util.h
libpag_so_la_SOURCES += hash-util.c hash-util.h
libpag_so_la_SOURCES += list-util.c list-util.h
libpag_so_la_SOURCES += meo-util.c meo-util.h
libpag_so_la_SOURCES += misc-util.c misc-util.h
libpag_so_la_SOURCES += net-util.c net-util.h
libpag_so_la_SOURCES += opt-util.c opt-util.h
libpag_so_la_SOURCES += ring_buffer.c ring_buffer.h
libpag_so_la_SOURCES += seq-util.c seq-util.h
libpag_so_la_SOURCES += signal.c sig.h
libpag_so_la_SOURCES += skt-util.c skt-util.h
libpag_so_la_SOURCES += str-util.c str-util.h
libpag_so_la_SOURCES += str1-util.c str1-util.h
libpag_so_la_SOURCES += talloc.c talloc.h
libpag_so_la_SOURCES += tcp-util.c tcp-util.h
libpag_so_la_SOURCES += tv-util.c tv-util.h
libpag_so_la_SOURCES += udp-util.c udp-util.h
libpag_so_la_SOURCES += uri-parser.c uri-parser.h



lib_libpag_so_la = lib/libpag.so.la
