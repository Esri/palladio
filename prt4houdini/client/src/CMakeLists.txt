cmake_minimum_required(VERSION 2.8.10)
cmake_policy(SET CMP0015 NEW)


### project definition

set(PROJECT prt4houdini_client)
project(${PROJECT})
include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/common.cmake)

set(PRT4HOUDINI_VERSION_MAJOR 0)
set(PRT4HOUDINI_VERSION_MINOR 5)
set(PRT4HOUDINI_VERSION_MICRO 0) #TODO


### dependencies

set(prt4houdini_codec_DIR "../../codec/install/cmake")
find_package(prt4houdini_codec CONFIG REQUIRED)
include_directories(${PRT4HOUDINI_CODEC_INCLUDE_PATH})
set(CESDK_VERSION "cesdk_${PRT_VERSION_MAJOR}_${PRT_VERSION_MINOR}_${PRT_VERSION_MICRO}")


### use makefiles by houdini in out-of-source build

if(WIN32)
	set(MAKEFILE_TEMPLATE Makefile.houdini.in.nmake)
else()
	set(MAKEFILE_TEMPLATE Makefile.houdini.in.gnu)
endif()

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/${MAKEFILE_TEMPLATE}"
	"${CMAKE_BINARY_DIR}/Makefile.houdini"
	@ONLY
)

file(GLOB_RECURSE SRC RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.cpp")
file(COPY ${SRC} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

add_custom_target(fwd
	ALL
	COMMAND ${CMAKE_MAKE_PROGRAM} -f Makefile.houdini
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)


### install prt libraries

install(
	FILES ${CMAKE_CURRENT_BINARY_DIR}/SOP_prt4houdini${CMAKE_SHARED_LIBRARY_SUFFIX}
	DESTINATION ${HOUDINI_DSO_PATH}
)
install(FILES ${PRT_LIBRARIES} DESTINATION ${HOUDINI_DSO_PATH})
install(FILES ${PRT_EXT_LIBRARIES} DESTINATION "${HOUDINI_DSO_PATH}/prtlib")
install(FILES ${PRT4HOUDINI_CODEC_EXT_LIBRARIES} DESTINATION "${HOUDINI_DSO_PATH}/prtlib")


# install some test data

file(GLOB TESTDATA "${CMAKE_CURRENT_LIST_DIR}/../../../testdata/*.rpk")
install(FILES ${TESTDATA} DESTINATION "${HOUDINI_DSO_PATH}/prtdata")
