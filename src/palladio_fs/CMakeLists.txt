cmake_minimum_required(VERSION 3.13)


### project definition

project(palladio_fs CXX)


### target definition

add_library(${PROJECT_NAME} SHARED
	main.cpp
	RulePackageFS.cpp
	../palladio/Utils.cpp
	../palladio/LogHandler.cpp)

target_include_directories(${PROJECT_NAME} PRIVATE
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
		${CMAKE_CURRENT_SOURCE_DIR}/..)

### compiler settings

add_toolchain_definition(${PROJECT_NAME})
pld_add_version_definitions(${PROJECT_NAME})

if(PLD_WINDOWS)
	# nothing, inheriting compiler flags from houdini

elseif(PLD_LINUX)
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
			INSTALL_RPATH "\$ORIGIN/../../packages/palladio"
			INSTALL_RPATH_USE_LINK_PATH FALSE
			SKIP_RPATH FALSE
			BUILD_WITH_INSTALL_RPATH TRUE)
endif()


### dependencies

pld_add_dependency_prt(${PROJECT_NAME})
pld_add_dependency_houdini(${PROJECT_NAME})


### setup install target

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${HOUDINI_RELATIVE_DSO_PATH}/fs LIBRARY DESTINATION ${HOUDINI_RELATIVE_DSO_PATH}/fs)
