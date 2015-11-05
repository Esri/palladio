# houdini-cityengine-plugin

## Requirements
- Houdini hfs14.0.444 or later
- CityEngine Node-Locked License
- Windows 7 or later with Visual Studio 2012 Update 4
- RHEL6 (and compatible) with GCC 4.8
- cmake 3.2 or later

## Build Instructions

### Windows
1. make sure the houdini "bin" directory is on the system path
2. clone the repo
3. (windows only) open a visual studio 2012 x64 shell
4. ```cd <repo root>/prt4houdini```
5. ```mkdir build && cd build```
6. ```cmake -G "NMake Makefiles" -DHOUDINI_ROOT=C:\somewhere\hfs14.0.444 -DCMAKE_BUILD_TYPE=Release ../src``` (adjust HOUDINI_ROOT to your system)
7. ```nmake install``` (simply iterate on this one when working on the code)

### Linux
1. make sure the houdini "bin" directory is on the system path
2. clone the repo
3. ```cd <repo root>/prt4houdini```
4. ```mkdir build && cd build```
5. ```cmake -DHOUDINI_ROOT=/opt/hfs14.0.444 -DCMAKE_BUILD_TYPE=Release ../src``` (adjust HOUDINI_ROOT to your system)
6. ```make install``` (simply iterate on this one when working on the code)
