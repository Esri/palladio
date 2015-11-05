include(${CMAKE_CURRENT_LIST_DIR}/utils.cmake)


### platform configuration

if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
	set(P4H_WINDOWS 1)
	set(P4H_OS "win32")
	add_definitions(-DP4H_WINDOWS)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	set(P4H_LINUX 1)
	set(P4H_OS "rhel6")
	add_definitions(-DP4H_LINUX)
endif()
set(P4H_ARCH "x86_64")


### toolchain configuration

if(P4H_WINDOWS)
	if(MSVC)
		include(CMakeDetermineVSServicePack)
		DetermineVSServicePack(P4H_MSVC)
	endif()
	if(P4H_MSVC STREQUAL "vc110sp4")
		set(P4H_TC "vc110")
		add_definitions(-DP4H_TC_VC)
	else()
		message(FATAL_ERROR "On windows, Visual Studio 2012 Update 4 is required")
	endif()
elseif(P4H_LINUX)
	set(P4H_TC "gcc44")
	add_definitions(-DP4H_TC_GCC)
endif()


### common dependency settings

set(P4H_DEP_CACHE ${CMAKE_CURRENT_LIST_DIR}/../../depcache) # in parallel to src and build


### PRT dependency

set(PRT_DEP_VER 1.4.2074)
set(PRT_DEP_URL_BASE "https://github.com/Esri/esri-cityengine-sdk/releases/download")
set(PRT_DEP_BASENAME "esri_ce_sdk-${PRT_DEP_VER}-${P4H_OS}-${P4H_TC}-${P4H_ARCH}-rel-opt")
set(PRT_DEP_ARCHIVE "${P4H_DEP_CACHE}/${PRT_DEP_BASENAME}.zip")
if(NOT EXISTS ${PRT_DEP_ARCHIVE}) 
	message(STATUS "CityEngine SDK not found in depcache, downloading from esri github repo...")
	file(DOWNLOAD "${PRT_DEP_URL_BASE}/${PRT_DEP_VER}/${PRT_DEP_BASENAME}.zip" ${PRT_DEP_ARCHIVE})
endif()
if(NOT EXISTS ${P4H_DEP_CACHE}/esri_ce_sdk/cmake/prtConfig.cmake)
	execute_process(
		COMMAND ${CMAKE_COMMAND} -E tar xf ${PRT_DEP_ARCHIVE}
		WORKING_DIRECTORY ${P4H_DEP_CACHE}
		ERROR_VARIABLE UNZIP_CHECK
	)
	if(UNZIP_CHECK)
		message(FATAL_ERROR "unzip failed")
	endif()
endif()
set(prt_DIR ${P4H_DEP_CACHE}/esri_ce_sdk/cmake)
find_package(prt CONFIG)
include_directories(${PRT_INCLUDE_PATH})
set(CESDK_VERSION "cesdk_${PRT_VERSION_MAJOR}_${PRT_VERSION_MINOR}_${PRT_VERSION_MICRO}")


### HOUDINI dependency

if(NOT HOUDINI_DSO_PATH)
	if(P4H_WINDOWS)
		set(HOUDINI_DSO_PATH "$ENV{HOMEDRIVE}$ENV{HOMEPATH}/Documents/houdini14.0/dso")
	else()
		set(HOUDINI_DSO_PATH "$ENV{HOME}/houdini14.0/dso")
	endif()
endif()