cmake_minimum_required(VERSION 3.13)


### project definition

project(palladio CXX)


### target definition

add_library(${PROJECT_NAME} SHARED
        PalladioMain.cpp
		ModelConverter.cpp
		Utils.cpp
		ShapeConverter.cpp
		ShapeData.cpp
		ShapeGenerator.cpp
		NodeParameter.cpp
		PRTContext.cpp
		ResolveMapCache.cpp
		SOPAssign.cpp
		SOPGenerate.cpp
		PrimitivePartition.cpp
		AttrEvalCallbacks.cpp
		AttributeConversion.cpp
		MultiWatch.cpp
		PrimitiveClassifier.cpp
		LogHandler.h
		LRUCache.h)

target_include_directories(${PROJECT_NAME} PRIVATE
		${palladio_codec_SOURCE_DIR}
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>)


### compiler settings

add_toolchain_definition(${PROJECT_NAME})

if(PLD_TEST)
	message(STATUS "Enabling test exports...")
	target_compile_definitions(palladio PRIVATE -DPLD_TEST_EXPORTS)
endif()

if(PLD_WINDOWS)
	# nothing, inheriting compiler flags from houdini

elseif(PLD_LINUX)
	target_compile_options(${PROJECT_NAME} PRIVATE
			-mtune=generic -m64 -fPIC -mmmx -msse -msse2 -msse3
			-std=c++11
			-Wall -W -Wno-parentheses -Wno-sign-compare -Wno-reorder
			-Wno-uninitialized -Wunused -Wno-unused-parameter
			-fno-strict-aliasing -Wno-unused-local-typedefs)

	if (${CMAKE_BUILD_TYPE} STREQUAL "Release")
		target_compile_options(${PROJECT_NAME} PRIVATE -O3 -flto)
		target_compile_definitions(${PROJECT_NAME} PRIVATE -DNDEBUG)
	elseif(${CMAKE_BUILD_TYPE} STREQUAL "RelWithDebInfo")
		target_compile_options(${PROJECT_NAME} PRIVATE -O3 -ggdb -pg)
		target_compile_definitions(${PROJECT_NAME} PRIVATE -DNDEBUG)
	elseif(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
		target_compile_options(${PROJECT_NAME} PRIVATE -O0 -ggdb -pg)
		target_compile_definitions(${PROJECT_NAME} PRIVATE -DDEBUG)
	endif()

	set_target_properties(${PROJECT_NAME} PROPERTIES
			INSTALL_RPATH "\$ORIGIN")

elseif(PLD_MACOS)
	target_compile_options(${PROJECT_NAME} PRIVATE
			-std=c++11)

	if(${CMAKE_BUILD_TYPE} STREQUAL "RelWithDebInfo")
		target_compile_options(${PROJECT_NAME} PRIVATE -O3 -g)
		target_compile_definitions(${PROJECT_NAME} PRIVATE -DNDEBUG)
	elseif(${CMAKE_BUILD_TYPE} STREQUAL "Release")
		target_compile_options(${PROJECT_NAME} PRIVATE -O3)
	endif()
endif()


### dependencies

pld_add_dependency_boost(${PROJECT_NAME})
pld_add_dependency_prt(${PROJECT_NAME})
pld_add_dependency_houdini(${PROJECT_NAME})


### setup install target

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION . LIBRARY DESTINATION .)
install(FILES ${PRT_LIBRARIES} DESTINATION .)
install(FILES ${PRT_EXT_LIBRARIES} DESTINATION prtlib)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/../../doc DESTINATION .)
install(FILES ${PROJECT_SOURCE_DIR}/../../README.md ${PROJECT_SOURCE_DIR}/../../LICENSE DESTINATION .)
