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
#	if(MSVC)
#		include(CMakeDetermineVSServicePack)
#		DetermineVSServicePack(P4H_MSVC)
#	    message("detected msvc: ${P4H_MSVC}")
#    endif()
#	if(P4H_MSVC STREQUAL "vc110sp4")
		set(P4H_TC "vc110")
		add_definitions(-DP4H_TC_VC)
#	else()
#		message(FATAL_ERROR "On windows, Visual Studio 2012 Update 4 is required")
#	endif()
elseif(P4H_LINUX)
	set(P4H_TC "gcc48")
	add_definitions(-DP4H_TC_GCC)
endif()


### common dependency settings

set(P4H_DEP_CACHE ${CMAKE_CURRENT_LIST_DIR}/../../depcache) # in parallel to src and build


### PRT dependency

set(PRT_DEP_VER 1.8.3501)
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
message(STATUS "CESDK_VERSION = ${CESDK_VERSION}")


### HOUDINI dependency

if (NOT HOUDINI_VERSION)
	set(HOUDINI_VERSION "16.0.705")
endif()
string(REPLACE "." ";" VL ${HOUDINI_VERSION})
list(GET VL 0 HOUDINI_MAJOR_VERSION)
list(GET VL 1 HOUDINI_MINOR_VERSION)
list(GET VL 2 HOUDINI_BUILD_VERSION)

if (NOT HOUDINI_ROOT)
	if(P4H_WINDOWS)
		set(HOUDINI_ROOT "C:/Program Files/Side Effects Software/Houdini ${HOUDINI_VERSION}")
	else()
		set(HOUDINI_ROOT "/opt/hfs${HOUDINI_VERSION}")
	endif()
endif()
message(STATUS "HOUDINI_ROOT     = ${HOUDINI_ROOT}")

if (NOT HOUDINI_DSO_PATH)
	if(P4H_WINDOWS)
		set(HOUDINI_DSO_PATH "$ENV{HOMEDRIVE}$ENV{HOMEPATH}/Documents/houdini${HOUDINI_MAJOR_VERSION}.${HOUDINI_MINOR_VERSION}/dso")
	else()
		set(HOUDINI_DSO_PATH "$ENV{HOME}/houdini${HOUDINI_MAJOR_VERSION}.${HOUDINI_MINOR_VERSION}/dso")
	endif()
endif()
message(STATUS "HOUDINI_DSO_PATH = ${HOUDINI_DSO_PATH}")


### default installation location

if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
	set (CMAKE_INSTALL_PREFIX "${HOUDINI_DSO_PATH}" CACHE PATH "default install path" FORCE )
endif()