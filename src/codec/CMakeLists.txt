cmake_minimum_required(VERSION 3.13)
cmake_policy(SET CMP0015 NEW)


### project definition

project(palladio_codec CXX)


### build target

add_library(${PROJECT_NAME} SHARED
		CodecMain.cpp
		encoder/HoudiniEncoder.cpp
		encoder/HoudiniCallbacks.h)

add_toolchain_definition(${PROJECT_NAME})

if(PLD_LINUX)
	if(${PRT_VERSION_MAJOR} STREQUAL "2")
		target_compile_options(${PROJECT_NAME} PRIVATE -std=c++14)
	elseif(${PRT_VERSION_MAJOR} STREQUAL "1")
		target_compile_options(${PROJECT_NAME} PRIVATE -std=c++11)
	endif()

	target_compile_options(${PROJECT_NAME} PRIVATE
			-march=nocona -D_GLIBCXX_USE_CXX11_ABI=0
			-fvisibility=hidden -fvisibility-inlines-hidden)

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
	# TODO
	target_compile_options(${PROJECT_NAME} PRIVATE
			-std=c++14
			-fvisibility=hidden -fvisibility-inlines-hidden)

elseif(PLD_WINDOWS)
	target_compile_options(${PROJECT_NAME} PRIVATE
			/bigobj /GR /EHsc /MD)
endif()


### dependencies

pld_add_dependency_prt(${PROJECT_NAME})


### install target

install(TARGETS ${PROJECT_NAME}
	RUNTIME DESTINATION "${HOUDINI_RELATIVE_PALLADIO_PATH}/${PRT_RELATIVE_EXTENSION_PATH}"
	LIBRARY DESTINATION "${HOUDINI_RELATIVE_PALLADIO_PATH}/${PRT_RELATIVE_EXTENSION_PATH}")
