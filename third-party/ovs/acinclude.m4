# -*- autoconf -*-

# Copyright (c) 2008, 2009, 2010, 2011, 2012, 2013, 2014 Nicira, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at:
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

dnl OVS_ENABLE_WERROR
AC_DEFUN([OVS_ENABLE_WERROR],
  [AC_ARG_ENABLE(
     [Werror],
     [AC_HELP_STRING([--enable-Werror], [Add -Werror to CFLAGS])],
     [], [enable_Werror=no])
   AC_CONFIG_COMMANDS_PRE(
     [if test "X$enable_Werror" = Xyes; then
        CFLAGS="$CFLAGS -Werror"
      fi])])

dnl OVS_CHECK_LINUX
dnl
dnl Configure linux kernel source tree 
AC_DEFUN([OVS_CHECK_LINUX], [
  AC_ARG_WITH([linux],
              [AC_HELP_STRING([--with-linux=/path/to/linux],
                              [Specify the Linux kernel build directory])])
  AC_ARG_WITH([linux-source],
              [AC_HELP_STRING([--with-linux-source=/path/to/linux-source],
                              [Specify the Linux kernel source directory
                               (usually figured out automatically from build
                               directory)])])

  # Deprecated equivalents to --with-linux, --with-linux-source.
  AC_ARG_WITH([l26])
  AC_ARG_WITH([l26-source])

  if test X"$with_linux" != X; then
    KBUILD=$with_linux
  elif test X"$with_l26" != X; then
    KBUILD=$with_l26
    AC_MSG_WARN([--with-l26 is deprecated, please use --with-linux instead])
  else
    KBUILD=
  fi

  if test X"$KBUILD" != X; then
    if test X"$with_linux_source" != X; then
      KSRC=$with_linux_source
    elif test X"$with_l26_source" != X; then
      KSRC=$with_l26_source
      AC_MSG_WARN([--with-l26-source is deprecated, please use --with-linux-source instead])
    else
      KSRC=
    fi
  elif test X"$with_linux_source" != X || test X"$with_l26_source" != X; then
    AC_MSG_ERROR([Linux source directory may not be specified without Linux build directory])
  fi

  if test -n "$KBUILD"; then
    KBUILD=`eval echo "$KBUILD"`
    case $KBUILD in
        /*) ;;
        *) KBUILD=`pwd`/$KBUILD ;;
    esac

    # The build directory is what the user provided.
    # Make sure that it exists.
    AC_MSG_CHECKING([for Linux build directory])
    if test -d "$KBUILD"; then
        AC_MSG_RESULT([$KBUILD])
        AC_SUBST(KBUILD)
    else
        AC_MSG_RESULT([no])
        AC_ERROR([source dir $KBUILD doesn't exist])
    fi

    # Debian breaks kernel headers into "source" header and "build" headers.
    # We want the source headers, but $KBUILD gives us the "build" headers.
    # Use heuristics to find the source headers.
    AC_MSG_CHECKING([for Linux source directory])
    if test -n "$KSRC"; then
      KSRC=`eval echo "$KSRC"`
      case $KSRC in
          /*) ;;
          *) KSRC=`pwd`/$KSRC ;;
      esac
      if test ! -e $KSRC/include/linux/kernel.h; then
        AC_MSG_ERROR([$KSRC is not a kernel source directory])
      fi
    else
      KSRC=$KBUILD
      if test ! -e $KSRC/include/linux/kernel.h; then
        # Debian kernel build Makefiles tend to include a line of the form:
        # MAKEARGS := -C /usr/src/linux-headers-3.2.0-1-common O=/usr/src/linux-headers-3.2.0-1-486
        # First try to extract the source directory from this line.
        KSRC=`sed -n 's/.*-C \([[^ ]]*\).*/\1/p' "$KBUILD"/Makefile`
        if test ! -e "$KSRC"/include/linux/kernel.h; then
          # Didn't work.  Fall back to name-based heuristics that used to work.
          case `echo "$KBUILD" | sed 's,/*$,,'` in # (
            */build)
              KSRC=`echo "$KBUILD" | sed 's,/build/*$,/source,'`
              ;; # (
            *)
              KSRC=`(cd $KBUILD && pwd -P) | sed 's,-[[^-]]*$,-common,'`
              ;;
          esac
        fi
      fi
      if test ! -e "$KSRC"/include/linux/kernel.h; then
        AC_MSG_ERROR([cannot find source directory (please use --with-linux-source)])
      fi
    fi
    AC_MSG_RESULT([$KSRC])

    AC_MSG_CHECKING([for kernel version])
    version=`sed -n 's/^VERSION = //p' "$KSRC/Makefile"`
    patchlevel=`sed -n 's/^PATCHLEVEL = //p' "$KSRC/Makefile"`
    sublevel=`sed -n 's/^SUBLEVEL = //p' "$KSRC/Makefile"`
    if test X"$version" = X || test X"$patchlevel" = X; then
       AC_ERROR([cannot determine kernel version])
    elif test X"$sublevel" = X; then
       kversion=$version.$patchlevel
    else
       kversion=$version.$patchlevel.$sublevel
    fi
    AC_MSG_RESULT([$kversion])

    if test "$version" -ge 3; then
       if test "$version" = 3 && test "$patchlevel" -le 14; then
          : # Linux 3.x
       else
         AC_ERROR([Linux kernel in $KBUILD is version $kversion, but version newer than 3.14.x is not supported (please refer to the FAQ for advice)])
       fi
    else
       if test "$version" -le 1 || test "$patchlevel" -le 5 || test "$sublevel" -le 31; then
         AC_ERROR([Linux kernel in $KBUILD is version $kversion, but version 2.6.32 or later is required])
       else
         : # Linux 2.6.x
       fi
    fi
    if (test ! -e "$KBUILD"/include/linux/version.h && \
        test ! -e "$KBUILD"/include/generated/uapi/linux/version.h)|| \
       (test ! -e "$KBUILD"/include/linux/autoconf.h && \
        test ! -e "$KBUILD"/include/generated/autoconf.h); then
        AC_MSG_ERROR([Linux kernel source in $KBUILD is not configured])
    fi
    OVS_CHECK_LINUX_COMPAT
  fi
  AM_CONDITIONAL(LINUX_ENABLED, test -n "$KBUILD")
])

