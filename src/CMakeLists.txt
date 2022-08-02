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
include(${CMAKE_CURRENT_LIST_DIR}/cmake/packaging.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/installation.cmake)


### setup sub-projects

add_subdirectory(codec)
add_subdirectory(palladio)
add_subdirectory(palladio_fs)
add_dependencies(palladio palladio_codec palladio_fs)

add_subdirectory(test EXCLUDE_FROM_ALL)


### setup package target

pld_setup_packaging()


### setup test target

enable_testing ()
add_test (NAME palladio_test COMMAND palladio_test)