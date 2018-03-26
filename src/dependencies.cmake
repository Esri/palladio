### setup conan

if(NOT EXISTS "${CMAKE_BINARY_DIR}/conan.cmake")
   message(STATUS "Downloading conan.cmake from https://github.com/conan-io/cmake-conan")
   file(DOWNLOAD "https://raw.githubusercontent.com/conan-io/cmake-conan/v0.10/conan.cmake"
                 "${CMAKE_BINARY_DIR}/conan.cmake")
endif()

include(${CMAKE_BINARY_DIR}/conan.cmake)
conan_cmake_run(CONANFILE conanfile.txt BASIC_SETUP CMAKE_TARGETS BUILD missing)


### PRT dependency

find_package(prt CONFIG REQUIRED)

function(pld_add_dependency_prt TGT)
	target_compile_definitions(${TGT} PRIVATE -DPRT_VERSION_MAJOR=${PRT_VERSION_MAJOR} -DPRT_VERSION_MINOR=${PRT_VERSION_MINOR})
	target_include_directories(${TGT} PRIVATE ${PRT_INCLUDE_PATH})
	target_link_libraries(${TGT} PRIVATE ${PRT_LIBRARY})
endfunction()


### HOUDINI dependency

list(APPEND CMAKE_PREFIX_PATH "${CONAN_HOUDINI_ROOT}/toolkit/cmake")
find_package(Houdini REQUIRED)

# TODO: move into function, use output variables
if(PLD_LINUX)
	# get Houdini fundamentals from conan as global variables
	set(HOUDINI_ROOT    ${CONAN_HOUDINI_ROOT})
	set(HOUDINI_VERSION ${CONAN_USER_HOUDINI_version}) # todo: replace by cmake
	message(STATUS "HOUDINI_VERSION = ${HOUDINI_VERSION}")

	string(REPLACE "." ";" VL ${HOUDINI_VERSION})
	list(GET VL 0 HOUDINI_MAJOR_VERSION)
	list(GET VL 1 HOUDINI_MINOR_VERSION)
	list(GET VL 2 HOUDINI_BUILD_VERSION)
endif()

function(pld_add_dependency_houdini TGT)
	if(PLD_WINDOWS)
		target_link_libraries(${TGT} PRIVATE Houdini)
		houdini_configure_target(${TGT})
	elseif(PLD_LINUX)
		# TODO: use houdini cmake config

		# setup paths
		set(HOUDINI_BINARY_DIR  ${HOUDINI_ROOT}/bin)
		set(HOUDINI_INCLUDE_DIR ${HOUDINI_ROOT}/toolkit/include)
		set(HOUDINI_LIBRARY_DIR ${HOUDINI_ROOT}/dsolib)

		message(STATUS "HOUDINI_ROOT        = ${HOUDINI_ROOT}")
		message(STATUS "HOUDINI_BINARY_DIR  = ${HOUDINI_BINARY_DIR}")
		message(STATUS "HOUDINI_FRAMEWORK   = ${HOUDINI_FRAMEWORK}")
		message(STATUS "HOUDINI_INCLUDE_DIR = ${HOUDINI_INCLUDE_DIR}")
		message(STATUS "HOUDINI_LIBRARY_DIR = ${HOUDINI_LIBRARY_DIR}")

		# configure sesi tag define
		string(TIMESTAMP DATE "%Y-%m-%d")
		string(TIMESTAMP TIME "%H:%M")
		site_name(HOSTNAME)
		if(PLD_WINDOWS)
			SET(UN $ENV{USERNAME})
		else()
			SET(UN $ENV{USER})
		endif()
		file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/taginfo "Compiled on:\n${DATE}\n${TIME}\nby: ${UN}@${HOSTNAME}")
		execute_process(
				COMMAND ${HOUDINI_BINARY_DIR}/sesitag -c -f taginfo
				WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
				OUTPUT_VARIABLE PLD_SESI_TAG
		)
		message(STATUS "Houdini: generated sesi tag: ${PLD_SESI_TAG}")

		target_compile_definitions(${TGT} PRIVATE
				${PLD_SESI_TAG} -DMAKING_DSO -DGCC3 -DGCC4 -DAMD64
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

	elseif(PLD_MACOS)
		# TODO
	endif()
endfunction()


### boost dependency

function(pld_add_dependency_boost TGT)
	target_link_libraries(${TGT} PRIVATE CONAN_PKG::boost)
endfunction()


### catch2 dependency

function(pld_add_dependency_catch2 TGT)
	target_link_libraries(${TGT} PRIVATE CONAN_PKG::catch2)
endfunction()
