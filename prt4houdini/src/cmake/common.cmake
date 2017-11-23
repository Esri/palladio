include(${CMAKE_CURRENT_LIST_DIR}/utils.cmake)


### platform configuration

message(STATUS "CMAKE_SYSTEM_NAME = ${CMAKE_SYSTEM_NAME}")
if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
	set(P4H_WINDOWS 1)
	set(P4H_OS "win32")
	add_definitions(-DP4H_WINDOWS)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	set(P4H_LINUX 1)
	set(P4H_OS "rhel6")
	add_definitions(-DP4H_LINUX)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
	set(P4H_MACOS 1)
	set(P4H_OS "macos")
endif()
set(P4H_ARCH "x86_64")


### toolchain configuration

if(P4H_WINDOWS)
	set(P4H_TC "vc110")
	add_definitions(-DP4H_TC_VC) # TODO
elseif(P4H_LINUX)
	set(P4H_TC "gcc48")
	add_definitions(-DP4H_TC_GCC) # TODO
elseif(P4H_MACOS)
	set(P4H_TC "ac73")
	add_definitions(-DP4H_TC_CLANG) # TODO
endif()


### PRT dependency

find_package(prt CONFIG)
set(CESDK_VERSION "cesdk_${PRT_VERSION_MAJOR}_${PRT_VERSION_MINOR}_${PRT_VERSION_MICRO}")
message(STATUS "CESDK_VERSION = ${CESDK_VERSION}")
message(STATUS "PRT_INSTALL_PATH = ${PRT_INSTALL_PATH}")


### HOUDINI dependency

# get Houdini fundamentals from conan as global variables
set(HOUDINI_ROOT    ${CONAN_HOUDINI-APPRENTICE_ROOT})
set(HOUDINI_VERSION ${CONAN_USER_HOUDINI-APPRENTICE_version})
message(STATUS "HOUDINI_VERSION = ${HOUDINI_VERSION}")

string(REPLACE "." ";" VL ${HOUDINI_VERSION})
list(GET VL 0 HOUDINI_MAJOR_VERSION)
list(GET VL 1 HOUDINI_MINOR_VERSION)
list(GET VL 2 HOUDINI_BUILD_VERSION)

