cmake_minimum_required(VERSION 2.8.10)
cmake_policy(SET CMP0015 NEW)

set(PROJECT prt4houdini)
project(${PROJECT})
include(../../common.cmake)

if(MAYA_PLUGIN_INSTALL_PATH)
	set(CMAKE_INSTALL_PREFIX ${HOUDINI_PLUGIN_INSTALL_PATH})
else()
	set(CMAKE_INSTALL_PREFIX "${PROJECT_SOURCE_DIR}/../install")
endif()


### dependencies

find_package(prt CONFIG REQUIRED)
include_directories(${PRT_INCLUDE_PATH})

set(prt4houdini_codec_DIR "../../codec/install/cmake")
find_package(prt4houdini_codec CONFIG REQUIRED)
include_directories(${PRT4HOUDINI_CODEC_INCLUDE_PATH})
set(CESDK_VERSION "cesdk_${PRT_VERSION_MAJOR}_${PRT_VERSION_MINOR}_${PRT_VERSION_MICRO}")


### use makefiles by houdini

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/Makefile.houdini.in"
	"${CMAKE_BINARY_DIR}/Makefile.houdini"
	@ONLY
)

add_custom_target(fwd
	ALL
	COMMAND make -f "${CMAKE_BINARY_DIR}/Makefile.houdini" install
	WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