dnl OVS_CHECK_DPDK
dnl
dnl Configure DPDK source tree
AC_DEFUN([OVS_CHECK_DPDK], [
  AC_ARG_WITH([dpdk],
              [AC_HELP_STRING([--with-dpdk=/path/to/dpdk],
                              [Specify the DPDK build directory])])

  if test X"$with_dpdk" != X; then
    RTE_SDK=$with_dpdk

    DPDK_INCLUDE=$RTE_SDK/include
    DPDK_LIB_DIR=$RTE_SDK/lib

    LDFLAGS="$LDFLAGS -L$DPDK_LIB_DIR"
    CFLAGS="$CFLAGS -I$DPDK_INCLUDE"

    # On some systems we have to add -ldl to link with dpdk
    #
    # This code, at first, tries to link without -ldl (""),
    # then adds it and tries again.
    # Before each attempt the search cache must be unset,
    # otherwise autoconf will stick with the old result

    found=false
    save_LIBS=$LIBS
    for extras in "" "-ldl"; do
        LIBS="-lintel_dpdk $extras $save_LIBS"
        AC_LINK_IFELSE(
           [AC_LANG_PROGRAM([#include <rte_config.h>
                             #include <rte_eal.h>],
                            [int rte_argc; char ** rte_argv;
                             rte_eal_init(rte_argc, rte_argv);])],
           [found=true])
        if $found; then
            break
        fi
    done
    if $found; then :; else
        AC_MSG_ERROR([cannot link with dpdk])
    fi

    AC_DEFINE([DPDK_NETDEV], [1], [System uses the DPDK module.])
  else
    RTE_SDK=
  fi

  AM_CONDITIONAL([DPDK_NETDEV], test -n "$RTE_SDK")
])

dnl OVS_GREP_IFELSE(FILE, REGEX, [IF-MATCH], [IF-NO-MATCH])
dnl
dnl Greps FILE for REGEX.  If it matches, runs IF-MATCH, otherwise IF-NO-MATCH.
dnl If IF-MATCH is empty then it defines to OVS_DEFINE(HAVE_<REGEX>), with
dnl <REGEX> translated to uppercase.
AC_DEFUN([OVS_GREP_IFELSE], [
  AC_MSG_CHECKING([whether $2 matches in $1])
  if test -f $1; then
    grep '$2' $1 >/dev/null 2>&1
    status=$?
    case $status in
      0) 
        AC_MSG_RESULT([yes])
        m4_if([$3], [], [OVS_DEFINE([HAVE_]m4_toupper([$2]))], [$3])
        ;;
      1) 
        AC_MSG_RESULT([no])
        $4
        ;;
      *) 
        AC_MSG_ERROR([grep exited with status $status])
        ;;
    esac
  else
    AC_MSG_RESULT([file not found])
    $4
  fi
])

