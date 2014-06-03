dnl -------------------------------------------------------- -*- autoconf -*-
dnl /*
dnl  * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
dnl  *
dnl  * This program and the accompanying materials are made available under the
dnl  * terms of the Eclipse Public License v1.0 which accompanies this distribution,
dnl  * and is available at http://www.eclipse.org/legal/epl-v10.html
dnl  */

dnl
dnl crypto.m4 Trafficserver's Crypto autoconf macros
dnl

dnl
dnl TS_CHECK_CRYPTO: look for crypto libraries and headers
dnl
AC_DEFUN([TS_CHECK_CRYPTO], [
  enable_crypto=no
  AC_CHECK_LIB([crypt],[crypt],[AC_SUBST([LIBCRYPT],["-lcrypt"])])

  TS_CHECK_CRYPTO_OPENSSL
  dnl add checks for other varieties of ssl here
])
dnl

AC_DEFUN([TS_CHECK_CRYPTO_OPENSSL], [
enable_openssl=no
AC_ARG_WITH(openssl, [AC_HELP_STRING([--with-openssl=DIR],[use a specific OpenSSL library])],
[
  if test "x$withval" != "xyes" && test "x$withval" != "x"; then
    openssl_base_dir="$withval"
    if test "$withval" != "no"; then
      enable_openssl=yes
      case "$withval" in
      *":"*)
        openssl_include="`echo $withval |sed -e 's/:.*$//'`"
        openssl_ldflags="`echo $withval |sed -e 's/^.*://'`"
        AC_MSG_CHECKING(checking for OpenSSL includes in $openssl_include libs in $openssl_ldflags )
        ;;
      *)
        openssl_include="$withval/include"
        openssl_ldflags="$withval/lib"
        AC_MSG_CHECKING(checking for OpenSSL includes in $withval)
        ;;
      esac
    fi
  fi
])

if test "x$openssl_base_dir" = "x"; then
  AC_MSG_CHECKING([for OpenSSL location])
  AC_CACHE_VAL(ats_cv_openssl_dir,[
  for dir in /usr/local/ssl /usr/pkg /usr/sfw /usr/local /usr; do
    if test -d $dir && test -f $dir/include/openssl/x509.h; then
      ats_cv_openssl_dir=$dir
      break
    fi
  done
  ])
  openssl_base_dir=$ats_cv_openssl_dir
  if test "x$openssl_base_dir" = "x"; then
    enable_openssl=no
    AC_MSG_RESULT([not found])
  else
    enable_openssl=yes
    openssl_include="$openssl_base_dir/include"
    openssl_ldflags="$openssl_base_dir/lib"
    AC_MSG_RESULT([${openssl_base_dir}])
  fi
else
  if test -d $openssl_include/openssl && test -d $openssl_ldflags && test -f $openssl_include/openssl/x509.h; then
    AC_MSG_RESULT([ok])
  else
    AC_MSG_RESULT([not found])
  fi
fi

if test "$enable_openssl" != "no"; then
  saved_ldflags=$LDFLAGS
  saved_cppflags=$CPPFLAGS
  openssl_have_headers=0
  openssl_have_libs=0
  if test "$openssl_base_dir" != "/usr"; then
    TS_ADDTO(CPPFLAGS, [-I${openssl_include}])
    TS_ADDTO(LDFLAGS, [-L${openssl_ldflags}])
    TS_ADDTO(LIBTOOL_LINK_FLAGS, [-R${openssl_ldflags}])
  fi
  AC_CHECK_LIB(crypto, BN_init, AC_CHECK_LIB(ssl, SSL_accept, [openssl_have_libs=1],,-lcrypto))
  if test "$openssl_have_libs" != "0"; then
      AC_CHECK_HEADERS(openssl/x509.h, [openssl_have_headers=1])
  fi
  if test "$openssl_have_headers" != "0"; then
    AC_CHECK_DECLS([EVP_PKEY_CTX_new], [], [],
                   [#include <openssl/evp.h>])
    enable_crypto=yes
    AC_SUBST([LIBSSL],["-lssl -lcrypto"])
  else
    enable_openssl=no
    CPPFLAGS=$saved_cppflags
    LDFLAGS=$saved_ldflags
  fi
fi

])

AC_DEFUN([TS_CHECK_CRYPTO_NEXTPROTONEG], [
  AC_ARG_ENABLE([tls-npn],
  [AS_HELP_STRING([--enable-tls-npn],[enable NextProtocolNegotiation TLS extension support])],
  [ ],
  [
    enable_tls_npn=yes
    _npn_saved_LIBS=$LIBS
    TS_ADDTO(LIBS, [$LIBSSL])
    AC_CHECK_FUNCS(SSL_CTX_set_next_protos_advertised_cb SSL_CTX_set_next_proto_select_cb SSL_select_next_proto SSL_get0_next_proto_negotiated,
      [], [enable_tls_npn=no]
    )
    LIBS=$_npn_saved_LIBS
  ])

  AC_MSG_CHECKING(whether to enable NextProtocolNegotiation TLS extension support)
  AC_MSG_RESULT([$enable_tls_npn])
  TS_ARG_ENABLE_VAR([use], [tls-npn])
  AC_SUBST(use_tls_npn)
])
