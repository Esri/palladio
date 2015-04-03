cmake_minimum_required(VERSION 2.8.10)
cmake_policy(SET CMP0015 NEW)

set(PROJECT prt4houdini)
project(${PROJECT})
include(../../common.cmake)


### dependencies

find_package(prt CONFIG REQUIRED)
include_directories(${PRT_INCLUDE_PATH})

set(prt4houdini_codec_DIR "../../codec/install/cmake")
find_package(prt4houdini_codec CONFIG REQUIRED)
include_directories(${PRT4HOUDINI_CODEC_INCLUDE_PATH})
set(CESDK_VERSION "cesdk_${PRT_VERSION_MAJOR}_${PRT_VERSION_MINOR}_${PRT_VERSION_MICRO}")


### install prt libraries

install(FILES ${PRT_LIBRARIES} DESTINATION ${HOUDINI_DSO_PATH})
install(FILES ${PRT_EXT_LIBRARIES} DESTINATION "${HOUDINI_DSO_PATH}/prtlib")
install(FILES ${PRT4HOUDINI_CODEC_EXT_LIBRARIES} DESTINATION "${HOUDINI_DSO_PATH}/prtlib")


# install some test data

file(GLOB TESTDATA "${CMAKE_CURRENT_LIST_DIR}/../../../testdata/*.rpk")
install(FILES ${TESTDATA} DESTINATION "${HOUDINI_DSO_PATH}/prtdata")


### use makefiles by houdini in out-of-source build

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/Makefile.houdini.in"
	"${CMAKE_BINARY_DIR}/Makefile.houdini"
	@ONLY
)

file(GLOB_RECURSE SRC RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.cpp")
file(COPY ${SRC} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

add_custom_target(fwd
	ALL
	COMMAND make -f "${CMAKE_BINARY_DIR}/Makefile.houdini" clean install
	WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
