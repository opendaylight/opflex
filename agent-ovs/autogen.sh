#!/bin/sh
#
# libopflex: a framework for developing opflex-based policy agents
# Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v1.0 which accompanies this distribution,
# and is available at http://www.eclipse.org/legal/epl-v10.html
#
###########
#
# This autogen script will run the autotools to generate the build
# system.  You should run this script in order to initialize a build
# immediately following a checkout.

for LIBTOOLIZE in libtoolize glibtoolize nope; do
    $LIBTOOLIZE --version 2>&1 > /dev/null && break
done

if [ "x$LIBTOOLIZE" = "xnope" ]; then
    echo
    echo "You must have libtool installed to compile this package."
    echo "Download the appropriate package for your distribution,"
    echo "or get the source tarball at ftp://ftp.gnu.org/pub/gnu/"
    exit 1
fi

ACLOCALDIRS= 
if [ -d  /usr/share/aclocal ]; then
    ACLOCALDIRS="-I /usr/share/aclocal"
fi
if [ -d  /usr/local/share/aclocal ]; then
    ACLOCALDIRS="$ACLOCALDIRS -I /usr/local/share/aclocal"
fi

$LIBTOOLIZE --automake --force && \
aclocal --force $ACLOCALDIRS && \
autoconf --force $ACLOCALDIRS && \
autoheader --force && \
automake --add-missing --foreign && \
echo "You may now configure the software by running ./configure" && \
echo "Run ./configure --help to get information on the options " && \
echo "that are available."

