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

AX_BOOST_BASE([1.49.0], [], AC_MSG_ERROR([Boost is required]))
AX_BOOST_UNIT_TEST_FRAMEWORK
PKG_CHECK_MODULES([libopflex], [libopflex >= 0.1.0])
PKG_CHECK_MODULES([libmodelgbp], [libmodelgbp >= 0.1.0])
PKG_CHECK_MODULES([libglog], [libglog >= 0.3.3])

# Older versions of autoconf don't define docdir
if test x$docdir = x; then
   AC_SUBST(docdir, ['${prefix}/share/doc/'$PACKAGE])
fi

# ---------------------------------------------------------------
# Output

AC_CONFIG_FILES([\
	Makefile \
	doc/Doxyfile])
AC_OUTPUT

AC_MSG_NOTICE([
======================================================================
Configuration complete

You may now compile the software by running 'make'
======================================================================])
