#
# libopflex: a framework for developing opflex-based policy agents
# Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v1.0 which accompanies this distribution,
# and is available at http://www.eclipse.org/legal/epl-v10.html
#
###########
#
# Process this file with autoconf to produce a configure script
#
# If you just want to start a build from source control, run
# autogen.sh first.
#

# ---------------------------------------------------------------
# Version information

# Before doing a release, edit:
# 1. the version in AC_INIT
# 2. the libtool version in VERSION_INFO

m4_define([sdk_pversion], 0)
m4_define([sdk_sversion], 1)
m4_define([sdk_iversion], 0)
AC_INIT([libopflex], [sdk_pversion.sdk_sversion.sdk_iversion])
AC_SUBST(SDK_MARKETNAME, ["OpFlex Framework"])
AC_DEFINE(SDK_MARKETNAME, ["OpFlex Framework"], [Marketing name for SDK])

# For more information on libtool versioning, see:
# http://www.gnu.org/software/libtool/manual.html#Versioning
# 
# The version string is [current:revision:age] or [c:r:a].  Here are a
# set of rules to help you update your library version information:
#  1. Start with version information of 0:0:0 for each libtool
#     library.
#  2. Update the version information only immediately before a public
#     release of your software. More frequent updates are unnecessary,
#     and only guarantee that the current interface number gets larger
#     faster.
#  3. If the library source code has changed at all since the last
#     update, then increment revision (c:r:a becomes c:r+1:a).
#  4. If any interfaces have been added, removed, or changed since the
#     last update, increment current, and set revision to 0.
#  5. If any interfaces have been added since the last public release,
#     then increment age.
#  6. If any interfaces have been removed since the last public
#     release, then set age to 0.

AC_SUBST(VERSION_INFO, [0:0:0])

# ---------------------------------------------------------------
# Initialization

# initialize automake and libtool
AM_INIT_AUTOMAKE([subdir-objects silent-rules foreign])
AM_CONFIG_HEADER(config.h)
AC_CONFIG_MACRO_DIR([m4])
LT_INIT()
AC_PROG_LIBTOOL

m4_include([m4/ax_boost_base.m4])
m4_include([m4/ax_boost_unit_test_framework.m4])
m4_include([m4/ax_check_compile_flags.m4])

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES])

# ---------------------------------------------------------------
# Configure options

# Modify the release/build version
AC_ARG_WITH(buildversion,
            AC_HELP_STRING([--with-buildversion], 
                           [Version number of build]),
            [sdk_bversion=${withval}],
            [sdk_bversion='private'])

# Create an option to install communications library separate
AC_ARG_ENABLE(comms,
              AC_HELP_STRING([--enable-comms],
                             [Install raw opflex communications library separately]),
             [separate_comms='true'],
             [separate_comms='false'])
AM_CONDITIONAL(SEPARATE_COMMS, [test x$separate_comms != 'xfalse'])

# ---------------------------------------------------------------
# Environment introspection

# check for compiler
AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL
AM_PROG_AS
AC_LANG([C++])

# check for compiler flags
AX_CHECK_COMPILE_FLAG([-Wno-multichar],
                      [with_as_cxxflags_compile_time_hash='-Wno-multichar'],
		      [with_as_cxxflags_compile_time_hash=''])
AC_SUBST([OUR_CXXFLAGS_COMPILE_TIME_HASH],
         "$with_as_cxxflags_compile_time_hash")

# check for doxygen
AC_CHECK_PROGS(DOXYGEN,doxygen,none)
AM_CONDITIONAL(HAVE_DOXYGEN, [test x$DOXYGEN != 'xnone']) 

# check for ietf tools
AC_PATH_PROG([XML2RFC], [xml2rfc], "no")
AC_PATH_PROG([XSLTPROC], [xsltproc], "no")

AM_CONDITIONAL([BUILD_SPECIFICATIONS],
               [test "x$XML2RFC" != "xno" -a "x$XSLTPROC" != "xno"])

AC_PATH_PROG([XMLLINT], [xmllint], "no")
AM_CONDITIONAL([LINT_SPECIFICATIONS],
               [test "x$XMLLINT" != "xno"])

# ---------------------------------------------------------------
# Dependency checks

# Checks for header files
AC_STDC_HEADERS

# Boost dependencies
AX_BOOST_BASE([1.49.0], [], AC_MSG_ERROR([Boost is required]))
AX_BOOST_UNIT_TEST_FRAMEWORK

# libuv
PKG_CHECK_MODULES([UV], [libuv >= 0.11.0])

# rapidjson
AC_CHECK_HEADER("rapidjson/rapidjson.h", [], AC_MSG_ERROR([RapidJson is required]))

# Older versions of autoconf don't define docdir
if test x$docdir = x; then
   AC_SUBST(docdir, ['${prefix}/share/doc/'$PACKAGE])
fi

# ---------------------------------------------------------------
# Build constants

AC_DEFINE_UNQUOTED(SDK_PVERSION, [sdk_pversion], [Major version])
AC_DEFINE_UNQUOTED(SDK_SVERSION, [sdk_sversion], [Minor version])
AC_DEFINE_UNQUOTED(SDK_IVERSION, [sdk_iversion], [Incremental version])
AC_DEFINE_UNQUOTED(SDK_BVERSION, ["${sdk_bversion}"])
AC_DEFINE_UNQUOTED(SDK_BVERSION, [${sdk_bversion}], [Build version])
AC_DEFINE_UNQUOTED(SDK_FULL_VERSION, ["sdk_pversion.sdk_sversion.sdk_iversion-${sdk_bversion}"], 
                                     [Full version])

# ---------------------------------------------------------------
# Output

AC_CONFIG_FILES([\
        Makefile \
        util/Makefile \
        logging/Makefile \
        comms/Makefile \
        modb/Makefile \
        modb/test/Makefile \
        engine/Makefile \
        engine/test/Makefile \
        ofcore/Makefile \
        ofcore/test/Makefile \
        cwrapper/Makefile \
        cwrapper/test/Makefile \
        libopflex.pc \
        doc/Doxyfile \
        doc/Doxyfile-internal
])
AC_OUTPUT

AC_MSG_NOTICE([
======================================================================
Configuration complete

You may now compile the software by running 'make'
======================================================================])
