# houdini-cityengine-plugin

## Requirements
- [SideFX Houdini 16.0](https://sidefx.com/download) (apprentice edition is supported)
- [Esri CityEngine](http://www.esri.com/software/cityengine) with node-locked license
- RHEL 6/7 (or compatible) with GCC 4.8
- [cmake 3.8 or later](https://cmake.org/download)
- [conan 0.29.1 or later](https://www.conan.io/downloads)

## Build Instructions

### Preparation
1. ```git clone git@github.com:Esri/houdini-cityengine-plugin.git && cd houdini-cityengine-plugin```
1. fetch CityEngine SDK: ```cd conan/cesdk && conan create cesdk/1.9.3786@esri-rd-zurich/stable -s compiler=gcc -s compiler.version=4.8```
1. package Houdini (adjust XXX and YYY): ```cd conan/houdini && conan create houdini/16.0.XXX@sidefx/stable -s compiler=gcc -s compiler.version=4.8 -o houdini-flavor=YYY```
1. in the ```conanfile.txt``` adjust ```houdini-flavor``` to your houdini (apprentice, indie, core, fx)

### Building
1. ```mkdir -p build/release && cd build/release```
1. ```conan install ../../src```
1. ```cmake -DCMAKE_BUILD_TYPE=Release ../../src```
1. ```make install``` (the plugin is installed into your ~/houdini16/dso directory)

### Running
1. ```LD_LIBRARY_PATH=/opt/hfs16.0.XXX/dsolib happrentice``` (or your houdini flavor)
