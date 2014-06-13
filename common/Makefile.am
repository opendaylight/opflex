# /*
#  * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
#  *
#  * This program and the accompanying materials are made available under the
#  * terms of the Eclipse Public License v1.0 which accompanies this distribution,
#  * and is available at http://www.eclipse.org/legal/epl-v10.html
#  */

lib_LTLIBRARIES = libcommon.so.la
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
AM_CPPFLAGS=-DBSD_COMP=1 
if HAVE_TCMALLOC
AM_LDFLAGS=-L/usr/lib -lnsl -lm -lpthread -ltcmalloc
else
AM_LDFLAGS=-L/usr/lib -lnsl -lm -lpthread
endif

libcommon_so_la_SOURCES  =
libcommon_so_la_SOURCES += debug_levels.c

lib_libcommon_so_la = lib/libcommon.so.la
