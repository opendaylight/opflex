#!/bin/bash
# 
#  Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
# 
#  This program and the accompanying materials are made available under the
#  terms of the Eclipse Public License v1.0 which accompanies this distribution,
#  and is available at http://www.eclipse.org/legal/epl-v10.html
# 

PROG=$(basename $0)
SCRIPT_NAME=$(basename $0)
SCRIPT_HOME=$(dirname $0)
BASE_DIR="/${PWD#*/}"
VERBOSE=""

function show_options () {
    echo "Usage: $SCRIPT_NAME [options]"
    echo
    echo "Build the program environment"
    echo
    echo "Options:"
    echo "    --force-ovs-build -- even though the OVS libs exist,"
    echo "                         rebuild OVS."
    echo "    --no-make --perform the make on the full tree."
    echo "    --tags -- runs the etags, a must for emacs"
    echo "    --stop-stage <stage> -- runs through the defined stage"
    echo "          choices are: ovs, aclocal, autoheader,"
    echo "          libtoolize, automake, autoconf."
    echo "    --verbose - just what it says"
    echo
    exit $1
}

usage() {
    local rtncode=$1
    echo "Usage: from the code root ./BUILD/$SCRIPT_NAME"
    exit $rtncode
    return 0
}

msgout() {
    local level=$1
    local str=$2
    local tm=`date +"%Y-%m-%d %H:%M:%S"`
    if [ $level = "DEBUG" ] && [ -z $VERBOSE ]; then
            return 0
    else
        echo "$tm: $PROG [$$]: $1: $str"
    fi

    return 0
}

die() { echo "$@"; exit 1; }

if [ "${PWD##*/}" == "BUILD" ]; then 
    msgout "ERROR" "THis must be run from the root directory"
    usage -1
fi    

DO_MAKE="yes"
DO_TAGS="n"
BUILD_OVS="no"
STOP_STAGE=""
TEMP=`getopt -o s:tnvhf --long stop-stage:,no-make::,tags::,help,force-ovs-build,verbose:: -n $SCRIPT_NAME -- "$@"`
if [ $? != 0 ] ; then echo "Terminating..." >&2 ; exit 1 ; fi

# Note the quotes around `$TEMP': they are essential!                                                                                                      
eval set -- "$TEMP"


while true ; do
    case "$1" in
        -f|--force-ovs-build)
            case "$2" in
                "") BUILD_OVS="yes"; shift 1 ;;
                *) BUILD_OVS="yes"; shift 1 ;;
            esac ;;
        -n|--no-make)
            case "$2" in
                "") DO_MAKE="no"; shift 2;;
                *) DO_MAKE=$2; shift 2 ;;
            esac ;;
        -t|--tags)
            case "$2" in
                "") DO_TAGS="yes"; shift 2;;
                *) DO_TAGS=$2; shift 2 ;;
            esac ;;
	-s|--stop-stage)
           case "$2" in
                "") STOP_STAGE=""; shift 2;;
                *) STOP_STAGE=$2; shift 2;;
            esac ;;
	-v|--verbose)
           case "$2" in
                "") VERBOSE="True"; shift 2;;
                *) VERBOSE=$2; shift 2;;
            esac ;;
        -h|--help) show_options 0;;
        --) shift ; break ;;
        *) echo "Error: unsupported option $1." ; exit 1 ;;
    esac
done

msgout "INFO" "Starting...."

# Handle "glibtoolize" (e.g., for native OS X autotools) as another
# name for "libtoolize". Use the first one, either name, found in PATH.
LIBTOOLIZE=libtoolize  # Default
IFS="${IFS=   }"; save_ifs="$IFS"; IFS=':'
for dir in $PATH
do
  if test -x $dir/glibtoolize
  then
    LIBTOOLIZE=glibtoolize
    break
  elif test -x $dir/libtoolize
  then
    break
  fi
done
IFS="$save_ifs"

# Build OVS
if [ ! -f third-party/ovs/lib/libopenvswitch.la -o \
     ! -f third-party/ovs/ovsdb/libovsdb.la -o \
     "$BUILD_OVS" = "yes" ]
