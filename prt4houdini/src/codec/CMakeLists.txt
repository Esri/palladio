cmake_minimum_required(VERSION 3.2)
cmake_policy(SET CMP0015 NEW)


### project definition

set(PROJECT prt4houdini_codec)
project(${PROJECT})
include(${CMAKE_CURRENT_LIST_DIR}/../cmake/common.cmake)


### dependencies

set(BOOST_DEP_BASENAME boost-1.53.0esri1-${P4H_OS}-${P4H_TC}-${P4H_ARCH})
set(BOOST_DEP_ARCHIVE "${P4H_DEP_CACHE}/${BOOST_DEP_BASENAME}.zip")
if (P4H_LINUX)
	set(BOOST_DEP_URL "https://esri.box.com/shared/static/7ib1dnlq4knaman6u500as030cbedg7a.zip")
elseif(P4H_WINDOWS)
	set(BOOST_DEP_URL "https://esri.box.com/shared/static/nsly1fgj7rvpvv469jqwn1st4vx1muaa.zip")
endif()
if(NOT EXISTS ${BOOST_DEP_ARCHIVE})
	message(STATUS "Boost 1.53 for CE SDK not found in dependency cache, downloading from esri.box.com...")
	file(DOWNLOAD "${BOOST_DEP_URL}" ${BOOST_DEP_ARCHIVE})
endif()
if(NOT EXISTS ${P4H_DEP_CACHE}/${BOOST_DEP_BASENAME}/cmake/boost-config.cmake)
	execute_process(
		COMMAND ${CMAKE_COMMAND} -E tar xf ${BOOST_DEP_ARCHIVE}
		WORKING_DIRECTORY ${P4H_DEP_CACHE}
		ERROR_VARIABLE UNZIP_CHECK
	)
	if(UNZIP_CHECK)
		message(FATAL_ERROR "unzip failed")
	endif()
endif()
set(Boost_DIR "${P4H_DEP_CACHE}/${BOOST_DEP_BASENAME}/cmake")
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

install(TARGETS ${PROJECT} RUNTIME DESTINATION ${HOUDINI_DSO_PATH}/prtlib)
