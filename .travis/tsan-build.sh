#!/bin/bash

pushd libopflex; ./autogen.sh;./configure --enable-tsan ;make;make check;
popd

pushd genie; mvn compile exec:java; cd target;
bash autogen.sh; ./configure;make
popd

pushd agent-ovs; ./autogen.sh;./configure --enable-tsan;make; make check;
