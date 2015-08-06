include(${CMAKE_CURRENT_LIST_DIR}/utils.cmake)


### configuration

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

set(PRT_DEPENDENCY_CACHE_PATH "C:/dev/depcache")
get_dep_classifier_components(${P4H_OS} ${P4H_TC} DEP_OS DEP_TC)

prt_get_dependency("thirdparty"	"com.esri" "boost" "1.53.0" "${DEP_OS}-${DEP_TC}-${P4H_ARCH}" "Boost_DIR")
set(BOOST_ROOT=${Boost_DIR})
set(Boost_ADDITIONAL_VERSIONS=1.53.0)
set(Boost_NO_SYSTEM_PATHS ON)
set(Boost_USE_STATIC_LIBS ON)
find_package(Boost)
include_directories(${Boost_INCLUDE_DIR})
link_directories(${Boost_LIBRARY_DIR})


### PRT dependency

 # fetch prt release if prt_DIR not specified
if(NOT prt_DIR)
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    	prt_get_dependency("releases" "cesdk" "prt" "1.3.1969" "${DEP_OS}-${FORCED_PRT_TC}-${P4H_ARCH}-rel-dbg" "prt_DIR")
    else()
    	prt_get_dependency("releases" "cesdk" "prt" "1.3.1969" "${DEP_OS}-${FORCED_PRT_TC}-${P4H_ARCH}-rel-opt" "prt_DIR")
    endif()
endif()
find_package(prt CONFIG)
include_directories(${PRT_INCLUDE_PATH})
set(CESDK_VERSION "cesdk_${PRT_VERSION_MAJOR}_${PRT_VERSION_MINOR}_${PRT_VERSION_MICRO}")


### HOUDINI dependency

# houdini location
set(houdini_DIR "C:/Program Files/Side Effects Software/Houdini 14.0.291")

# houdini user dso location
if (WIN32)
	set(HOUDINI_DSO_PATH "$ENV{HOMEDRIVE}$ENV{HOMEPATH}/Documents/houdini14.0/dso")
else()
	set(HOUDINI_DSO_PATH "$ENV{HOME}/houdini14.0/dso")
endif()
