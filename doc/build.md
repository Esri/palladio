# Build and Run Palladio

## Build Requirements

### Supported Operating Systems
- RHEL/CentOS 6/7 (or compatible)
- Windows 7/8.1/10 
 
### Dependencies
* Installation of Houdini 16.5 or Houdini 17.0 (see https://sidefx.com/download)
* Installation of CityEngine 2017.1

### Toolchain & Compiler
* [cmake 3.13 or later](https://cmake.org/download)
* [conan 1.11 or later](https://www.conan.io/downloads)

#### Houdini 16.5
* Linux: GCC 4.8
* Windows: Visual Studio 2015 (MSVC 14.0)

#### Houdini 17.0
* Linux: GCC 6.3
* Windows: Visual Studio 2017 (MSVC 14.1)

### Dependencies
The bootstrap steps below will take care of these additional dependencies: 
* [Esri CityEngine SDK 1.9](https://github.com/Esri/esri-cityengine-sdk)
* SideFX Houdini HDK 16.5 (part of [Houdini 16.5]())
* Boost (only for Houdini 16.5)

## Build Instructions

Default is Houdini 17. See embedded notes below for Houdini 16.5.

### Bootstrap

The below steps will populate your local Conan repository with dependencies for the Palladio build system. You only need to work through this section once (or if you want to upgrade one of the dependencies).

Note: to build for Houdini 16.5, switch the conan profile to gcc48 or msvc v140.

#### Linux
1. Checkout Palladio: `git clone git@github.com:esri/palladio.git && cd palladio`
1. Download CityEngine SDK: `conan create -pr conan/profiles/linux-gcc63 conan/cesdk cesdk/1.9.3786@esri-rd-zurich/stable`
1. Extract and package the HDK from your local Houdini 17 installation (adjust Z to your Houdini version): `conan create -pr conan/profiles/linux-gcc63 conan/houdini houdini/17.0.Z@sidefx/stable` (Note: use the option `-e HOUDINI_INSTALL=/path/to/your/hfs17.0.Z`, if Houdini is not installed at the standard location, e.g. at `/opt/hfs17.0.Z` for Linux).

#### Windows
1. Checkout Palladio: `git clone git@github.com:esri/palladio.git`
1. Open a Windows command shell and `cd` to the Palladio git repository
1. Download CityEngine SDK: `conan create -pr conan/profiles/windows-v141 conan/cesdk cesdk/1.9.3786@esri-rd-zurich/stable`
1. Extract and package the HDK from your local Houdini installation (adjust Z to your Houdini version): `conan create -pr conan/profiles/windows-v141 conan/houdini houdini/17.0.Z@sidefx/stable` (Note: use the option `-e HOUDINI_INSTALL=C:/path/to/your/houdini/installation`, if Houdini is not installed at the standard location for Windows).

### Building Palladio

Note: to build for Houdini 16.5, add cmake argument `-DPLD_HOUDINI_VERSION=16.5`. For Windows, open a MSVC 14.0 shell (Visual Studio 2015). For Linux, ensure that GCC 4.8 is active.

#### Linux
1. Ensure GCC 6.3 is active.
1. `cd` into your Palladio git repository
1. ```mkdir -p build/release && cd build/release```
1. ```cmake -DCMAKE_BUILD_TYPE=Release ../../src```
1. ```make install``` (the plugin will be installed into your ~/houdini17.0/dso directory)

Note: the `Debug` build type should also work out of the box. If you want to use the `RelWithDebInfo` build type you need to edit your `~/.conan/settings.yml` file and append `RelWithDebInfo` to the `build_type` array at the bottom.

#### Windows
1. Open a MSVC 14.1 x64 shell (Visual Studio 2017) and `cd` to the Palladio git repository
1. ```mkdir build/release```
1. ```cd build/release```
1. ```cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release ../../src```
1. ```nmake install``` (the plugin will be installed into your ~/houdini17.0/dso directory)

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

