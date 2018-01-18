cmake_minimum_required(VERSION 3.8)

project(palladio_root CXX)


### versioning

set(PLD_VERSION_MAJOR 1)
set(PLD_VERSION_MINOR 0)
set(PLD_VERSION_PATCH 0)
if(NOT PLD_VERSION_BUILD)
    set(PLD_VERSION_BUILD DEV)
endif()

# versioning follows semver.org, Semantic Versioning 2.0.0
# i.e. <major>.<minor>.<patch>+b<buildnr>
# TODO: include houdini/prt version into semver metadata? eg. append ".h16.0.705.ce1.9.1234"
set(PLD_VERSION "${PLD_VERSION_MAJOR}.${PLD_VERSION_MINOR}.${PLD_VERSION_PATCH}+b${PLD_VERSION_BUILD}")


### platform configuration

message(STATUS "CMAKE_SYSTEM_NAME = ${CMAKE_SYSTEM_NAME}")
if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
	set(PLD_WINDOWS 1)
	set(PLD_OS "win32")
	add_definitions(-DPLD_WINDOWS)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	set(PLD_LINUX 1)
	set(PLD_OS "rhel6")
	add_definitions(-DPLD_LINUX)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
	set(PLD_MACOS 1)
	set(PLD_OS "macos")
endif()
set(PLD_ARCH "x86_64")


### toolchain configuration

if(PLD_WINDOWS)
	set(PLD_TC "vc110")
	add_definitions(-DPLD_TC_VC) # TODO
elseif(PLD_LINUX)
	set(PLD_TC "gcc48")
	add_definitions(-DPLD_TC_GCC) # TODO
elseif(PLD_MACOS)
	set(PLD_TC "ac73")
	add_definitions(-DPLD_TC_CLANG) # TODO
endif()


### dependencies

include(${CMAKE_CURRENT_LIST_DIR}/dependencies.cmake)


### setup sub-projects

add_subdirectory(codec)
add_subdirectory(palladio)
add_subdirectory(test)

add_dependencies(palladio palladio_codec)


### setup installation target

if (NOT HOUDINI_DSO_PATH)
	if(PLD_WINDOWS)
		set(HOUDINI_DSO_PATH "$ENV{HOMEDRIVE}$ENV{HOMEPATH}/Documents/houdini${HOUDINI_MAJOR_VERSION}.${HOUDINI_MINOR_VERSION}/dso")
	else()
		set(HOUDINI_DSO_PATH "$ENV{HOME}/houdini${HOUDINI_MAJOR_VERSION}.${HOUDINI_MINOR_VERSION}/dso")
	endif()
endif()
message(STATUS "HOUDINI_DSO_PATH = ${HOUDINI_DSO_PATH}")

if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
	set (CMAKE_INSTALL_PREFIX "${HOUDINI_DSO_PATH}" CACHE PATH "default install path" FORCE )
	message(STATUS "set default install prefix: ${CMAKE_INSTALL_PREFIX}")
endif()

add_custom_target(install_palladio
    make install
    DEPENDS palladio
    COMMENT "Installing palladio")


### package targets

set(CPACK_PACKAGE_NAME                "palladio")
set(CPACK_PACKAGE_VENDOR              "Esri R&D Zurich / VRBN")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Houdini Plugin based on Esri CityEngine")
set(CPACK_PACKAGE_VERSION_MAJOR       ${PLD_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR       ${PLD_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH       ${PLD_VERSION_PATCH})
set(CPACK_PACKAGE_INSTALL_DIRECTORY   "palladio-${PLD_VERSION}")
set(CPACK_PACKAGE_FILE_NAME           "${CPACK_PACKAGE_NAME}-${PLD_VERSION}-${PLD_OS}-h${HOUDINI_VERSION}")

if(PLD_WINDOWS)
    set(CPACK_GENERATOR ZIP)
else()
    set(CPACK_GENERATOR TGZ)
endif()

include(CPack)

add_custom_target(package_palladio
    make package
    DEPENDS install_palladio
    COMMENT "Packaging palladio")
