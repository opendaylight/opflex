#
# opflex-agent: Opflex policy agent for Group-based policy
#
###########
#
# Process this file with autoconf to produce a configure script
#
# If you just want to start a build from source control, run
# autogen.sh first.
#

# ---------------------------------------------------------------
# Initialization

AC_INIT([opflex-agent], [2.1.0])
AC_SUBST(MODULE_NAME, ["Group-based Policy Agent"])

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

# initialize automake and libtool
AM_INIT_AUTOMAKE([subdir-objects])
AM_CONFIG_HEADER(config.h)
LT_INIT([disable-static])
AC_PROG_LIBTOOL
AC_CONFIG_MACRO_DIR([m4])

m4_include([m4/ax_boost_base.m4])
m4_include([m4/ax_boost_unit_test_framework.m4])
m4_include([m4/ax_boost_program_options.m4])
m4_include([m4/ax_boost_system.m4])
m4_include([m4/ax_boost_date_time.m4])
m4_include([m4/ax_boost_filesystem.m4])
m4_include([m4/ax_boost_iostreams.m4])
m4_include([m4/ax_boost_asio.m4])
m4_include([m4/ax_cxx_compile_stdcxx.m4])

dnl ---------------------------------------------------------------
dnl Configure options

dnl Modify the release/build version
AC_ARG_WITH(buildversion,
            AC_HELP_STRING([--with-buildversion],
                           [Version number of build]),
            [bversion=${withval}],
            [bversion='private'])
AC_SUBST(BVERSION, [${bversion}])

dnl allow to create final builds with assert()s disabled
AC_HEADER_ASSERT

# ---------------------------------------------------------------
# Environment introspection

# check for compiler
AC_PROG_CC_STDC
AC_PROG_CXX
AC_PROG_INSTALL
AM_PROG_AS
AC_LANG([C])
AC_LANG([C++])
AX_CXX_COMPILE_STDCXX([11], [ext], [mandatory])

# check for doxygen
AC_CHECK_PROGS(DOXYGEN,doxygen,none)
AM_CONDITIONAL(HAVE_DOXYGEN, [test x$DOXYGEN != 'xnone'])

# switch to enable OVS renderer plugin
AC_ARG_ENABLE(renderer-ovs,
              AC_HELP_STRING([--renderer-ovs],
                             [Build the OVS renderer]))
AM_CONDITIONAL([RENDERER_OVS], [test x$enable_renderer_ovs != 'xno'])
AM_COND_IF([RENDERER_OVS],
           [AC_MSG_NOTICE([Open vSwitch renderer is enabled])],
           [AC_MSG_NOTICE([Open vSwitch renderer is disabled])])

# Enable grpc support, disabled by default
AC_ARG_ENABLE(grpc,
              AC_HELP_STRING([--enable-grpc], [Enable Grpc support]))
AM_CONDITIONAL(ENABLE_GRPC, [test x$enable_grpc = 'xyes'])
AM_COND_IF([ENABLE_GRPC],
          [AC_MSG_NOTICE([GRPC is enabled])],
          [AC_MSG_NOTICE([GRPC is disabled])])
# ---------------------------------------------------------------
# Dependency checks

# Checks for header files
AC_STDC_HEADERS

AX_BOOST_BASE([1.53.0], [], AC_MSG_ERROR([Boost is required]))
AX_BOOST_PROGRAM_OPTIONS
if test "x$ax_cv_boost_program_options" != xyes; then
    AC_MSG_ERROR([Boost::Program_Options library is required])
fi
AX_BOOST_UNIT_TEST_FRAMEWORK
if test "x$ax_cv_boost_unit_test_framework" != xyes; then
    AC_MSG_ERROR([Boost::Test library is required])
fi
AX_BOOST_SYSTEM
if test "x$ax_cv_boost_system" != xyes; then
    AC_MSG_ERROR([Boost::System library is required])
fi
AX_BOOST_DATE_TIME
if test "x$ax_cv_boost_date_time" != xyes; then
    AC_MSG_ERROR([Boost::Date_Time library is required])
fi
AX_BOOST_FILESYSTEM
if test "x$ax_cv_boost_filesystem" != xyes; then
    AC_MSG_ERROR([Boost::Filesystem library is required])
fi
AX_BOOST_IOSTREAMS
if test "x$ax_cv_boost_iostreams" != xyes; then
    AC_MSG_ERROR([Boost::IOStreams library is required])
fi
AX_BOOST_ASIO
if test "x$ax_cv_boost_asio" != xyes; then
    AC_MSG_ERROR([Boost::ASIO library is required])
fi

PKG_CHECK_MODULES([libopflex], [libopflex >= 2.1.0])
PKG_CHECK_MODULES([libmodelgbp], [libmodelgbp >= 2.1.0])
PKG_CHECK_MODULES([rapidjson], [RapidJSON >= 1.1])

AM_COND_IF([RENDERER_OVS], [
    # check for libopenvswitch
    PKG_CHECK_MODULES([libofproto], [libofproto >= 2.6.7])
    PKG_CHECK_MODULES([libopenvswitch], [libopenvswitch >= 2.6.7])

    # check for libnetfilter-conntrack
    PKG_CHECK_MODULES([libnfct], [libnetfilter_conntrack >= 1.0.0],
                      [AC_DEFINE([HAVE_LIBNFCT], [1], [Use libnfct])],
                      AC_MSG_WARN([libnetfilter_conntrack not found]))
])

AM_COND_IF([ENABLE_GRPC], [
    # check for grpc++
    PKG_CHECK_MODULES([GRPC], [grpc++], [grpc_found=yes], [grpc_found=no])
    # check for protobuf
    PKG_CHECK_MODULES([PROTOBUF], [protobuf],
                     [protobuf_found=yes], [protobuf_found=no])
    AS_IF([test x$grpc_found = 'xyes' && test x$protobuf_found = 'xyes'], [
        AC_DEFINE([HAVE_GRPC_SUPPORT],[1],[Define if grpc support is enabled])
    ], [
        AC_MSG_ERROR([***GRPC requested but not found ***])
    ])
])

# inotify check
AC_ARG_ENABLE(inotify, "Whether to use inotify")
if test x$enable_inotify = xno; then
    have_inotify=no;
else
    AC_CHECK_HEADERS(sys/inotify.h, HAVE_INOTIFY=no, HAVE_INOTIFY=yes)
fi

# eventfd check
AC_ARG_ENABLE(eventfd, "Whether to use eventfd")
if test x$enable_eventfd = xno; then
    have_eventfd=no;
else
    AC_CHECK_HEADERS(sys/eventfd.h, HAVE_EVENTFD=no, HAVE_EVENTFD=yes)
fi

# getifaddrs check
AC_ARG_ENABLE(getifaddrs, "Whether to use getifaddrs for interface enumeration")
if test x$enable_getifaddrs = xno; then
    have_getifaddrs=no;
else
    AC_CHECK_HEADERS(ifaddrs.h, HAVE_GETIFADDRS=no, HAVE_GETIFADDRS=yes)
fi

# Older versions of autoconf don't define docdir
if test x$docdir = x; then
   AC_SUBST(docdir, ['${prefix}/share/doc/'$PACKAGE])
fi

# ---------------------------------------------------------------
# Output

AC_CONFIG_FILES([\
        Makefile \
	libopflex_agent.pc \
        debian/changelog \
        doc/Doxyfile \
        rpm/opflex-agent.spec])
AC_CONFIG_FILES([debian/rules], [chmod +x debian/rules])
AC_OUTPUT

AC_MSG_NOTICE([
======================================================================
Configuration complete

You may now compile the software by running 'make'
======================================================================])
