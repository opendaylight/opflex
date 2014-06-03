#------------------------------------------------------------------
# confgure.ac
#
# May 2014, Abilash Menon
#
# Copyright (c) 2013-2014 by Cisco Systems, Inc.
#
# All rights reserved.
#------------------------------------------------------------------

AC_INIT(opflex_agent, 0.1, oplfex@cisco.com)
AM_INIT_AUTOMAKE([-Wall -Werror foreign ])
#NX_BUILDNR  - nicira build number macro, removed from -master
AC_PROG_CC
AC_CONFIG_HEADERS([config.h])
#AC_CONFIG_MACRO_DIR([openvswitch/m4])
AC_HEADER_STDC
AC_CHECK_HEADERS
m4_pattern_allow([AM_PROG_AR])
AM_PROG_AR

AC_PROG_LIBTOOL
AC_PROG_RANLIB
#LT_INIT  - new version of AC_PROG_LIBT

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_HEADER_TIME
AC_C_CONST
AC_C_INLINE
AC_STRUCT_TM
AC_C_VOLATILE
AC_CHECK_TYPES

AC_SEARCH_LIBS([pow], [m])
AC_SEARCH_LIBS([clock_gettime], [rt])
AC_SEARCH_LIBS([timer_create], [rt])

# To enable linker section
OVS_CHECK_LINKER_SECTIONS
# To  fix compile issue - error: unknown type name '__aligned_u64'
AC_CHECK_HEADERS([linux/types.h])
AC_CHECK_TYPES(__be16)

# Checks for library functions.
# will define HAVE_STRERROR_R if available
AC_FUNC_STRERROR_R
AC_CHECK_FUNCS
AC_C_BIGENDIAN

#Custom defintions added
AC_DEFINE([NO_SFLOW], [1], [disable sflow code])
AC_DEFINE([NO_DPIF_NETDEV], [1], [disable dpif-netdev])
AC_DEFINE([NO_SYSLOG], [1], [disable OVS native syslog support])
AC_DEFINE([NO_LOGFILE], [1], [disable file-based logging])
AC_DEFINE([NO_PACKET_DUMP], [1], [disable packet-dump code])
AC_DEFINE([HAVE_OPENSSL], [1], [SSL Support])
#AC_DEFINE([HAVE_LINUX_TYPES_H], [1], [Linux types])
#CISCO
#AC_DEFINE([__USE_UNIX98], [1], [pthread support])

AC_CONFIG_FILES([Makefile])

AC_OUTPUT
