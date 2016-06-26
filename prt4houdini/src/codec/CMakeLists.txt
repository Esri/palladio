cmake_minimum_required(VERSION 3.2)
cmake_policy(SET CMP0015 NEW)


### project definition

set(PROJECT prt4houdini_codec)
project(${PROJECT})


### build target

add_definitions(-DPRT_VERSION_MAJOR=${PRT_VERSION_MAJOR} -DPRT_VERSION_MINOR=${PRT_VERSION_MINOR})

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -march=nocona -fvisibility=hidden -fvisibility-inlines-hidden -Wl,--exclude-libs,ALL")
set(CMAKE_CXX_FLAGS_Release "${CMAKE_CXX_FLAGS_Release} -O3")

list(APPEND SOURCES houdini.cpp encoder/HoudiniEncoder.cpp)
add_library(${PROJECT} SHARED ${SOURCES})

#if(PRT_WINDOWS)
#	target_link_libraries(${PROJECT} IPHlpApi Psapi DbgHelp)
#elseif(PRT_LINUX)
#	target_link_libraries(${PROJECT} z pthread dl rt)
#endif()

target_link_libraries(${PROJECT} ${PRT_LINK_LIBRARIES})
set_target_properties(${PROJECT} PROPERTIES LINKER_LANGUAGE CXX)


### install target

install(TARGETS ${PROJECT}
	RUNTIME DESTINATION ${HOUDINI_DSO_PATH}/prtlib
	LIBRARY DESTINATION ${HOUDINI_DSO_PATH}/prtlib
)
