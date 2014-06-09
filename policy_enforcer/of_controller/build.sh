#!/bin/bash
#
# May 2014, Abilash Menon
#
# This file is used to compile and install opflex agent executable
#
# Copyright (c) 2013-2014 by Cisco Systems, Inc.
#
# All rights reserved.
#------------------------------------------------------------------


function print_err {
    while [[ $# > 0 ]]; do
        echo ERROR: opflex build failed - $1
        shift
    done
}

function print_warn {
    while [[ $# > 0 ]]; do
        echo WARNING: opflex build - $1
        shift
    done
}

function usage {
    echo "This script does the following : "
    echo " a) Run autoreconf"
    echo " b) Invoke configure script"
    echo " c) Compile and build opflex agent"
    echo " d) Clean the opflex agent object files"
    echo ""
    echo "The command-line options allow any one of the above above to be run."
    echo "Invoking the script without any option will execute all of the "
    echo "above steps"
    echo "Options:"
    echo "  -a (Run autoreconf)"
    echo "  -b (Run configure)"
    echo "  -c (Run make)"
    echo "  -d (Run make clean (of agent))"
    echo "  -j <FACTOR> (Use -j factor when running make)"
    echo ""
    echo ""
    echo " The above options can also be specified together. For eg: to clean and then rebuild the"
    echo " external libraries, [./compile_ovs.sh -cf] can be used"
    echo ""
    exit 1;
}

run_all=1
while getopts abcdefguj:t:i: opt; do
    case $opt in
	a) run_all=; run_autoreconf=1;;
	b) run_all=; run_configure=1;;
	c) run_all=; run_make=1;;
	d) run_all=; run_make_clean=1;;
	j) jfactor=$OPTARG;;
	'?') if [[ ${@:$OPTIND-1:1} == -* ]]; then
	    usage
	    fi;;
    esac
done

if [[ ! ( ${jfactor:-1} =~ [0-9]+ ) ]]; then
    echo "ERROR: -j argument must be a number"
    usage
fi

DISTRO_CC=gcc
#.c4.5.3-p0

########################
# Set up the directories.
########################
OPFLEX_AGENT_SRC_TOP=`pwd`
OPFLEX_AGENT_OBJ_SUBDIR=obj-target
OPFLEX_AGENT_OBJ_PATH=$OPFLEX_AGENT_SRC_TOP/$OPFLEX_AGENT_OBJ_SUBDIR
if [[ ! -d $OPFLEX_AGENT_OBJ_PATH ]];then
    echo "Creating $OPFLEX_AGENT_OBJ_SUBDIR..."
    mkdir $OPFLEX_AGENT_OBJ_PATH
    chmod 777 $OPFLEX_AGENT_OBJ_PATH
fi

#############################
# Run make distclean
#############################

if [[ -n $run_all || -n $run_make_clean ]]; then
    cd $OPFLEX_AGENT_OBJ_PATH
    make distclean 2>&1| tee $OPFLEX_AGENT_OBJ_PATH/run_make_clean.log
    if [ $PIPESTATUS -ne 0 ]; then
        print_warn "Make distclean failed"
        print_warn "Check run_make.log"
    fi
fi

##############################
# Run autoreconf
##############################
if [[ -n $run_all || -n $run_autoreconf ]]; then
    cd $OPFLEX_AGENT_SRC_TOP
    autoreconf -vif 2>&1| tee $OPFLEX_AGENT_OBJ_PATH/run_autoreconf.log
    if [ $PIPESTATUS -ne 0 ]; then
	print_err "Autoreconf failed"
	print_err "Check run_autreconf.log oplfex/agent"
	exit 1
    fi
fi

##############################
# Run configure
##############################

if [[ -n $run_all || -n $run_configure ]]; then
    run_configure_log=run_configure.log
    cd $OPFLEX_AGENT_OBJ_PATH
    ../configure -C CC=$DISTRO_CC 2>&1| tee $OF_AGENT_OBJ_PATH/$run_configure_log
    if [ $PIPESTATUS -ne 0 ]; then
        print_err "Confgure failed"
        print_err "Check run_configure.log opflex agent"
        exit 1
    fi
fi

##############################
# Run make
##############################
if [[ -n $run_all || -n $run_make ]]; then
    # For lxe, use native libraries rather than libraries from container
    # staging rootfs, with following hack
    # - libxml2.so: use native version
    #   - native libxml2 is mis-installed in /usr/lib
    #     (missing /usr/lib/libxml2.so, so symlink to libxml2.so.2).
    #   - lxc (container) rootfs libxml2 is not compatible in practice,
    #     not just in theory.  For lxe case under CEL 5.03,
    #     using lxc rootfs version fails with
    #     "undefined reference to `__isoc99_sscanf@GLIBC_2.7"
    cd $OPFLEX_AGENT_OBJ_PATH
    make ${jfactor:+-j $jfactor} 2>&1| tee $OPFLEX_AGENT_OBJ_PATH/run_make.log
    if [ $PIPESTATUS -ne 0 ]; then
	print_err "Make failed"
	print_err "Check run_make.log"
	exit 1
    fi
fi
