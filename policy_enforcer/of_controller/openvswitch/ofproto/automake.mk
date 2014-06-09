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
noinst_LIBRARIES += lib_libofproto.a

lib_libofproto_a_SOURCES = \
        $(OPENVSWITCH_SRCDIR)/ofproto/names.c \
        $(OPENVSWITCH_SRCDIR)/ofproto/ofproto.h