dnl OVS_DEFINE(NAME)
dnl
dnl Defines NAME to 1 in kcompat.h.
AC_DEFUN([OVS_DEFINE], [
  echo '#define $1 1' >> datapath/linux/kcompat.h.new
])

AC_DEFUN([OVS_CHECK_LOG2_H], [
  AC_MSG_CHECKING([for $KSRC/include/linux/log2.h])
  if test -e $KSRC/include/linux/log2.h; then
    AC_MSG_RESULT([yes])
    OVS_DEFINE([HAVE_LOG2_H])
  else
    AC_MSG_RESULT([no])
  fi
])

dnl OVS_CHECK_LINUX_COMPAT
dnl
dnl Runs various Autoconf checks on the Linux 2.6 kernel source in
dnl the directory in $KBUILD.
AC_DEFUN([OVS_CHECK_LINUX_COMPAT], [
  rm -f datapath/linux/kcompat.h.new
  mkdir -p datapath/linux
  : > datapath/linux/kcompat.h.new

  OVS_GREP_IFELSE([$KSRC/arch/x86/include/asm/checksum_32.h], [src_err,],
                  [OVS_DEFINE([HAVE_CSUM_COPY_DBG])])

  OVS_GREP_IFELSE([$KSRC/include/linux/err.h], [ERR_CAST])

  OVS_GREP_IFELSE([$KSRC/include/linux/etherdevice.h], [eth_hw_addr_random])
  OVS_GREP_IFELSE([$KSRC/include/linux/etherdevice.h], [ether_addr_copy])

  OVS_GREP_IFELSE([$KSRC/include/linux/if_vlan.h], [vlan_set_encap_proto])

  OVS_GREP_IFELSE([$KSRC/include/linux/in.h], [ipv4_is_multicast])

  OVS_GREP_IFELSE([$KSRC/include/linux/netdevice.h], [dev_disable_lro])
  OVS_GREP_IFELSE([$KSRC/include/linux/netdevice.h], [dev_get_stats])
  OVS_GREP_IFELSE([$KSRC/include/linux/netdevice.h], [dev_get_by_index_rcu])
  OVS_GREP_IFELSE([$KSRC/include/linux/netdevice.h], [__skb_gso_segment])
  OVS_GREP_IFELSE([$KSRC/include/linux/netdevice.h], [can_checksum_protocol])
  OVS_GREP_IFELSE([$KSRC/include/linux/netdevice.h], [netdev_features_t])
  OVS_GREP_IFELSE([$KSRC/include/linux/netdevice.h], [pcpu_sw_netstats])

  OVS_GREP_IFELSE([$KSRC/include/linux/random.h], [prandom_u32])

  OVS_GREP_IFELSE([$KSRC/include/linux/rcupdate.h], [rcu_read_lock_held], [],
                  [OVS_GREP_IFELSE([$KSRC/include/linux/rtnetlink.h],
                                   [rcu_read_lock_held])])
  OVS_GREP_IFELSE([$KSRC/include/linux/rtnetlink.h], [lockdep_rtnl_is_held])
  
  # Check for the proto_data_valid member in struct sk_buff.  The [^@]
  # is necessary because some versions of this header remove the
  # member but retain the kerneldoc comment that describes it (which
  # starts with @).  The brackets must be doubled because of m4
  # quoting rules.
  OVS_GREP_IFELSE([$KSRC/include/linux/skbuff.h], [[[^@]]proto_data_valid],
                  [OVS_DEFINE([HAVE_PROTO_DATA_VALID])])
  OVS_GREP_IFELSE([$KSRC/include/linux/skbuff.h], [rxhash])
  OVS_GREP_IFELSE([$KSRC/include/linux/skbuff.h], [skb_dst(],
                  [OVS_DEFINE([HAVE_SKB_DST_ACCESSOR_FUNCS])])
  OVS_GREP_IFELSE([$KSRC/include/linux/skbuff.h], 
                  [skb_copy_from_linear_data_offset])
  OVS_GREP_IFELSE([$KSRC/include/linux/skbuff.h],
                  [skb_reset_tail_pointer])
  OVS_GREP_IFELSE([$KSRC/include/linux/skbuff.h], [skb_cow_head])
  OVS_GREP_IFELSE([$KSRC/include/linux/skbuff.h], [skb_transport_header],
                  [OVS_DEFINE([HAVE_SKBUFF_HEADER_HELPERS])])
  OVS_GREP_IFELSE([$KSRC/include/linux/icmpv6.h], [icmp6_hdr],
                  [OVS_DEFINE([HAVE_ICMP6_HDR])])
  OVS_GREP_IFELSE([$KSRC/include/linux/skbuff.h], [skb_warn_if_lro],
                  [OVS_DEFINE([HAVE_SKB_WARN_LRO])])
  OVS_GREP_IFELSE([$KSRC/include/linux/skbuff.h], [consume_skb])
  OVS_GREP_IFELSE([$KSRC/include/linux/skbuff.h], [skb_frag_page])
  OVS_GREP_IFELSE([$KSRC/include/linux/skbuff.h], [skb_has_frag_list])
  OVS_GREP_IFELSE([$KSRC/include/linux/skbuff.h], [__skb_fill_page_desc])
  OVS_GREP_IFELSE([$KSRC/include/linux/skbuff.h], [skb_reset_mac_len])
  OVS_GREP_IFELSE([$KSRC/include/linux/skbuff.h], [skb_unclone])
  OVS_GREP_IFELSE([$KSRC/include/linux/skbuff.h], [skb_orphan_frags])
  OVS_GREP_IFELSE([$KSRC/include/linux/skbuff.h], [skb_get_hash])
  OVS_GREP_IFELSE([$KSRC/include/linux/skbuff.h], [skb_clear_hash])
  OVS_GREP_IFELSE([$KSRC/include/linux/skbuff.h], [l4_rxhash])

  OVS_GREP_IFELSE([$KSRC/include/linux/types.h], [bool],
                  [OVS_DEFINE([HAVE_BOOL_TYPE])])
  OVS_GREP_IFELSE([$KSRC/include/linux/types.h], [__wsum],
                  [OVS_DEFINE([HAVE_CSUM_TYPES])])
  OVS_GREP_IFELSE([$KSRC/include/uapi/linux/types.h], [__wsum],
                  [OVS_DEFINE([HAVE_CSUM_TYPES])])

  OVS_GREP_IFELSE([$KSRC/include/net/checksum.h], [csum_replace4])
  OVS_GREP_IFELSE([$KSRC/include/net/checksum.h], [csum_unfold])

  OVS_GREP_IFELSE([$KSRC/include/net/genetlink.h], [parallel_ops])
  OVS_GREP_IFELSE([$KSRC/include/net/gre.h], [gre_cisco_register])
  OVS_GREP_IFELSE([$KSRC/include/net/netlink.h], [nla_get_be16])
  OVS_GREP_IFELSE([$KSRC/include/net/netlink.h], [nla_put_be16])
  OVS_GREP_IFELSE([$KSRC/include/net/netlink.h], [nla_put_be32])
  OVS_GREP_IFELSE([$KSRC/include/net/netlink.h], [nla_put_be64])
  OVS_GREP_IFELSE([$KSRC/include/net/netlink.h], [nla_find_nested])

  OVS_GREP_IFELSE([$KSRC/include/net/sctp/checksum.h], [sctp_compute_cksum])

  OVS_GREP_IFELSE([$KSRC/include/linux/if_vlan.h], [ADD_ALL_VLANS_CMD],
                  [OVS_DEFINE([HAVE_VLAN_BUG_WORKAROUND])])

  OVS_GREP_IFELSE([$KSRC/include/linux/percpu.h], [this_cpu_ptr])

  OVS_GREP_IFELSE([$KSRC/include/linux/openvswitch.h], [openvswitch_handle_frame_hook],
                  [OVS_DEFINE([HAVE_RHEL_OVS_HOOK])])

  OVS_CHECK_LOG2_H

  if cmp -s datapath/linux/kcompat.h.new \
            datapath/linux/kcompat.h >/dev/null 2>&1; then
    rm datapath/linux/kcompat.h.new
  else
    mv datapath/linux/kcompat.h.new datapath/linux/kcompat.h
  fi
])

dnl Checks for net/if_packet.h.
AC_DEFUN([OVS_CHECK_IF_PACKET],
  [AC_CHECK_HEADER([net/if_packet.h],
                   [HAVE_IF_PACKET=yes],
                   [HAVE_IF_PACKET=no])
   AM_CONDITIONAL([HAVE_IF_PACKET], [test "$HAVE_IF_PACKET" = yes])
   if test "$HAVE_IF_PACKET" = yes; then
      AC_DEFINE([HAVE_IF_PACKET], [1],
                [Define to 1 if net/if_packet.h is available.])
   fi])

dnl Checks for net/if_dl.h.
dnl
dnl (We use this as a proxy for checking whether we're building on FreeBSD
dnl or NetBSD.)
AC_DEFUN([OVS_CHECK_IF_DL],
  [AC_CHECK_HEADER([net/if_dl.h],
                   [HAVE_IF_DL=yes],
                   [HAVE_IF_DL=no])
   AM_CONDITIONAL([HAVE_IF_DL], [test "$HAVE_IF_DL" = yes])
   if test "$HAVE_IF_DL" = yes; then
      AC_DEFINE([HAVE_IF_DL], [1],
                [Define to 1 if net/if_dl.h is available.])

      # On these platforms we use libpcap to access network devices.
      AC_SEARCH_LIBS([pcap_open_live], [pcap])
   fi])

dnl Checks for buggy strtok_r.
dnl
dnl Some versions of glibc 2.7 has a bug in strtok_r when compiling
dnl with optimization that can cause segfaults:
dnl
dnl http://sources.redhat.com/bugzilla/show_bug.cgi?id=5614.
AC_DEFUN([OVS_CHECK_STRTOK_R],
  [AC_CACHE_CHECK(
     [whether strtok_r macro segfaults on some inputs],
     [ovs_cv_strtok_r_bug],
     [AC_RUN_IFELSE(
        [AC_LANG_PROGRAM([#include <stdio.h>
                          #include <string.h>
                         ],
                         [[char string[] = ":::";
                           char *save_ptr = (char *) 0xc0ffee;
                           char *token1, *token2;
                           token1 = strtok_r(string, ":", &save_ptr);
                           token2 = strtok_r(NULL, ":", &save_ptr);
                           freopen ("/dev/null", "w", stdout);
                           printf ("%s %s\n", token1, token2);
                           return 0;
                          ]])],
        [ovs_cv_strtok_r_bug=no],
        [ovs_cv_strtok_r_bug=yes],
        [ovs_cv_strtok_r_bug=yes])])
   if test $ovs_cv_strtok_r_bug = yes; then
     AC_DEFINE([HAVE_STRTOK_R_BUG], [1],
               [Define if strtok_r macro segfaults on some inputs])
   fi
])

dnl ----------------------------------------------------------------------
dnl These macros are from GNU PSPP, with the following original license:
dnl Copyright (C) 2005, 2006, 2007 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

AC_DEFUN([_OVS_CHECK_CC_OPTION], [dnl
  m4_define([ovs_cv_name], [ovs_cv_[]m4_translit([$1], [-=], [__])])dnl
  AC_CACHE_CHECK([whether $CC accepts $1], [ovs_cv_name], 
    [ovs_save_CFLAGS="$CFLAGS"
     dnl Include -Werror in the compiler options, because without -Werror
     dnl clang's GCC-compatible compiler driver does not return a failure
     dnl exit status even though it complains about options it does not
     dnl understand.
     CFLAGS="$CFLAGS $WERROR $1"
     AC_COMPILE_IFELSE([AC_LANG_PROGRAM(,)], [ovs_cv_name[]=yes], [ovs_cv_name[]=no])
     CFLAGS="$ovs_save_CFLAGS"])
  if test $ovs_cv_name = yes; then
    m4_if([$2], [], [:], [$2])
  else
    m4_if([$3], [], [:], [$3])
  fi
])

dnl OVS_CHECK_WERROR
dnl
dnl Check whether the C compiler accepts -Werror.
dnl Sets $WERROR to "-Werror", if so, and otherwise to the empty string.
AC_DEFUN([OVS_CHECK_WERROR],
  [WERROR=
   _OVS_CHECK_CC_OPTION([-Werror], [WERROR=-Werror])])

dnl OVS_CHECK_CC_OPTION([OPTION], [ACTION-IF-ACCEPTED], [ACTION-IF-REJECTED])
dnl Check whether the given C compiler OPTION is accepted.
dnl If so, execute ACTION-IF-ACCEPTED, otherwise ACTION-IF-REJECTED.
AC_DEFUN([OVS_CHECK_CC_OPTION],
  [AC_REQUIRE([OVS_CHECK_WERROR])
   _OVS_CHECK_CC_OPTION([$1], [$2], [$3])])

dnl OVS_ENABLE_OPTION([OPTION])
dnl Check whether the given C compiler OPTION is accepted.
dnl If so, add it to WARNING_FLAGS.
dnl Example: OVS_ENABLE_OPTION([-Wdeclaration-after-statement])
AC_DEFUN([OVS_ENABLE_OPTION], 
  [OVS_CHECK_CC_OPTION([$1], [WARNING_FLAGS="$WARNING_FLAGS $1"])
   AC_SUBST([WARNING_FLAGS])])

dnl OVS_CONDITIONAL_CC_OPTION([OPTION], [CONDITIONAL])
dnl Check whether the given C compiler OPTION is accepted.
dnl If so, enable the given Automake CONDITIONAL.

dnl Example: OVS_CONDITIONAL_CC_OPTION([-Wno-unused], [HAVE_WNO_UNUSED])
AC_DEFUN([OVS_CONDITIONAL_CC_OPTION],
  [OVS_CHECK_CC_OPTION(
    [$1], [ovs_have_cc_option=yes], [ovs_have_cc_option=no])
   AM_CONDITIONAL([$2], [test $ovs_have_cc_option = yes])])
dnl ----------------------------------------------------------------------

dnl Check for too-old XenServer.
AC_DEFUN([OVS_CHECK_XENSERVER_VERSION],
  [AC_CACHE_CHECK([XenServer release], [ovs_cv_xsversion],
    [if test -e /etc/redhat-release; then
       ovs_cv_xsversion=`sed -n 's/^XenServer DDK release \([[^-]]*\)-.*/\1/p' /etc/redhat-release`
     fi
     if test -z "$ovs_cv_xsversion"; then
       ovs_cv_xsversion=none
     fi])
  case $ovs_cv_xsversion in
    none)
      ;;

    [[1-9]][[0-9]]* |                    dnl XenServer 10 or later
    [[6-9]]* |                           dnl XenServer 6 or later
    5.[[7-9]]* |                         dnl XenServer 5.7 or later
    5.6.[[1-9]][[0-9]][[0-9]][[0-9]]* |  dnl XenServer 5.6.1000 or later
    5.6.[[2-9]][[0-9]][[0-9]]* |         dnl XenServer 5.6.200 or later
    5.6.1[[0-9]][[0-9]])                 dnl Xenserver 5.6.100 or later
      ;;

    *)
      AC_MSG_ERROR([This appears to be XenServer $ovs_cv_xsversion, but only XenServer 5.6.100 or later is supported.  (If you are really using a supported version of XenServer, you may override this error message by specifying 'ovs_cv_xsversion=5.6.100' on the "configure" command line.)])
      ;;
  esac])

