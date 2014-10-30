#
# agent-ovs: Opflex agent for integrating Group-based policy with OVS
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

AC_INIT([agent-ovs], [1.0.0])
AC_SUBST(MODULE_NAME, ["Group-based Policy Agent for OVS"])

# initialize automake and libtool
AM_INIT_AUTOMAKE([subdir-objects])
AM_CONFIG_HEADER(config.h)
LT_INIT()
AC_PROG_LIBTOOL
AC_CONFIG_MACRO_DIR([m4])

m4_include([m4/ax_boost_base.m4])
m4_include([m4/ax_boost_unit_test_framework.m4])
m4_include([m4/ax_boost_program_options.m4])
m4_include([m4/ax_boost_thread.m4])
m4_include([m4/ax_boost_system.m4])
m4_include([m4/ax_boost_date_time.m4])
m4_include([m4/ax_boost_filesystem.m4])

# ---------------------------------------------------------------
# Environment introspection

# check for compiler
AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL
AM_PROG_AS
AC_LANG([C++])


# check for doxygen
AC_CHECK_PROGS(DOXYGEN,doxygen,none)
AM_CONDITIONAL(HAVE_DOXYGEN, [test x$DOXYGEN != 'xnone']) 

# ---------------------------------------------------------------
# Dependency checks

# Checks for header files
AC_STDC_HEADERS

AX_BOOST_BASE([1.53.0], [], AC_MSG_ERROR([Boost is required]))
AX_BOOST_PROGRAM_OPTIONS
AX_BOOST_UNIT_TEST_FRAMEWORK
AX_BOOST_SYSTEM
AX_BOOST_DATE_TIME
AX_BOOST_THREAD
AX_BOOST_FILESYSTEM

PKG_CHECK_MODULES([libopflex], [libopflex >= 0.1.0])
PKG_CHECK_MODULES([libmodelgbp], [libmodelgbp >= 0.1.0])
PKG_CHECK_MODULES([libglog], [libglog >= 0.3.3])

# check for libopenvswitch. Should use pkg-config
OVS_PREFIX=/usr/local
AC_SUBST(OVS_PREFIX)
AC_CHECK_LIB([openvswitch], main,
    [OVS_LIBS="-lopenvswitch -lpthread -lrt -lssl -lcrypto";
     OVS_CFLAGS="-I\$(OVS_PREFIX)/include -I\$(OVS_PREFIX)/include/ovs/include -fpermissive"],
    [echo "libopenvswitch not found; agent won't be built"])
AC_SUBST(OVS_LIBS)
AC_SUBST(OVS_CFLAGS)
AM_CONDITIONAL([HAVE_OVS], [test "$ac_cv_lib_openvswitch_main" = yes])

# inotify check
AC_ARG_ENABLE(inotify, "Whether to use inotify for endpoint source")
if test x$enable_inotify = xno; then
    have_inotify=no;
else
    AC_CHECK_HEADERS(sys/inotify.h, HAVE_INOTIFY=no, HAVE_INOTIFY=yes)
fi
AC_SUBST([HAVE_INOTIFY])

# eventfd check
AC_ARG_ENABLE(eventfd, "Whether to use eventfd for endpoint source")
if test x$enable_eventfd = xno; then
    have_eventfd=no;
else
    AC_CHECK_HEADERS(sys/eventfd.h, HAVE_EVENTFD=no, HAVE_EVENTFD=yes)
fi
AC_SUBST([HAVE_EVENTFD])

# Older versions of autoconf don't define docdir
if test x$docdir = x; then
   AC_SUBST(docdir, ['${prefix}/share/doc/'$PACKAGE])
fi

# ---------------------------------------------------------------
# Output

AC_CONFIG_FILES([\
	Makefile \
	src/enforcer/Makefile \
	src/enforcer/test/Makefile \
	doc/Doxyfile])
AC_OUTPUT

AC_MSG_NOTICE([
======================================================================
Configuration complete

You may now compile the software by running 'make'
======================================================================])
