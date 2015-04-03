# houdini-cityengine-plugin

## Requirements
- Houdini 13.0
- CityEngine SDK (see esri-cityengine-sdk on github.com)
- CityEngine License
- Linux Box (e.g. RHEL6)
- GCC 4.4

## Building the Plugin
1. clone the repo
2. ```cd <repo root>/prt4houdini```
3. ```./build.sh <path to ce sdk cmake subdir> <path to houdini root folder>```, for example ```./build.sh ~/dev/esri_ce_sdk/cmake/ /opt/hfs13.0.509/```

