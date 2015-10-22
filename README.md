# houdini-cityengine-plugin

## Requirements
- Houdini 14.0
- CityEngine Node-Locked License
- Windows 7 or later with Visual Studio 2012 Update 4
- RHEL6 (and compatible) with GCC 4.8
- cmake 3.2 or later

## Build Instructions
1. clone the repo
2. (windows only) open a visual studio 2012 x64 shell
3. ```cd <repo root>/prt4houdini```
4. make sure the houdini "bin" directory is on the system path
5. download boost dependency from here:
   https://github.com/Esri/houdini-cityengine-plugin/releases/tag/0.01
6. ```mkdir depcache``` and unzip the downloaded boost in there 
7. ```mkdir build && cd build```
8. ```cmake -DHOUDINI_ROOT=/opt/hfs14.0.444 -DCMAKE_BUILD_TYPE=Release ../src``` (adjust houdini path to your system)
9. ```make install``` (simply iterate on this one when working on the code)
