#!/bin/sh

# libopflex: a framework for developing opflex-based policy agents
# Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v1.0 which accompanies this distribution,
# and is available at http://www.eclipse.org/legal/epl-v10.html

# This autogen script will run the autotools to generate the build
# system.  You should run this script in order to initialize a build
# immediately following a checkout.

for i in m4/*
do
    if [ -L "${i}" ]
    then
        rm "${i}"
    fi
done

autoreconf -fis