function(p4h_add_dependency_houdini TGT)
	# setup paths
	if(P4H_WINDOWS)
		# TODO
	elseif(P4H_LINUX)
		set(HOUDINI_BINARY_DIR  ${HOUDINI_ROOT}/bin)
		set(HOUDINI_INCLUDE_DIR ${HOUDINI_ROOT}/toolkit/include)
		set(HOUDINI_LIBRARY_DIR ${HOUDINI_ROOT}/dsolib)
	elseif(P4H_MACOS)
		set(HOUDINI_ROOT /Applications/Houdini/Houdini${HOUDINI_VERSION})
		set(HOUDINI_BINARY_DIR ${HOUDINI_FRAMEWORK}/Resources/bin)
		set(HOUDINI_FRAMEWORK ${HOUDINI_ROOT}/Frameworks/Houdini.framework/Versions/${HOUDINI_VERSION})
		set(HOUDINI_INCLUDE_DIR ${HOUDINI_FRAMEWORK}/Resources/toolkit/include)
		set(HOUDINI_LIBRARY_DIR ${HOUDINI_FRAMEWORK}/Libraries)
		#set(HOUDINI_TOOLKIT_DIR ${HOUDINI_ROOT}/Frameworks/Houdini.framework/Versions/${HOUDINI_VERSION}/Resources/toolkit)
	endif()

	message(STATUS "HOUDINI_ROOT        = ${HOUDINI_ROOT}")
	message(STATUS "HOUDINI_FRAMEWORK   = ${HOUDINI_FRAMEWORK}")
	message(STATUS "HOUDINI_INCLUDE_DIR = ${HOUDINI_INCLUDE_DIR}")
	message(STATUS "HOUDINI_LIBRARY_DIR = ${HOUDINI_LIBRARY_DIR}")

	# configure sesi tag define
	string(TIMESTAMP DATE "%Y-%m-%d")
	string(TIMESTAMP TIME "%H:%M")
	site_name(HOSTNAME)
	if(P4H_WINDOWS)
		SET(UN $ENV{USERNAME})
	else()
		SET(UN $ENV{USER})
	endif()
	file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/taginfo "Compiled on:\n${DATE}\n${TIME}\nby: ${UN}@${HOSTNAME}")
	execute_process(
			COMMAND ${HOUDINI_BINARY_DIR}/sesitag -c -f taginfo
			WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
			OUTPUT_VARIABLE P4H_SESI_TAG
	)
	message(STATUS "Houdini: generated sesi tag: ${P4H_SESI_TAG}")

	if(P4H_WINDOWS)
		# TODO

	elseif(P4H_LINUX)
		#target_compile_options(${TGT} PRIVATE
		#		-Wl,-rpath=\$ORIGIN:${HOUDINI_ROOT}/dsolib)

		# below has been extracted from "HFS=/opt/hfs16.0 bin/hcustom -c"
		# TODO: ideally call hcustom at configuration time
		# OR maybe sidefx will provide cmake scripts eventually...
		target_compile_definitions(${TGT} PRIVATE
				${P4H_SESI_TAG} -DMAKING_DSO -DGCC3 -DGCC4 -DAMD64
				-DSIZEOF_VOID_P=${CMAKE_SIZEOF_VOID_P} -DVERSION=\"${HOUDINI_VERSION}\" -DDLLEXPORT=""
				-D_GNU_SOURCE -DLINUX -DSESI_LITTLE_ENDIAN -DENABLE_THREADS -DUSE_PTHREADS -DENABLE_UI_THREADS
				-DFBX_ENABLED=1 -DOPENCL_ENABLED=1 -DOPENVDB_ENABLED=1 -D_FILE_OFFSET_BITS=64)

		target_include_directories(${TGT} PRIVATE
				${HOUDINI_ROOT}/toolkit/include)

		# find main Houdini libraries
		find_library(HOUDINI_UI_LIBRARY  HoudiniUI  PATHS ${HOUDINI_LIBRARY_DIR} NO_DEFAULT_PATH NO_CMAKE_SYSTEM_PATH)
		find_library(HOUDINI_OPZ_LIBRARY HoudiniOPZ PATHS ${HOUDINI_LIBRARY_DIR} NO_DEFAULT_PATH NO_CMAKE_SYSTEM_PATH)
		find_library(HOUDINI_OP3_LIBRARY HoudiniOP3 PATHS ${HOUDINI_LIBRARY_DIR} NO_DEFAULT_PATH NO_CMAKE_SYSTEM_PATH)
		find_library(HOUDINI_OP2_LIBRARY HoudiniOP2 PATHS ${HOUDINI_LIBRARY_DIR} NO_DEFAULT_PATH NO_CMAKE_SYSTEM_PATH)
		find_library(HOUDINI_OP1_LIBRARY HoudiniOP1 PATHS ${HOUDINI_LIBRARY_DIR} NO_DEFAULT_PATH NO_CMAKE_SYSTEM_PATH)
		find_library(HOUDINI_GEO_LIBRARY HoudiniGEO PATHS ${HOUDINI_LIBRARY_DIR} NO_DEFAULT_PATH NO_CMAKE_SYSTEM_PATH)
		find_library(HOUDINI_PRM_LIBRARY HoudiniPRM PATHS ${HOUDINI_LIBRARY_DIR} NO_DEFAULT_PATH NO_CMAKE_SYSTEM_PATH)
		find_library(HOUDINI_UT_LIBRARY  HoudiniUT  PATHS ${HOUDINI_LIBRARY_DIR} NO_DEFAULT_PATH NO_CMAKE_SYSTEM_PATH)

		# using boost included in Houdini
		find_library(BOOST_SYSTEM     boost_system     PATHS ${HOUDINI_LIBRARY_DIR} NO_DEFAULT_PATH NO_CMAKE_SYSTEM_PATH)
		find_library(BOOST_FILESYSTEM boost_filesystem PATHS ${HOUDINI_LIBRARY_DIR} NO_DEFAULT_PATH NO_CMAKE_SYSTEM_PATH)

		target_link_libraries(${TGT} PRIVATE
				${HOUDINI_UI_LIBRARY}
				${HOUDINI_OPZ_LIBRARY}
				${HOUDINI_OP3_LIBRARY}
				${HOUDINI_OP2_LIBRARY}
				${HOUDINI_OP1_LIBRARY}
				${HOUDINI_GEO_LIBRARY}
				${HOUDINI_PRM_LIBRARY}
				${HOUDINI_UT_LIBRARY}
				${BOOST_FILESYSTEM}
				${BOOST_SYSTEM})

	elseif(P4H_MACOS)
		# TODO
		target_compile_definitions(${TGT} PRIVATE
				${P4H_SESI_TAG} -DMBSD_INTEL -DGCC3 -DGCC4 -DMBSD -DMAKING_DSO
				-DSIZEOF_VOID_P=${CMAKE_SIZEOF_VOID_P} -DVERSION=\"${HOUDINI_VERSION}\" -DDLLEXPORT=""
				-DSESI_LITTLE_ENDIAN -DENABLE_THREADS -DUSE_PTHREADS -DENABLE_UI_THREADS
				-DFBX_ENABLED=1 -DOPENCL_ENABLED=1 -DOPENVDB_ENABLED=1 -D_FILE_OFFSET_BITS=64)

		target_include_directories(${TGT} PRIVATE
				${HOUDINI_INCLUDE_DIR})

		find_library(HOUDINI_UI_LIBRARY  NAMES HoudiniUI  PATHS ${HOUDINI_LIBRARY_DIR})
		find_library(HOUDINI_OPZ_LIBRARY HoudiniOPZ PATHS ${HOUDINI_LIBRARY_DIR})
		find_library(HOUDINI_OP3_LIBRARY HoudiniOP3 PATHS ${HOUDINI_LIBRARY_DIR})
		find_library(HOUDINI_OP2_LIBRARY HoudiniOP2 PATHS ${HOUDINI_LIBRARY_DIR})
		find_library(HOUDINI_OP1_LIBRARY HoudiniOP1 PATHS ${HOUDINI_LIBRARY_DIR})
		find_library(HOUDINI_GEO_LIBRARY HoudiniGEO PATHS ${HOUDINI_LIBRARY_DIR})
		find_library(HOUDINI_PRM_LIBRARY HoudiniPRM PATHS ${HOUDINI_LIBRARY_DIR})
		find_library(HOUDINI_UT_LIBRARY  NAMES HoudiniUT  PATHS ${HOUDINI_LIBRARY_DIR})

		target_link_libraries(${TGT} PRIVATE
				${HOUDINI_UI_LIBRARY}
				${HOUDINI_OPZ_LIBRARY}
				${HOUDINI_OP3_LIBRARY}
				${HOUDINI_OP2_LIBRARY}
				${HOUDINI_OP1_LIBRARY}
				${HOUDINI_GEO_LIBRARY}
				${HOUDINI_PRM_LIBRARY}
				${HOUDINI_UT_LIBRARY}
				${BOOST_FILESYSTEM}
				${BOOST_SYSTEM})

		add_custom_command(TARGET ${TGT} POST_BUILD
				COMMAND ${CMAKE_INSTALL_NAME_TOOL}
				-change @rpath/libcom.esri.prt.core.dylib @loader_path/libcom.esri.prt.core.dylib
				$<TARGET_FILE:${PROJECT_NAME}>)
	endif()

endfunction()


### setup local deployment target directory

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
	message(STATUS "set default install prefix: ${CMAKE_INSTALL_PREFIX}")
endif()