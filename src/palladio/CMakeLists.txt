cmake_minimum_required(VERSION 3.8)


### project definition

project(palladio CXX)


### target definition

add_library(${PROJECT_NAME} SHARED
		HoudiniNodesMain.cpp
		ModelConverter.cpp
		Utils.cpp
		ShapeConverter.cpp
		ShapeData.cpp
		ShapeGenerator.cpp
		NodeParameter.cpp
		PRTContext.cpp
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
		${palladio_codec_SOURCE_DIR})


### compiler settings

if(PLD_WINDOWS)
	# TODO: outdated, move houdini-specific stuff to pld_add_dependency_houdini
	set(CMAKE_CXX_FLAGS "-EHsc -nologo -TP -Zc:forScope -wd4355 -w14996")
	set(CMAKE_CXX_FLAGS_Release "-O2 -DNDEBUG -MD -GR -bigobj")
	target_comipile_definitions(${PROJECT_NAME} PRIVATE ${PLD_SESI_TAG} -DMAKING_DSO -DI386 -DWIN32 -DSWAP_BITFIELDS -D_WIN32_WINNT=0x0502 -DNOMINMAX -DSTRICT -DWIN32_LEAN_AND_MEAN -D_USE_MATH_DEFINES -D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEPRECATE -D_SCL_SECURE_NO_WARNINGS -DBOOST_ALL_NO_LIB -DSESI_LITTLE_ENDIAN -DAMD64 -DSIZEOF_VOID_P=8 -DFBX_ENABLED=1 -DOPENCL_ENABLED=1 -DOPENVDB_ENABLED=1)

	file(GLOB_RECURSE HOUDINI_LIBS
			"${HOUDINI_ROOT}/custom/houdini/dsolib/*.a"
			"${HOUDINI_ROOT}/custom/houdini/dsolib/*.lib")

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

pld_add_dependency_prt(${PROJECT_NAME})
pld_add_dependency_houdini(${PROJECT_NAME}) # also provides boost


### setup install target

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION . LIBRARY DESTINATION .)
install(FILES ${PRT_LIBRARIES} DESTINATION .)
install(FILES ${PRT_EXT_LIBRARIES} DESTINATION prtlib)