dnl OVS_MAKE_HAS_IF([if-true], [if-false])
dnl
dnl Checks whether make has the GNU make $(if condition,then,else) extension.
dnl Runs 'if-true' if so, 'if-false' otherwise.
AC_DEFUN([OVS_CHECK_MAKE_IF],
  [AC_CACHE_CHECK(
     [whether ${MAKE-make} has GNU make \$(if) extension],
     [ovs_cv_gnu_make_if],
     [cat <<'EOF' > conftest.mk
conftest.out:
	echo $(if x,y,z) > conftest.out
.PHONY: all
EOF
      rm -f conftest.out
      AS_ECHO(["$as_me:$LINENO: invoking ${MAKE-make} -f conftest.mk all:"]) >&AS_MESSAGE_LOG_FD 2>&1
      ${MAKE-make} -f conftest.mk conftest.out >&AS_MESSAGE_LOG_FD 2>&1
      AS_ECHO(["$as_me:$LINENO: conftest.out contains:"]) >&AS_MESSAGE_LOG_FD 2>&1
      cat conftest.out >&AS_MESSAGE_LOG_FD 2>&1
      result=`cat conftest.out`
      rm -f conftest.mk conftest.out
      if test "X$result" = "Xy"; then
        ovs_cv_gnu_make_if=yes
      else
        ovs_cv_gnu_make_if=no
      fi])])

