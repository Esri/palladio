cmake_minimum_required(VERSION 3.13)

project(palladio_root CXX)


### global definitions

set(PRT_RELATIVE_EXTENSION_PATH "prtlib")
set(HOUDINI_RELATIVE_DSO_PATH "dso")
set(HOUDINI_RELATIVE_ICON_PATH "config/Icons")
set(HOUDINI_RELATIVE_PACKAGES_PATH "packages")
set(HOUDINI_RELATIVE_PALLADIO_PATH "${HOUDINI_RELATIVE_PACKAGES_PATH}/palladio")


### versioning

set(PLD_VERSION_MAJOR 1)
set(PLD_VERSION_MINOR 10)
set(PLD_VERSION_PATCH 0)
set(PLD_VERSION_PRE "-dev.0") # set to empty string for final releases
if(NOT PLD_VERSION_BUILD)
	set(PLD_VERSION_BUILD DEV)
endif()


### utils & dependencies

include(${CMAKE_CURRENT_LIST_DIR}/cmake/common.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/dependencies.cmake)


### setup installation target

if (NOT HOUDINI_USER_PATH)
	if(PLD_WINDOWS)
		set(HOUDINI_USER_PATH "$ENV{HOMEDRIVE}$ENV{HOMEPATH}/Documents/houdini${Houdini_VERSION_MAJOR}.${Houdini_VERSION_MINOR}")
	else()
		set(HOUDINI_USER_PATH "$ENV{HOME}/houdini${Houdini_VERSION_MAJOR}.${Houdini_VERSION_MINOR}")
	endif()
endif()

set(HOUDINI_DSO_PATH "${HOUDINI_USER_PATH}/${HOUDINI_RELATIVE_DSO_PATH}")
set(HOUDINI_ICON_PATH "${HOUDINI_USER_PATH}/${HOUDINI_RELATIVE_ICON_PATH}")
set(HOUDINI_PACKAGES_PATH "${HOUDINI_USER_PATH}/${HOUDINI_RELATIVE_PACKAGES_PATH}")

message(STATUS "HOUDINI_DSO_PATH = ${HOUDINI_DSO_PATH}")
message(STATUS "HOUDINI_ICON_PATH = ${HOUDINI_ICON_PATH}")
message(STATUS "HOUDINI_PACKAGES_PATH = ${HOUDINI_PACKAGES_PATH}")

if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
	set (CMAKE_INSTALL_PREFIX "${HOUDINI_USER_PATH}" CACHE PATH "default install path" FORCE )
	message(STATUS "Set default install prefix: ${CMAKE_INSTALL_PREFIX}")
endif()


### setup sub-projects

add_subdirectory(codec)
add_subdirectory(palladio)
add_subdirectory(palladio_fs)
add_dependencies(palladio palladio_codec palladio_fs)

add_subdirectory(test EXCLUDE_FROM_ALL)


### setup package target

set(CPACK_PACKAGE_NAME                "palladio")
set(CPACK_PACKAGE_VENDOR              "Esri R&D Zurich / VRBN")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Houdini Plugin based on Esri CityEngine")
set(CPACK_PACKAGE_VERSION_MAJOR       ${PLD_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR       ${PLD_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH       ${PLD_VERSION_PATCH}${PLD_VERSION_PRE})
set(CPACK_PACKAGE_INSTALL_DIRECTORY   "palladio-${PLD_VERSION}")

if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
	set(PLD_PKG_OS "windows")
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	set(PLD_PKG_OS "linux")
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