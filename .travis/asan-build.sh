#!/bin/bash

pushd libopflex
./autogen.sh
./configure --enable-asan
make -j2
make check

sudo make install
popd

pushd genie
mvn compile exec:java
pushd target/libmodelgbp
bash autogen.sh
./configure
make
sudo make install
popd
popd

git clone https://github.com/openvswitch/ovs.git --branch v2.12.0 --depth 1
pushd ovs
./boot.sh
./configure  --enable-shared
make -j2
sudo make install
sudo mkdir -p /usr/local/include/openvswitch/openvswitch
sudo mv /usr/local/include/openvswitch/*.h /usr/local/include/openvswitch/openvswitch
sudo mv /usr/local/include/openflow /usr/local/include/openvswitch
sudo cp -t "/usr/local/include/openvswitch/" include/*.h
sudo find lib -name "*.h" -exec cp --parents -t "/usr/local/include/openvswitch/" {} \
popd

pushd agent-ovs
./autogen.sh
./configure --enable-asan;
make -j2
make check

