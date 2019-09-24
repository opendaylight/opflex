#
# libopflex: a framework for developing opflex-based policy agents
# Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v1.0 which accompanies this distribution,
# and is available at http://www.eclipse.org/legal/epl-v10.html
#
###########
dnl
dnl Process this file with autoconf to produce a configure script
dnl
dnl If you just want to start a build from source control, run
dnl autogen.sh first.
dnl

dnl ---------------------------------------------------------------
dnl Version information

dnl Before doing a release, edit:
dnl 1. the version in AC_INIT
dnl 2. the libtool version in VERSION_INFO

m4_define([sdk_pversion], 2)
m4_define([sdk_sversion], 1)
m4_define([sdk_iversion], 0)
AC_INIT([libopflex], [sdk_pversion.sdk_sversion.sdk_iversion])
AC_SUBST(SDK_MARKETNAME, ["OpFlex Framework"])
AC_DEFINE(SDK_MARKETNAME, ["OpFlex Framework"], [Marketing name for SDK])

dnl For more information on libtool versioning, see:
dnl https://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html
dnl
dnl The version string is [current:revision:age] or [c:r:a].  Here are a
dnl set of rules to help you update your library version information:
dnl  1. Start with version information of 0:0:0 for each libtool
dnl     library.
dnl  2. Update the version information only immediately before a public
dnl     release of your software. More frequent updates are unnecessary,
dnl     and only guarantee that the current interface number gets larger
dnl     faster.
dnl  3. If the library source code has changed at all since the last
dnl     update, then increment revision (c:r:a becomes c:r+1:a).
dnl  4. If any interfaces have been added, removed, or changed since the
dnl     last update, increment current, and set revision to 0.
dnl  5. If any interfaces have been added since the last public release,
dnl     then increment age.
dnl  6. If any interfaces have been removed since the last public
dnl     release, then set age to 0.

AC_SUBST(VERSION_INFO, [13:0:13])

dnl ---------------------------------------------------------------
dnl Initialization

dnl initialize automake and libtool
AM_INIT_AUTOMAKE([subdir-objects silent-rules foreign])
AM_CONFIG_HEADER(config.h)
AC_CONFIG_MACRO_DIR([m4])
LT_INIT()

m4_include([m4/ax_boost_base.m4])
m4_include([m4/ax_boost_unit_test_framework.m4])
m4_include([m4/ax_boost_system.m4])
m4_include([m4/ax_boost_asio.m4])
m4_include([m4/ax_check_compile_flags.m4])
m4_include([m4/ax_cxx_compile_stdcxx.m4])

dnl ---------------------------------------------------------------
dnl Configure options

dnl Modify the release/build version
AC_ARG_WITH(buildversion,
            AC_HELP_STRING([--with-buildversion], 
                           [Version number of build]),
            [sdk_bversion=${withval}],
            [sdk_bversion='private'])

dnl allow to create final builds with assert()s disabled
AC_HEADER_ASSERT

dnl Create an option to install communications library separately
AC_ARG_ENABLE(separate-comms,
              AC_HELP_STRING([--enable-separate-comms],
                             [Install raw opflex communications library separately]))
AM_CONDITIONAL(SEPARATE_COMMS, [test x$enable_separate_comms = 'xyes'])

dnl Create an option to build unit tests only with make check
AC_ARG_ENABLE(tests-make-all,
              AC_HELP_STRING([--disable-tests-make-all],
                             [Disable building tests with make all]))
AM_CONDITIONAL(MAKE_ALL_TESTS, [test x$enable_tests_make_all != 'xno'])

dnl ---------------------------------------------------------------
dnl Environment introspection

dnl check for compiler
AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL
AM_PROG_AS
AC_LANG([C++])
AX_CXX_COMPILE_STDCXX([11], [noext], [optional])

dnl check for compiler flags
AX_CHECK_COMPILE_FLAG([-Wno-multichar],
                      [with_as_cxxflags_compile_time_hash='-Wno-multichar'],
		      [with_as_cxxflags_compile_time_hash=''])
AC_SUBST([OUR_CXXFLAGS_COMPILE_TIME_HASH],
         "$with_as_cxxflags_compile_time_hash")

AX_CHECK_COMPILE_FLAG([-Wno-unsupported-friend],
                      [with_as_cxxflags_unsupported_friend='-Wno-unsupported-friend'],
		      [with_as_cxxflags_unsupported_friend=''])
AC_SUBST([OUR_CXXFLAGS_UNSUPPORTED_FRIEND],
         "$with_as_cxxflags_unsupported_friend")

AX_CHECK_COMPILE_FLAG([-Wno-error=unsupported-friend],
                      [with_as_cxxflags_error_unsupported_friend='-Wno-error=unsupported-friend'],
		      [with_as_cxxflags_error_unsupported_friend=''])
AC_SUBST([OUR_CXXFLAGS_ERROR_UNSUPPORTED_FRIEND],
         "$with_as_cxxflags_error_unsupported_friend")


dnl check for doxygen
AC_CHECK_PROGS(DOXYGEN,doxygen,none)
AM_CONDITIONAL(HAVE_DOXYGEN, [test x$DOXYGEN != 'xnone']) 

dnl ---------------------------------------------------------------
dnl Dependency checks

dnl Checks for header files
AC_STDC_HEADERS

dnl Boost dependencies
AX_BOOST_BASE([1.49.0], [], AC_MSG_ERROR([Boost is required]))
AX_BOOST_SYSTEM
AX_BOOST_ASIO
AX_BOOST_UNIT_TEST_FRAMEWORK

dnl Package-config dependencies
PKG_CHECK_MODULES([UV], [libuv >= 1.8.0])
PKG_CHECK_MODULES([OPENSSL], [openssl >= 1.0.1])
PKG_CHECK_MODULES([RAPIDJSON], [RapidJSON >= 1.1])

dnl Older versions of autoconf don't define docdir
if test x$docdir = x; then
   AC_SUBST(docdir, ['${prefix}/share/doc/'$PACKAGE])
fi

dnl ---------------------------------------------------------------
dnl Build constants

AC_SUBST(SDK_PVERSION, [sdk_pversion])
AC_SUBST(SDK_SVERSION, [sdk_sversion])
AC_SUBST(SDK_IVERSION, [sdk_iversion])
AC_SUBST(SDK_BVERSION, [${sdk_bversion}])
AC_SUBST(SDK_FULL_VERSION, ["sdk_pversion.sdk_sversion.sdk_iversion-${sdk_bversion}"])

dnl ---------------------------------------------------------------
dnl Output

AC_CONFIG_FILES([              \
        Makefile               \
	debian/changelog       \
        util/Makefile          \
        logging/Makefile       \
        comms/Makefile         \
        comms/include/Makefile \
        modb/Makefile          \
        modb/test/Makefile     \
        engine/Makefile        \
        engine/test/Makefile   \
        ofcore/Makefile        \
        ofcore/test/Makefile   \
        cwrapper/Makefile      \
        cwrapper/test/Makefile \
        libopflex.pc           \
        doc/Doxyfile           \
        doc/Doxyfile-internal  \
        rpm/libopflex.spec
])
AC_CONFIG_FILES([debian/rules], [chmod +x debian/rules])

AC_CONFIG_LINKS([
                 comms/test/server.pem:comms/test/server.pem
                 comms/test/ca.pem:comms/test/ca.pem
                 comms/test/stable_tests.sh:comms/test/stable_tests.sh
               ])

AC_OUTPUT

AC_MSG_NOTICE([
======================================================================
Configuration complete

You may now compile the software by running 'make'
======================================================================])