dnl OVS_CHECK_GNU_MAKE
dnl
dnl Checks whether make is GNU make (because Linux kernel Makefiles
dnl only work with GNU make).
AC_DEFUN([OVS_CHECK_GNU_MAKE],
  [AC_CACHE_CHECK(
     [whether ${MAKE-make} is GNU make],
     [ovs_cv_gnu_make],
     [rm -f conftest.out
      AS_ECHO(["$as_me:$LINENO: invoking ${MAKE-make} --version:"]) >&AS_MESSAGE_LOG_FD 2>&1
      ${MAKE-make} --version >conftest.out 2>&1
      cat conftest.out >&AS_MESSAGE_LOG_FD 2>&1
      result=`cat conftest.out`
      rm -f conftest.mk conftest.out

      case $result in # (
        GNU*) ovs_cv_gnu_make=yes ;; # (
        *) ovs_cv_gnu_make=no ;;
      esac])
   AM_CONDITIONAL([GNU_MAKE], [test $ovs_cv_gnu_make = yes])])

dnl OVS_CHECK_SPARSE_TARGET
dnl
dnl The "cgcc" script from "sparse" isn't very good at detecting the
dnl target for which the code is being built.  This helps it out.
AC_DEFUN([OVS_CHECK_SPARSE_TARGET],
  [AC_CACHE_CHECK(
    [target hint for cgcc],
    [ac_cv_sparse_target],
    [AS_CASE([`$CC -dumpmachine 2>/dev/null`],
       [i?86-* | athlon-*], [ac_cv_sparse_target=x86],
       [x86_64-*], [ac_cv_sparse_target=x86_64],
       [ac_cv_sparse_target=other])])
   AS_CASE([$ac_cv_sparse_target],
     [x86], [SPARSEFLAGS= CGCCFLAGS=-target=i86],
     [x86_64], [SPARSEFLAGS=-m64 CGCCFLAGS=-target=x86_64],
     [SPARSEFLAGS= CGCCFLAGS=])
   AC_SUBST([SPARSEFLAGS])
   AC_SUBST([CGCCFLAGS])])

