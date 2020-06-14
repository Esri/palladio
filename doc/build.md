# Build and Run Palladio


## Build Requirements

### Supported Operating Systems
* RedHat Enterprise Linux 7 (and compatible)
* Windows 7/8.1/10
 
### Required Toolchain & Compiler
* [cmake 3.13 or later](https://cmake.org/download)
* [conan 1.20 or later](https://www.conan.io/downloads)
* Linux: GCC 6.3
* Windows: Visual Studio 2017 (MSVC 14.1)

### Required Build Dependencies
* Installation of Houdini 17.0 or later (see https://sidefx.com/download)

The following will be automatically fetched via the bootstrap steps below: 
* [Esri CityEngine SDK](https://github.com/Esri/esri-cityengine-sdk)
* SideFX Houdini HDK


## Build Instructions

Default is Houdini 18.0. See below how to build for different Houdini versions.

### Bootstrap

The below steps will populate your local Conan repository with dependencies for the Palladio build system. You only need to work through this section once (or if you want to upgrade one of the dependencies).

#### Linux
1. Checkout Palladio: `git clone git@github.com:esri/palladio.git && cd palladio`
1. Download CityEngine SDK: `conan create -pr conan/profiles/linux-gcc63 conan/cesdk cesdk/2.1.5704@esri-rd-zurich/stable`
1. Extract and package the HDK from your local Houdini 18 installation (adjust Z to your Houdini version): `conan create -pr conan/profiles/linux-gcc63 conan/houdini houdini/18.0.Z@sidefx/stable` (Note: use the option `-e HOUDINI_INSTALL=/path/to/your/hfs18.0.Z`, if Houdini is not installed at the standard location, e.g. at `/opt/hfs18.0.Z` for Linux).

#### Windows
1. Checkout Palladio: `git clone git@github.com:esri/palladio.git`
1. Open a Windows command shell and `cd` to the Palladio git repository
1. Download CityEngine SDK: `conan create -pr conan/profiles/windows-v141 conan/cesdk cesdk/2.1.5704@esri-rd-zurich/stable`
1. Extract and package the HDK from your local Houdini installation (adjust Z to your Houdini version): `conan create -pr conan/profiles/windows-v141 conan/houdini houdini/18.0.Z@sidefx/stable` (Note: use the option `-e HOUDINI_INSTALL=C:/path/to/your/houdini/installation`, if Houdini is not installed at the standard location for Windows).

### Building Palladio

Note: to build for another Houdini version, add the cmake argument `-DPLD_HOUDINI_VERSION=X.Y`.

#### Linux
1. Ensure GCC 6.3 is active.
1. `cd` into your Palladio git repository
1. ```mkdir -p build/release && cd build/release```
1. ```cmake -DCMAKE_BUILD_TYPE=Release ../../src```
1. ```make install``` (the plugin will be installed into your ~/houdini18.0/dso directory)

#### Windows
1. Open a MSVC 14.1 x64 shell (Visual Studio 2017) and `cd` to the Palladio git repository
1. ```mkdir build/release```
1. ```cd build/release```
1. ```cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release ../../src```
1. ```nmake install``` (the plugin will be installed into your ~/houdini18.0/dso directory)

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
1. Open a MSVC 14.1 x64 shell (Visual Studio 2017) and `cd` to the Palladio git repository
1. ```mkdir build/relTest```
1. ```cd build/relTest```
1. ```cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DPLD_TEST=1 ../../src```
1. ```nmake palladio_test```
1. ensure that the `bin` subdirectory of your Houdini installation is in the `PATH`
1. run `bin\palladio_test`
