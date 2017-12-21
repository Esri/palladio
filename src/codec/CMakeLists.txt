cmake_minimum_required(VERSION 3.8)
cmake_policy(SET CMP0015 NEW)


### project definition

project(prt4houdini_codec CXX)


### build target

add_library(${PROJECT_NAME} SHARED
		houdini.cpp
		encoder/HoudiniEncoder.cpp
		encoder/HoudiniCallbacks.h)

target_compile_definitions(${PROJECT_NAME} PRIVATE -DPRT_VERSION_MAJOR=${PRT_VERSION_MAJOR} -DPRT_VERSION_MINOR=${PRT_VERSION_MINOR})

if(P4H_LINUX)
	target_compile_options(${PROJECT_NAME} PRIVATE -std=c++11 -march=nocona -fvisibility=hidden -fvisibility-inlines-hidden)
elseif(P4H_MACOS)
	target_compile_options(${PROJECT_NAME} PRIVATE -std=c++14 -fvisibility=hidden -fvisibility-inlines-hidden)
endif()

if(${CMAKE_BUILD_TYPE} STREQUAL "RelWithDebInfo")
	target_compile_options(${PROJECT_NAME} PRIVATE -O3 -ggdb)
	target_compile_definitions(${PROJECT_NAME} PRIVATE -DNDEBUG)
elseif(${CMAKE_BUILD_TYPE} STREQUAL "Release")
	set(CMAKE_CXX_FLAGS_Release "${CMAKE_CXX_FLAGS_Release} -O3")
endif()

target_include_directories(${PROJECT_NAME} PRIVATE ${PRT_INCLUDE_PATH})
target_link_libraries(${PROJECT_NAME} PRIVATE ${PRT_LINK_LIBRARIES})


### install target

install(TARGETS ${PROJECT_NAME}
	RUNTIME DESTINATION prtlib
	LIBRARY DESTINATION prtlib)
