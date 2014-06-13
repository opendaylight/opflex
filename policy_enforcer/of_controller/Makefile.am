#------------------------------------------------------------------
# Makefile.am
#
# May 2014, Abilash Menon
#
# Copyright (c) 2013-2014 by Cisco Systems, Inc.
#
# All rights reserved.
#------------------------------------------------------------------

# Workspace definitions
WORKSPACE_BASE = $(top_srcdir)
OPENVSWITCH_SRCDIR=$(top_srcdir)/openvswitch

#DISTRO_TOOLCHAIN=/auto/xe-distro/toolchain/cge6ga-mar2013-1/tools/
#LIBNAME=lib
#COMPILE_OPTS=
#SOCK_OPTS= -lsockvrf
#RANLIB_PATH=$(DISTRO_TOOLCHAIN)/i686-gnu/bin/i686-montavista-linux-gnu-ranlib
#AR_PATH=$(DISTRO_TOOLCHAIN)/i686-gnu/bin/i686-montavista-linux-gnu-ar

#RANLIB=$(RANLIB_PATH)
#AR=$(AR_PATH)

VPATH = ..: ${OPENVSWITCH_SRCDIR} 
AM_MAKEFLAGS = -f Makefile
# master amflags doesn't yet work, using file from 1.4.0 copy to below
#     as it was always being used (even for master) in the past
#ACLOCAL_AMFLAGS = -I openvswitch/m4
noinst_LIBRARIES =

bin_PROGRAMS = opflex_agent

opflex_agent_SOURCES = src/opflex-enforcer.c \
			$(OPENVSWITCH_SRCDIR)/utilities/ovs-ofctl.c

opflex_agent_LDADD = lib_libovs.a \
		        lib_libofproto.a
#		     libutilities_ovs_ofctl.a

#Automake C flags
AM_CFLAGS =  -Wall -Werror $(HARDEN_CFLAGS)
# Add -pthread GCC flag, which also sets -D_REENTRANT and -lpthread
AM_CFLAGS += -pthread
# For ADS gcc.c4.5.3-p0, gnu99 triggers define of needed LLONG_MIN/LLONG_MAX
AM_CFLAGS += -std=gnu11
AM_CFLAGS += $(COMPILE_OPTS)

#Automak CPP flags
AM_CPPFLAGS = -D_GNU_SOURCE=1
AM_CPPFLAGS += -I $(OPENVSWITCH_SRCDIR)
AM_CPPFLAGS += -I $(OPENVSWITCH_SRCDIR)/include
AM_CPPFLAGS += -I $(OPENVSWITCH_SRCDIR)/ofproto
AM_CPPFLAGS += -I $(OPENVSWITCH_SRCDIR)/lib
AM_CPPFLAGS += -I $(top_builddir)/lib
#AM_CPPFLAGS += -I $(top_srcdir)/openvswitch/$(OPENVSWITCH_MASTER)
#AM_CPPFLAGS += -I $(top_srcdir)/openvswitch/$(OPENVSWITCH_MASTER)/include
#AM_CPPFLAGS += -I $(top_srcdir)/openvswitch/$(OPENVSWITCH_MASTER)/ofproto
#AM_CPPFLAGS += -I $(top_srcdir)/openvswitch/$(OPENVSWITCH_MASTER)/lib
#AM_CPPFLAGS += -I $(top_srcdir)/cmn

#Automake LD flags
AM_LDFLAGS = $(HARDEN_LDFLAGS)

#Shared library location
# look for libs in obj-lxe: compile_lxe.sh builds symlinks to libs there
AM_LDFLAGS += -L.
AM_LDFLAGS += -lxml2 -lz
AM_LDFLAGS += -lssl -lcrypto

ro_c = echo '/* -*- mode: c; buffer-read-only: t -*- */'

EXTRA_DIST =


#include cmn/cmn_automake.mk
#include openvswitch/ovs_automake.mk
include openvswitch/lib/automake.mk
include openvswitch/ofproto/automake.mk
#include openvswitch/utilities/automake.mk
