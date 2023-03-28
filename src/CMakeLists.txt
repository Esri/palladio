cmake_minimum_required(VERSION 3.13)

project(palladio CXX)


### global definitions

set(TGT_PALLADIO "palladio")
set(TGT_FS "palladio_fs")
set(TGT_CODEC "palladio_codec")
set(TGT_TEST "palladio_test")
set(TGT_PACKAGE "palladio_package")

set(PRT_RELATIVE_EXTENSION_PATH "prtlib")
set(HOUDINI_RELATIVE_DSO_PATH "dso")
set(HOUDINI_RELATIVE_ICON_PATH "config/Icons")
set(HOUDINI_RELATIVE_PACKAGES_PATH "packages")
set(HOUDINI_RELATIVE_PALLADIO_PATH "${HOUDINI_RELATIVE_PACKAGES_PATH}/palladio")


### versioning

set(PLD_VERSION_MAJOR 2)
set(PLD_VERSION_MINOR 0)
set(PLD_VERSION_PATCH 0)
set(PLD_VERSION_PRE "-beta.2") # set to empty string for final releases
if (NOT PLD_VERSION_BUILD)
    set(PLD_VERSION_BUILD DEV)
endif ()


### utils & dependencies

include(${CMAKE_CURRENT_LIST_DIR}/cmake/common.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/dependencies.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/versioning.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/packaging.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/installation.cmake)


### setup targets

add_subdirectory(codec)
add_subdirectory(palladio)
add_subdirectory(palladio_fs)
add_dependencies(${TGT_PALLADIO} ${TGT_CODEC} ${TGT_FS})


### setup package target

pld_setup_packaging()


### setup test target

enable_testing()
if (PLD_GEN_VISUAL_STUDIO)
    add_subdirectory(test)
else()
    add_subdirectory(test EXCLUDE_FROM_ALL)
endif()

add_dependencies(${TGT_TEST} ${TGT_CODEC})