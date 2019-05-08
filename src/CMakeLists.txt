cmake_minimum_required(VERSION 3.13)

project(palladio_root CXX)


### platform configuration

if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
	set(PLD_WINDOWS 1)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	set(PLD_LINUX 1)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
	set(PLD_MACOS 1)
endif()


### toolchain configuration

function(add_toolchain_definition TGT)
	if(PLD_WINDOWS)
		target_compile_definitions(${TGT} PRIVATE -DPLD_WINDOWS=1 -DPLD_TC_VC=1)
	elseif(PLD_LINUX)
		target_compile_definitions(${TGT} PRIVATE -DPLD_LINUX=1 -DPLD_TC_GCC=1)
	elseif(PLD_MACOS)
		target_compile_definitions(${TGT} PRIVATE -DPLD_MACOS=1 -DPLD_TC_CLANG=1)
	endif()
endfunction()


### dependencies

include(${CMAKE_CURRENT_LIST_DIR}/dependencies.cmake)


### versioning

set(PLD_VERSION_MAJOR 1)
set(PLD_VERSION_MINOR 6)
set(PLD_VERSION_PATCH 0-beta.1)
if(NOT PLD_VERSION_BUILD)
    set(PLD_VERSION_BUILD DEV)
endif()

# we use semver.org, Semantic Versioning 2.0.0
# i.e. <major>.<minor>.<patch>+b<buildnr>.hdn<houdini ver>.prt<prt ver>
set(PLD_VERSION "${PLD_VERSION_MAJOR}.${PLD_VERSION_MINOR}.${PLD_VERSION_PATCH}\
+b${PLD_VERSION_BUILD}\
.hdn${Houdini_VERSION_MAJOR}-${Houdini_VERSION_MINOR}-${Houdini_VERSION_PATCH}\
.prt${PRT_VERSION_MAJOR}-${PRT_VERSION_MINOR}-${PRT_VERSION_MICRO}")
message(STATUS "Using PLD_VERSION = ${PLD_VERSION}")


### setup sub-projects

add_subdirectory(codec)
add_subdirectory(palladio)
add_dependencies(palladio palladio_codec)

add_subdirectory(test EXCLUDE_FROM_ALL)


### setup installation target

if (NOT HOUDINI_DSO_PATH)
	if(PLD_WINDOWS)
		set(HOUDINI_DSO_PATH "$ENV{HOMEDRIVE}$ENV{HOMEPATH}/Documents/houdini${Houdini_VERSION_MAJOR}.${Houdini_VERSION_MINOR}/dso")
	else()
		set(HOUDINI_DSO_PATH "$ENV{HOME}/houdini${Houdini_VERSION_MAJOR}.${Houdini_VERSION_MINOR}/dso")
	endif()
endif()
message(STATUS "HOUDINI_DSO_PATH = ${HOUDINI_DSO_PATH}")

if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
	set (CMAKE_INSTALL_PREFIX "${HOUDINI_DSO_PATH}" CACHE PATH "default install path" FORCE )
	message(STATUS "Set default install prefix: ${CMAKE_INSTALL_PREFIX}")
endif()


### setup package target

set(CPACK_PACKAGE_NAME                "palladio")
set(CPACK_PACKAGE_VENDOR              "Esri R&D Zurich / VRBN")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Houdini Plugin based on Esri CityEngine")
set(CPACK_PACKAGE_VERSION_MAJOR       ${PLD_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR       ${PLD_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH       ${PLD_VERSION_PATCH})
set(CPACK_PACKAGE_INSTALL_DIRECTORY   "palladio-${PLD_VERSION}")

if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
	set(PLD_PKG_OS "windows")
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	set(PLD_PKG_OS "linux")
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
	set(PLD_PKG_OS "macos")
endif()
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${PLD_VERSION}-${PLD_PKG_OS}")

if(PLD_WINDOWS)
    set(CPACK_GENERATOR ZIP)
else()
    set(CPACK_GENERATOR TGZ)
endif()

include(CPack)

# not strictly necessary, useful for IDEs like CLion without direct support to run "make package"
add_custom_target(palladio_package
	${CMAKE_MAKE_PROGRAM} package
    DEPENDS palladio
    COMMENT "Packaging Palladio")


### setup test target

enable_testing ()
add_test (NAME palladio_test COMMAND palladio_test)