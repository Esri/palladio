cmake_minimum_required(VERSION 3.8)
cmake_policy(SET CMP0015 NEW)


### project definition

project(palladio_codec CXX)


### build target

add_library(${PROJECT_NAME} SHARED
		CodecMain.cpp
		encoder/HoudiniEncoder.cpp
		encoder/HoudiniCallbacks.h)

if(PLD_LINUX)
	target_compile_options(${PROJECT_NAME} PRIVATE
			-std=c++11 -march=nocona
			-fvisibility=hidden -fvisibility-inlines-hidden) # TODO: use cmake tools to set these

	if(${CMAKE_BUILD_TYPE} STREQUAL "Release")
		target_compile_options(${PROJECT_NAME} PRIVATE -O3 -flto)
		target_compile_definitions(${PROJECT_NAME} PRIVATE -DNDEBUG)
	elseif(${CMAKE_BUILD_TYPE} STREQUAL "RelWithDebInfo")
		target_compile_options(${PROJECT_NAME} PRIVATE -O3 -ggdb -pg)
		target_compile_definitions(${PROJECT_NAME} PRIVATE -DNDEBUG)
	elseif(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
		target_compile_options(${PROJECT_NAME} PRIVATE -O0 -ggdb -pg)
		target_compile_definitions(${PROJECT_NAME} PRIVATE -DDEBUG)
	endif()

elseif(PLD_MACOS)
	target_compile_options(${PROJECT_NAME} PRIVATE
			-std=c++14
			-fvisibility=hidden -fvisibility-inlines-hidden) # TODO: dito
elseif(PLD_WINDOWS)
endif()


### dependencies

pld_add_dependency_prt(${PROJECT_NAME})


### install target

install(TARGETS ${PROJECT_NAME}
	RUNTIME DESTINATION prtlib
	LIBRARY DESTINATION prtlib)
