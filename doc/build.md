# Palladio

## Build Requirements
- RHEL/CentOS 6/7 (or compatible) with GCC 4.8
- [cmake 3.8 or later](https://cmake.org/download)
- [conan 1.0.1 or later](https://www.conan.io/downloads)

The bootstrap will take care of these additional dependencies: 
- [SideFX Houdini HDK 16](https://sidefx.com/download)
- [Esri CityEngine SDK 1.9](https://github.com/Esri/esri-cityengine-sdk)

## Build Instructions

### Bootstrap
The below steps will populate your local Conan repository with dependencies for the Palladio build system. You only need to work through this section once (or if you want to upgrade one of the dependencies). 
1. Checkout Palladio:```git clone git@github.com:esri/palladio.git && cd palladio```
1. Download (from github) and package CityEngine SDK: ```cd conan/cesdk && conan create . cesdk/1.9.3786@esri-rd-zurich/stable -s compiler=gcc -s compiler.version=4.8```
1. Extract and package the HDK from your local Houdini installation (adjust X.Y.Z): ```cd conan/houdini && conan create . houdini/X.Y.Z@sidefx/stable -s compiler=gcc -s compiler.version=4.8``` (Note: use the option ``-o /path/to/your/hfsX.Y.Z``, if Houdini is not installed at the standard location, e.g. at ``/opt/hfsX.Y.Z`` for Linux).

### Building Palladio
1. ```mkdir -p build/release && cd build/release```
1. ```conan install ../../src```
1. ```cmake -DCMAKE_BUILD_TYPE=Release ../../src```
1. ```make install``` (the plugin is installed into your ~/houdiniX.Y/dso directory)

### Running Palladio
See [Quick Start](usage.md) how to launch Houdini with Palladio.

## Environment Variables

- CITYENGINE_LOG_LEVEL: controls global (minimal) log level for all assign and generate nodes. valid values are "debug", "info", "warning", "error", "fatal"
- CITYENGINE_LICENSE_SERVER: specify how CityEngine license is acquired. an empty string specifies a node-locked license while "<port>@<host>" indicates a network license server, e.g. "27000@my.server.org".
- HOUDINI_DSO_ERROR: useful to debug loading issues, see http://www.sidefx.com/docs/houdini/ref/env