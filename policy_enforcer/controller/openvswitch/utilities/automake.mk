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
## Setup ovs-ofctl as a static library
noinst_LIBRARIES += libutilities_ovs_ofctl.a

#ovs ofct source
libutilities_ovs_ofctl_a_SOURCES= \
        $(OPENVSWITCH_SRCDIR)/utilities/ovs-ofctl.c

libutilities_ovs_ofctl_a_LIBADD = \
	lib_libovs.a