then
    msgout "INFO" "First, build OVS."
    origDIR=`pwd`
    msgout "INFO" "Currently in `pwd`"
    cd third-party/ovs
    ovsBuildDIR=`pwd`
    msgout "INFO" "Entered `pwd`"
    make distclean
    # We need our vlog.h
    msgout "INFO" "Copying OpFlex vlog.h into OVS"
    mv -v lib/vlog.h lib/orig.vlog.h
    cp -v ../vlog.h ./lib
    # Build ovs
    cd $ovsBuildDIR
    msgout "INFO" "Running boot.sh in $ovsBuildDIR"
    ./boot.sh || die "Can't run ovs/boot.sh"
    msgout "INFO" "Running configure in $ovsBuildDIR"
# Running configure with --prefix=/usr --localstatedir=/var
# but this should be variablized with these options as the
# default.
    ./configure --prefix=/usr --sysconfdir=/etc --localstatedir=/var || die "Can't run ovs/configure"
    msgout "INFO" "Running make in $ovsBuildDIR"
    make || die "Can't make ovs"
    cd $origDIR
fi
#end BUILD ovs

if [ "$STOP_STAGE" == ovs ]; then
    msgout "INFO" "Stop-stage set at ovs."
    exit 0
fi

msgout "INFO" "make distclean: clean out everyting."
make distclean

AC4="$BASE_DIR/aclocal.m4"
msgout "INFO" "Running aclocal: creating the $AC4"
aclocal || die "Can't execute aclocal"
if [ "$?" != "0" ]; then
    msgout "ERROR" "There are aclocal errors."
    exit -1
fi
if [ -f "$AC4" ]; then
    msgout "INFO" "aclocal complete: created $AC4"
else
    msgout "ERROR" "$AC4 not created"
    exit -1
fi

if [ "$STOP_STAGE" == "aclocal" ]; then
    msgout "INFO" "Stop-stage set at $STOP_STAGE, Complete."
    exit 0
fi
CONF_HDR="$BASE_DIR/config.h.in"
msgout "INFO" "Running autoheader: generates hdr template from configure.ac, config.h.in"
autoheader || die "Can't execute autoheader"
if [ "$?" != "0" ]; then
    msgout "ERROR" "There are autoheader errors."
    exit -1
fi
if [ -f "$CONF_HDR" ]; then
    msgout "INFO" "autoheader complete: created $CONF_HDR"
else
    msgout "ERROR" "$CONF_HDR not created"
    exit -1
fi

if [ "$STOP_STAGE" == "autoheader" ]; then
    msgout "INFO" "Stop-stage set at $STOP_STAGE, Complete."
    exit 0
fi
# --force means overwrite ltmain.sh script if it already exists
msgout "INFO" 'Running libtoolize: adds files to distribution that are required by macros from `libtool.m4'
$LIBTOOLIZE --automake --force --copy || die "Can't execute libtoolize"
if [ "$?" != "0" ]; then
    msgout "ERROR" "There are libtoolize errors."
    exit -1
fi

# --add-missing instructs automake to install missing auxiliary files
# and --force to overwrite them if they already exist
if [ "$STOP_STAGE" == "libtoolize" ]; then
    msgout "INFO" "Stop-stage set at $STOP_STAGE, Complete."
    exit 0
fi
MAKEFILE="$BASE_DIR/Makefile.in"
msgout "INFO" "Running automake --automake --force --copy: generate standard makefile templates."
automake --add-missing --force  --copy || die "ERROR: Can't execute automake"
if [ "$?" != "0" ]; then
    msgout "ERROR" "There are automake errors."
    exit -1
fi
if [ -f "$MAKEFILE" ]; then
    msgout "INFO" "automake complete: created $MAKEFILE"
else
    msgout "ERROR" "$MAKEFILE not created"
    exit -1
fi

if [ "$STOP_STAGE" == "automake" ]; then
    msgout "INFO" "Stop-stage set at $STOP_STAGE, Complete."
    exit 0
fi
msgout "INFO" 'Running autoconf'
autoconf || die "Can't execute autoconf"
if [ "$STOP_STAGE" == "autoconf" ]; then
    msgout "INFO" "Stop-stage set at $STOP_STAGE, Complete."
    exit 0
fi

msgout "INFO" "Running the ./configure"
./configure || die "Can't execute configure"
if [ "$?" != "0" ]; then
    msgout "ERROR" "There are configure errors."
    exit -1
fi


if [ "$DO_TAGS" == "yes" ]; then
    msgout "INFO" 'Running makeTags '
    $BASE_DIR/BUILD/makeTags 
fi


if [ "$DO_MAKE" == "yes" ]; then
    msgout "INFO" "make $AM_MAKEFLAGS"
    make $AM_MAKEFLAGS
fi

msgout "INFO" "==============================="
msgout "INFO" "Complete."
