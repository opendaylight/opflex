#!/bin/bash
PROG=$(basename $0)
SCRIPT_NAME=$(basename $0)
SCRIPT_HOME=$(dirname $0)
BASE_DIR="/${PWD#*/}"
VERBOSE=""
DIR0=""
DIR1=""

function show_options () {
    echo "Usage: $SCRIPT_NAME [options]"
    echo
    echo "Check headers"
    echo
    echo "Options:"
    echo "    --dir1 --perform the make on the full tree."
    echo "    --dir2 -- runs the etags, a must for emacs"
    echo "    --verbose - just what it says"
    echo
    exit $1
}

usage() {
    local rtncode=$1
    echo "Usage: from the code root $SCRIPT_NAME"
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

TEMP=`getopt -o s:f:nvh --dir0:,dir2,help,verbose:: -n $SCRIPT_NAME -- "$@"`
if [ $? != 0 ] ; then echo "Terminating..." >&2 ; exit 1 ; fi

# Note the quotes around `$TEMP': they are essential!                                                                                                      
eval set -- "$TEMP"


while true ; do
    case "$1" in
        -n|--dir0)
            case "$2" in
                *) DIR0=$2; shift 2 ;;
            esac ;;
        -t|--dir1)
            case "$2" in
                *) DIR1=$2; shift 2 ;;
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



files0=`ls $dir0/*.h`

for f in $files0; do
    fn=`basename ${f}`
    diff "dir0\$fn" "dir1\$fn"
    
done