dnl OVS_SPARSE_EXTRA_INCLUDES
dnl
dnl The cgcc script from "sparse" does not search gcc's default
dnl search path. Get the default search path from GCC and pass
dnl them to sparse.
AC_DEFUN([OVS_SPARSE_EXTRA_INCLUDES],
    AC_SUBST([SPARSE_EXTRA_INCLUDES],
           [`$CC -v -E - </dev/null 2>&1 >/dev/null | sed -n -e '/^#include.*search.*starts.*here:/,/^End.*of.*search.*list\./s/^ \(.*\)/-I \1/p' |grep -v /usr/lib | grep -x -v '\-I /usr/include' | tr \\\n ' ' `] ))

dnl OVS_ENABLE_SPARSE
AC_DEFUN([OVS_ENABLE_SPARSE],
  [AC_REQUIRE([OVS_CHECK_SPARSE_TARGET])
   AC_REQUIRE([OVS_CHECK_MAKE_IF])
   AC_REQUIRE([OVS_SPARSE_EXTRA_INCLUDES])
   : ${SPARSE=sparse}
   AC_SUBST([SPARSE])
   AC_CONFIG_COMMANDS_PRE(
     [if test $ovs_cv_gnu_make_if = yes; then
        CC='$(if $(C),env REAL_CC="'"$CC"'" CHECK="$(SPARSE) -I $(top_srcdir)/include/sparse $(SPARSEFLAGS) $(SPARSE_EXTRA_INCLUDES) " cgcc $(CGCCFLAGS),'"$CC"')'
      fi])])

