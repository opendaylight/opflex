#
# renderer-sample: Sample OpFlex agent renderer plugin
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

AC_INIT([renderer-sample], [1.0.0])

# initialize automake and libtool
AM_INIT_AUTOMAKE([subdir-objects silent-rules foreign])
AM_CONFIG_HEADER(config.h)
AC_CONFIG_MACRO_DIR([m4])
LT_INIT([disable-static])

m4_include([m4/ax_cxx_compile_stdcxx.m4])
m4_include([m4/ax_boost_unit_test_framework.m4])

# ---------------------------------------------------------------
# Configure options

# Modify the release/build version
AC_ARG_WITH(buildversion,
            AC_HELP_STRING([--with-buildversion],
                           [Version number of build]),
            [bversion=${withval}],
            [bversion='private'])
AC_SUBST(BVERSION, [${bversion}])

# allow to create final builds with assert()s disabled
AC_HEADER_ASSERT

# ---------------------------------------------------------------
# Environment introspection

# check for compiler
AC_PROG_CXX
AC_PROG_INSTALL
AM_PROG_AS
AC_LANG([C++])
AX_CXX_COMPILE_STDCXX([11], [], [mandatory])

# ---------------------------------------------------------------
# Dependency checks

# Checks for header files
AC_STDC_HEADERS

PKG_CHECK_MODULES([libopflex_agent], [libopflex_agent >= 1.7.0])

AX_BOOST_BASE([1.53.0], [], AC_MSG_ERROR([Boost is required]))
AX_BOOST_UNIT_TEST_FRAMEWORK
if test "x$ax_cv_boost_unit_test_framework" != xyes; then
    AC_MSG_ERROR([Boost::Test library is required])
fi

# ---------------------------------------------------------------
# Output

AC_CONFIG_FILES([\
        Makefile \
	])
AC_OUTPUT

AC_MSG_NOTICE([
======================================================================
Configuration complete

You may now compile the software by running 'make'
======================================================================])
