# /*
#  * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
#  *
#  * This program and the accompanying materials are made available under the
#  * terms of the Eclipse Public License v1.0 which accompanies this distribution,
#  * and is available at http://www.eclipse.org/legal/epl-v10.html
#  */

#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_INIT([pagentd], [1.0.1], [policy-agent])

# configure_flags="$*"
# AC_DEFINE_UNQUOTED([CONFIG_FLAGS], ["$configure_flags"], [Build options.])
AC_ARG_WITH([tests], 
		      [AC_HELP_STRING([--with-tests],
		      [Builds the tests, requires cmocka])],
		      [with_tests=yes],
		      [with_test=no])

AM_CONFIG_HEADER([config.h])
AM_INIT_AUTOMAKE
AC_USE_SYSTEM_EXTENSIONS

AC_CONFIG_SRCDIR([pagentd/pagentd.c])
# AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_LIBTOOL

# Checks for libraries.
#LT_INIT()
AC_PROG_CC
AC_PROG_CXX
AC_PROG_CPP 
PAG_CHECK_OPENSSL

# Print version of CC and CXX compiler (if they support --version)
case $SYSTEM_TYPE in
  *netware*)
CC_VERSION=`$CC -version | grep -i version`
    ;;
  *)
CC_VERSION=`$CC --version | sed 1q`
    ;;
esac
if test $? -eq "0"
then
  AC_MSG_CHECKING("C Compiler version")
  AC_MSG_RESULT("$CC $CC_VERSION")
else
CC_VERSION=""
fi
AC_SUBST(CC_VERSION)

AC_PROG_RANLIB
AC_PROG_LIBTOOL

# Ensure that we have --preserve-dup-deps defines, otherwise we get link
# LIBTOOL='$(SHELL) $(top_builddir)/libtool'
LIBTOOL='$(SHELL) /usr/bin//libtool'
LIBTOOL="$LIBTOOL --preserve-dup-deps"
AC_SUBST(LIBTOOL)dnl

AC_CHECK_MEMBERS([struct stat.st_mtim.tv_nsec, struct stat.st_mtimensec],
  [], [], [[#include <sys/stat.h>]])
# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h float.h inttypes.h limits.h malloc.h netdb.h netinet/in.h stddef.h stdint.h stdlib.h string.h sys/mount.h sys/socket.h sys/time.h syslog.h unistd.h], [], [_gen_headers=0])
AC_CHECK_HEADERS([readline/readline.h readline/history.h], [], [_readline_headers=0])
AC_CHECK_HEADER([linux/netlink.h],
                   [HAVE_NETLINK=yes],
                   [HAVE_NETLINK=no],
                   [#include <sys/socket.h>
   ])
if test "$_gen_headers" == 0
then
    AC_MSG_ERROR([Cannot find generic headers, check config.log])
fi

if test "$_readline_headers" == 0
then
    AC_MSG_ERROR([Cannot find readline headers, possibly install readline-devel])
fi

# OpenSSL
# Checks for libraries.
#### Find OpenSSL /usr/include/openssl
AM_CONDITIONAL([HAVE_OPENSSL], [test "$ssl" = yes])
if test "$ssl" = yes; then
   AC_CHECK_HEADER([openssl/ssl.h], 
      [AC_DEFINE([HAVE_OPENSSL], [1], [Openssl available])],
      [AC_DEFINE([HAVE_OPENSSL], [0], [Openssl not available])])
fi

# test for netlink
AM_CONDITIONAL([HAVE_NETLINK], [test "$HAVE_NETLINK" = yes])
if test "$HAVE_NETLINK" = yes; then
   AC_DEFINE([HAVE_NETLINK], [1],
   [Define to 1 if Netlink protocol available.])
fi

# test for POSIX asynchronous I/O
# The POSIX asynchronous I/O (AIO) interface allows applications to
# initiate one or more I/O operations that are performed asynchronously
# (i.e., in the background).  The application can elect to be notified
# of completion of the I/O operation in a variety of ways: by delivery
# of a signal, by instantiation of a thread, or no notification at all.
AM_CONDITIONAL([HAVE_POSIX_AIO], [test "$HAVE_POSIX_AIO" = yes])
if test "$HAVE_POSIX_AIO" = yes; then
   AC_SEARCH_LIBS([aio_write], [rt], [], [
   AC_DEFINE([HAVE_POSIX_AIO], [1],
   [Define to 1 if POSIX AIO available.])])
else
   AC_DEFINE([HAVE_POSIX_AIO],[0],
   [Define to 0 if POSIX AIO not available.])
fi

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_TYPE_UID_T
AC_C_INLINE
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_INT8_T
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_CHECK_DECLS([sys_siglist])
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T
AC_CHECK_TYPES([ptrdiff_t])

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_STRNLEN
AC_FUNC_STRTOD
AC_FUNC_WAIT3
AC_CHECK_FUNCS([bzero atexit bzero clock_gettime dup2 floor getcwd gethostbyaddr gethostbyname gettimeofday inet_ntoa localtime_r memchr memmove memset modf pathconf pow rint socket sqrt strcasecmp strchr strcspn strdup strerror strncasecmp strndup strrchr strspn strstr strtoul strtoull])

AC_LANG_C
AC_PROG_CC

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_TYPE_UID_T
AC_C_INLINE
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_INT8_T
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_STRNLEN
AC_FUNC_STRTOD
AC_FUNC_WAIT3
AC_CHECK_FUNCS([atexit bzero clock_gettime dup2 floor ftruncate getcwd gethostbyaddr gethostbyname gethostname gettimeofday inet_ntoa isascii localtime_r memchr memmove memset mkdir modf pathconf pow rint select socket sqrt strcasecmp strchr strcspn strdup strerror strncasecmp strndup strpbrk strrchr strspn strstr strtol strtoul strtoull])

# check for tcalloc library, gperftools
AM_CONDITIONAL([HAVE_TCMALLOC], [test "$have_tcmalloc" = yes])
if test "$have_tcmalloc" = yes; then
   AC_CHECK_HEADER([google/malloc_extension_c.h], 
      [AC_DEFINE([HAVE_TCMALLOC], [1], [gperftools-libs/devel tcmalloc available])],
      [AC_DEFINE([HAVE_TCMALLOC], [0], [gperftools-devel tcmalloc not available])])
fi

AC_LANG_C
AC_PROG_CC
AM_PROG_LIBTOOL

AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([third-party/Makefile])
AC_CONFIG_FILES([lib/Makefile])
AC_CONFIG_FILES([modb/Makefile])
AC_CONFIG_FILES([pagentd/Makefile])

if test "$with_tests" = "yes"; then
   AC_CONFIG_FILES([tests/Makefile])
fi
AM_CONDITIONAL(BUILD_TESTS, [test "$with_tests" = "yes"])

AC_OUTPUT

