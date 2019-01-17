# Palladio

## Build Requirements
- RHEL/CentOS 6/7 (or compatible) with GCC 4.8
- or Windows 7/8.1/10 with Visual Studio 2015 (MSVC 14.0)
- Installation of [Houdini 16.5](https://sidefx.com/download)
- [cmake 3.13 or later](https://cmake.org/download)
- [conan 1.11 or later](https://www.conan.io/downloads)

The bootstrap steps below will take care of these additional dependencies: 
- SideFX Houdini HDK 16.5
- [Esri CityEngine SDK 1.9](https://github.com/Esri/esri-cityengine-sdk)
- Boost

## Build Instructions

### Bootstrap

The below steps will populate your local Conan repository with dependencies for the Palladio build system. You only need to work through this section once (or if you want to upgrade one of the dependencies).

#### Linux
1. Checkout Palladio:```git clone git@github.com:esri/palladio.git && cd palladio```
1. Download CityEngine SDK: ```conan create -s compiler=gcc -s compiler.version=4.8 conan/cesdk cesdk/1.9.3786@esri-rd-zurich/stable ```
1. Extract and package the HDK from your local Houdini installation (adjust Z to your Houdini version): ```conan create -s compiler=gcc -s compiler.version=4.8 conan/houdini houdini/16.5.Z@sidefx/stable``` (Note: use the option ``-e HOUDINI_INSTALL=/path/to/your/hfs16.5.Z``, if Houdini is not installed at the standard location, e.g. at ``/opt/hfs16.5.Z`` for Linux).

#### Windows
1. Checkout Palladio git repository
1. Open a MSVC 14.0 x64 shell (Visual Studio 2015) and `cd` to the Palladio git repository
1. Download CityEngine SDK: ```conan create conan/cesdk cesdk/1.9.3786@esri-rd-zurich/stable ```
1. Extract and package the HDK from your local Houdini installation (adjust Z to your Houdini version): ```conan create conan/houdini houdini/16.5.Z@sidefx/stable``` (Note: use the option ``-e HOUDINI_INSTALL=C:/path/to/your/houdini/installation``, if Houdini is not installed at the standard location for Windows).

### Building Palladio

#### Linux
1. `cd` into your Palladio git repository
1. ```mkdir -p build/release && cd build/release```
1. ```cmake -DCMAKE_BUILD_TYPE=Release ../../src```
1. ```make install``` (the plugin will be installed into your ~/houdini16.5/dso directory)

Note: the `Debug` build type should also work out of the box. If you want to use the `RelWithDebInfo` build type you need to edit your `~/.conan/settings.yml` file and append `RelWithDebInfo` to the `build_type` array at the bottom.

#### Windows
1. Open a MSVC 14.0 x64 shell (Visual Studio 2015) and `cd` to the Palladio git repository
1. ```mkdir build/release```
1. ```cd build/release```
1. ```cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release ../../src```
1. ```nmake install``` (the plugin will be installed into your ~/houdini16.5/dso directory)

Note: if you run Visual Studio 2017 you can enable the MSVC 14.0 (VS2015) toolchain as follows:
1. Use the Visual Studio Installer to install `VC++ 2015.3 v140 toolset for Desktop` (see `Individual Components`)
1. Open a cmd shell and run `%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Professional\VC\Auxiliary\Build\vcvarsall.bat" x64 8.1 -vcvars_ver=14.0`
1. `cl.exe` should now have version `19.00.xxx`

### Running Palladio
See [Quick Start](usage.md) how to launch Houdini with Palladio.

### Building and Running Unit Tests

#### Linux
1. `cd` into your Palladio git repository
1. ```mkdir -p build/relTest && cd build/relTest```
1. ```cmake -DCMAKE_BUILD_TYPE=Release -DPLD_TEST=1 ../../src```
1. ```make palladio_test```
1. run `bin/palladio_test`

#### Windows
1. Open a MSVC 14.0 x64 shell (Visual Studio 2015) and `cd` to the Palladio git repository
1. ```mkdir build/relTest```
1. ```cd build/relTest```
1. ```cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DPLD_TEST=1 ../../src```
1. ```nmake palladio_test```
1. ensure that the `bin` subdirectory of your Houdini installation is in the `PATH`
1. run `bin\palladio_test`


## Environment Variables

- `CITYENGINE_LOG_LEVEL`: controls global (minimal) log level for all assign and generate nodes. Valid values are "debug", "info", "warning", "error", "fatal"
- `CITYENGINE_LICENSE_SERVER`: specifies how the CityEngine license is acquired. An empty string specifies a node-locked license while `<port>@<host>` indicates a network license server, e.g. `27000@my.server.org`.
- `HOUDINI_DSO_ERROR`: useful to debug loading issues, see http://www.sidefx.com/docs/houdini/ref/env

