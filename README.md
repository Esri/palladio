# houdini-cityengine-plugin

## Requirements
- Houdini hfs14.0.444 or later
- CityEngine Node-Locked License
- Windows 7 or later with Visual Studio 2012 Update 4
- RHEL6 (and compatible) with GCC 4.8
- cmake 3.2 or later
- RHEL only: Boost 1.55 (Houdini does not include all boost libs)

## Build Instructions

### Windows
1. clone the repo
2. open a visual studio 2012 x64 shell
3. ```cd <repo root>/prt4houdini```
4. ```mkdir build && cd build```
5. ```cmake -G "NMake Makefiles" -DHOUDINI_BUILD_VERSION=xxx -DCMAKE_BUILD_TYPE=RelWithDebInfo ../src``` (adjust xxx to your houdini 14.0.xxx version)
6. ```nmake install``` (simply iterate on this one when working on the code)

### Linux
1. clone the repo
2. ```cd <repo root>/prt4houdini```
3. ```mkdir build && cd build```
4. ```cmake -DHOUDINI_ROOT=/opt/hfs14.0.444 -DCMAKE_BUILD_TYPE=Release ../src``` (adjust HOUDINI_ROOT to your system)
5. ```make install``` (simply iterate on this one when working on the code)
