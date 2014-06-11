#!/bin/bash

#  Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
#
#  This program and the accompanying materials are made available under the
#  terms of the Eclipse Public License v1.0 which accompanies this distribution,
#  and is available at http://www.eclipse.org/legal/epl-v10.html
#

build_dep_libuv() {
    msgout "INFO" "libuv autogen.sh"
    sh autogen.sh || die "Can't run libuv's autogen.sh"
    msgout "INFO" "libuv configure"
    ./configure || die "Can't run libuv's configure"
    msgout "INFO" "libuv make"
    make || die "Can't run libuv's make"
}

build_dep() {
    dep=${1:-""}
    cd ${dep} || die "Can't enter deps/${dep}"
    msgout "INFO" "${dep}"
    eval build_dep_${dep}
    cd ..
}

build_deps() {
    cd deps || die "Can't cd into deps/"
    build_dep libuv || die "Can't build libuv"
    cd ..
}