dnl OVS_PTHREAD_SET_NAME
dnl
dnl This checks for three known variants of pthreads functions for setting
dnl the name of the current thread:
dnl
dnl   glibc: int pthread_setname_np(pthread_t, const char *name);
dnl   NetBSD: int pthread_setname_np(pthread_t, const char *format, void *arg);
dnl   FreeBSD: int pthread_set_name_np(pthread_t, const char *name);
dnl
dnl For glibc and FreeBSD, the arguments are just a thread and its name.  For
dnl NetBSD, 'format' is a printf() format string and 'arg' is an argument to
dnl provide to it.
dnl
dnl This macro defines:
dnl
dnl    glibc: HAVE_GLIBC_PTHREAD_SETNAME_NP
dnl    NetBSD: HAVE_NETBSD_PTHREAD_SETNAME_NP
dnl    FreeBSD: HAVE_PTHREAD_SET_NAME_NP
AC_DEFUN([OVS_CHECK_PTHREAD_SET_NAME],
  [AC_CHECK_FUNCS([pthread_set_name_np])
   if test $ac_cv_func_pthread_set_name_np != yes; then
     AC_CACHE_CHECK(
       [for pthread_setname_np() variant],
       [ovs_cv_pthread_setname_np],
       [AC_LINK_IFELSE(
         [AC_LANG_PROGRAM([#include <pthread.h>
  ], [pthread_setname_np(pthread_self(), "name");])],
         [ovs_cv_pthread_setname_np=glibc],
         [AC_LINK_IFELSE(
           [AC_LANG_PROGRAM([#include <pthread.h>
], [pthread_setname_np(pthread_self(), "%s", "name");])],
           [ovs_cv_pthread_setname_np=netbsd],
           [ovs_cv_pthread_setname_np=none])])])
     case $ovs_cv_pthread_setname_np in # (
       glibc)
          AC_DEFINE(
            [HAVE_GLIBC_PTHREAD_SETNAME_NP], [1],
            [Define to 1 if pthread_setname_np() is available and takes 2 parameters (like glibc).])
          ;; # (
       netbsd)
          AC_DEFINE(
            [HAVE_NETBSD_PTHREAD_SETNAME_NP], [1],
            [Define to 1 if pthread_setname_np() is available and takes 3 parameters (like NetBSD).])
          ;;
     esac
   fi])

dnl OVS_CHECK_LINUX_HOST.
dnl
dnl Checks whether we're building for a Linux host, based on the presence of
dnl the __linux__ preprocessor symbol, and sets up an Automake conditional
dnl LINUX based on the result.
AC_DEFUN([OVS_CHECK_LINUX_HOST],
  [AC_CACHE_CHECK(
     [whether __linux__ is defined],
     [ovs_cv_linux],
     [AC_COMPILE_IFELSE(
        [AC_LANG_PROGRAM([enum { LINUX = __linux__};], [])],
        [ovs_cv_linux=true],
        [ovs_cv_linux=false])])
   AM_CONDITIONAL([LINUX], [$ovs_cv_linux])])
