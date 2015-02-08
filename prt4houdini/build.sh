#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

prt_DIR=${DIR}/../prt/cmake
BUILDTYPE=Release

if [ $# -eq 0 ]
then
	echo "ERROR: first argument must be houdini path"
	exit 1
fi

export houdini_DIR=$1
export PATH=${houdini_DIR}/bin:${PATH}

if [ $# -eq 1 ]
then
	CLIENT_TARGET=install
	VER_MAJOR=0
	VER_MINOR=0
	VER_MICRO=0
else
  	CLIENT_TARGET=package
	VER_MAJOR=$2
	VER_MINOR=$3
	VER_MICRO=$4
fi

export CC=/usr/bin/gcc-4.4.7
export CXX=/usr/bin/g++-4.4.7

pushd codec
rm -rf build install && mkdir build
pushd build
cmake -Dprt_DIR=${prt_DIR} -DCMAKE_BUILD_TYPE=${BUILDTYPE} ../src
make install
popd
popd

pushd client
rm -rf build install && mkdir build
pushd build
cmake -Dprt_DIR=${prt_DIR} -DCMAKE_BUILD_TYPE=${BUILDTYPE} -DPRT4HOUDINI_VERSION_MAJOR=${VER_MAJOR} -DPRT4HOUDINI_VERSION_MINOR=${VER_MINOR} -DPRT4HOUDINI_VERSION_MICRO=${VER_MICRO} ../src
make ${CLIENT_TARGET}
popd
popd
