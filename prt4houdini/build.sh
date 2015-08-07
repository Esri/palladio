#!/bin/bash

if [ $# -eq 0 ]
then
	echo "ERROR: usage: ./build.sh <MODE>: MODE==0: incremental build, MODE==1: clean build, MODE==2: build releaseable package"
	exit 1
fi

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

houdini_DIR="/opt/hfs14.0.361"
export PATH="${houdini_DIR}/bin:${PATH}" # to get the custom houdini makefiles to work

MODE=$1
BUILDTYPE=RelWithDebInfo

if [ $MODE -eq "0" ]; then
	CLIENT_TARGET=install
elif [ $MODE -eq "1" ]; then
	CLIENT_TARGET=install
elif [ $MODE -eq "2" ]; then
  	CLIENT_TARGET=package
fi

#export CC=/usr/bin/gcc-4.4.7
#export CXX=/usr/bin/g++-4.4.7

pushd codec
if [ $MODE -ne "0" ]; then
	rm -rf build install
fi
mkdir -p build
pushd build
cmake -DCMAKE_BUILD_TYPE=${BUILDTYPE} ../src
make install
popd
popd

pushd client
if [ $MODE -ne "0" ]; then
	rm -rf build install
fi
mkdir -p build
pushd build
cmake -Dhoudini_DIR=${houdini_DIR} -DCMAKE_BUILD_TYPE=${BUILDTYPE}  ../src
make ${CLIENT_TARGET}
popd
popd
