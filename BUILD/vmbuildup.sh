#!/bin/bash
# 
#  Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
# 
#  This program and the accompanying materials are made available under the
#  terms of the Eclipse Public License v1.0 which accompanies this distribution,
#  and is available at http://www.eclipse.org/legal/epl-v10.html
# 

build_install_sw() {
    local url=$1
    local name=$2
    wget -nv $url

    local fname=""
    for d in `ls`; do 
	if [ `echo $d | grep $name | grep  ".gz"` ]; then 
	    fname="$d" 
	fi; 
    done
    tar -zxvf $fname
    for d in `ls`; do 
	if [ `echo $d | grep $name | grep  -v ".gz"` ]; then 
	    fname="$d" 
	fi
    done
    cd $fname
    ./configure
    make
    sudo make install
    return 0
}

echo "Script to help in the setup of the development environment"
echo " on a new system, VM or bare metal."

echo "getting the env......."
UPD_CMD="yum"
if [ -f "/etc/redhat-release" ]; then
    UPD_CMD="yum"
    echo "Install the epel...."
    cd /tmp
    wget http://dl.fedoraproject.org/pub/epel/6/x86_64/epel-release-6-8.noarch.rpm
    wget http://rpms.famillecollet.com/enterprise/remi-release-6.rpm
    sudo rpm -Uvh remi-release-6*.rpm epel-release-6*.rpm
else
    UPD_CMD="apt-get"  
fi

mkdir -p ~/devl

echo "updating....."
sudo $UPD_CMD update -y

echo "installing packages......"
sudo $UPD_CMD install ssh git htop emacs vim build-essential python-setuptools python2.7-dev libzip2 libzip-dev libxml2 libpq-dev libxslt1-dev python-ncrypt openssl openssl-devel libmysqlclient-dev python-pip readline readline-devl -y
if [ -f "/etc/redhat-release" ]; then
    sudo yum remove autoconf libtool automake -y
    cwd="$PWD"
    cd /tmp
    echo "+++++++++++++++ Building autoconf tools"
    build_install_sw "http://ftp.gnu.org/gnu/autoconf/autoconf-latest.tar.gz" "autoconf"
    
    echo "++++++++++++++++++++ Building libtools"
    build_install_sw "http://ftp.gnu.org/gnu/libtool/libtool-2.4.tar.gz" "libtool"

    echo "+++++++++++++++++++ Building automake tools"
    build_install_sw "http://ftp.gnu.org/gnu/automake/automake-1.14.tar.gz" "automake"

    echo "+++++++++++++++++++ Building cmake"
    wget -nv http://www.cmake.org/files/v2.8/cmake-2.8.12.2.tar.gz
    tar -zxvf cmake-2.8.12.2.tar.gz
    cd cmake-2.8.12.2
    ./bootstrap --prefix=/usr
    make
    sudo make install
    cmake --version
    cd /tmp

    echo "+++++++++++++++++++ Building cmocka"
    wget -nv https://open.cryptomilk.org/attachments/download/37/cmocka-0.4.0.tar.xz
    tar -xvf cmocka-0.4.0.tar.xz
    cd cmocka-0.4.0
    mkdir -p build
    cd build
    cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Debug ..
    make
    sudo make install

    cd $cwd
else
    sudo $UPD_CMD install autoconf libtool automake
fi
sudo $UPD_CMD install gcc-c++ -y


echo "Better reboot, complete."
