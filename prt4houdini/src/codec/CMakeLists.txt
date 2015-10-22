cmake_minimum_required(VERSION 3.2)
cmake_policy(SET CMP0015 NEW)


### project definition

set(PROJECT prt4houdini_codec)
project(${PROJECT})
include(${CMAKE_CURRENT_LIST_DIR}/../cmake/common.cmake)


### dependencies

# TODO: this stuff only works in esri intranet
set(PRT_DEPENDENCY_CACHE_PATH ${P4H_DEP_CACHE})
prt_get_dependency("thirdparty"	"org.boost" "boost" "1.53.0esri1" "${P4H_OS}-${P4H_TC}-${P4H_ARCH}" "Boost_DIR")

find_package(Boost NO_MODULE)
include_directories(${Boost_INCLUDE_DIR})
link_directories(${Boost_LIBRARY_DIR})


### build target

set(CMAKE_INCLUDE_CURRENT_DIR ON)

list(APPEND SOURCES houdini.cpp encoder/HoudiniEncoder.cpp)
add_library(${PROJECT} SHARED ${SOURCES})

if(PRT_WINDOWS)
	target_link_libraries(${PROJECT} IPHlpApi Psapi DbgHelp)
elseif(PRT_LINUX)
	target_link_libraries(${PROJECT} z pthread dl rt)
endif()

target_link_libraries(${PROJECT} ${PRT_LINK_LIBRARIES})
set_target_properties(${PROJECT} PROPERTIES LINKER_LANGUAGE CXX)


### install target

install(TARGETS ${PROJECT} DESTINATION ${HOUDINI_DSO_PATH}/prtlib)
