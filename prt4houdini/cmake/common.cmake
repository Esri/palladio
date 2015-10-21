include(${CMAKE_CURRENT_LIST_DIR}/utils.cmake)


### platform configuration

if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
	set(P4H_WINDOWS 1)
	set(PRT_WINDOWS 1)
	set(P4H_OS "win32")
	add_definitions(-DP4H_WINDOWS)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	set(P4H_OS "linux")
	set(P4H_LINUX 1)
	set(PRT_LINUX 1)
	add_definitions(-DP4H_LINUX)
endif()

set(P4H_ARCH "x86_64")


### toolchain configuration

if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
	if(MSVC)
		include(CMakeDetermineVSServicePack)
	    DetermineVSServicePack(P4H_MSVC)
	endif()
    if(P4H_MSVC STREQUAL "vc100sp1")
		set(P4H_TC "P4H_TC_VC100")
		set(PRT_TC "PRT_TC_VC10")
		set(PRT_TC_VC10 1)
	elseif(P4H_MSVC STREQUAL "vc110sp4")
		set(P4H_TC "P4H_TC_VC110")
		set(PRT_TC "PRT_TC_VC11")
		set(PRT_TC_VC11 1)
	endif()
	set(FORCED_PRT_TC vc100) # always use official VC10 builds
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	set(P4H_TC "P4H_TC_GCC44")
	set(PRT_TC "PRT_TC_GCC44")
	set(PRT_TC_GCC 1)
	set(FORCED_PRT_TC gcc44) # always use official gcc44 builds
endif()


### common dependencies

if (WIN32)
	set(PRT_DEPENDENCY_CACHE_PATH "C:/dev/depcache")
else()
	set(PRT_DEPENDENCY_CACHE_PATH "${CMAKE_CURRENT_LIST_DIR}/../depcache")
endif()
get_dep_classifier_components(${P4H_OS} ${P4H_TC} DEP_OS DEP_TC)


### PRT dependency

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
	prt_get_dependency("releases" "cesdk" "prt" "1.3.1969" "${DEP_OS}-${FORCED_PRT_TC}-${P4H_ARCH}-rel-dbg" "prt_DIR")
else()
	prt_get_dependency("releases" "cesdk" "prt" "1.3.1969" "${DEP_OS}-${FORCED_PRT_TC}-${P4H_ARCH}-rel-opt" "prt_DIR")
endif()
find_package(prt CONFIG)
include_directories(${PRT_INCLUDE_PATH})
set(CESDK_VERSION "cesdk_${PRT_VERSION_MAJOR}_${PRT_VERSION_MINOR}_${PRT_VERSION_MICRO}")


### HOUDINI dependency

if (WIN32)
	set(houdini_DIR "C:/Program Files/Side Effects Software/Houdini 14.0.444")
	set(HOUDINI_DSO_PATH "$ENV{HOMEDRIVE}$ENV{HOMEPATH}/Documents/houdini14.0/dso")
else()
	# see build.sh set(houdini_DIR "/opt/hfs14.0.361")
	set(HOUDINI_DSO_PATH "$ENV{HOME}/houdini14.0/dso")
endif()
