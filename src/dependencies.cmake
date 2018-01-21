### trigger conan

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)
# TODO: use full conan integration, i.e. avoid the manual "conan install" step


### PRT dependency

function(pld_add_dependency_prt TGT)
	find_package(prt CONFIG)

	#set(CESDK_VERSION "cesdk_${PRT_VERSION_MAJOR}_${PRT_VERSION_MINOR}_${PRT_VERSION_MICRO}")
	#message(STATUS "CESDK_VERSION    = ${CESDK_VERSION}")
	#message(STATUS "PRT_INSTALL_PATH = ${PRT_INSTALL_PATH}")

	target_compile_definitions(${TGT} PRIVATE -DPRT_VERSION_MAJOR=${PRT_VERSION_MAJOR} -DPRT_VERSION_MINOR=${PRT_VERSION_MINOR})
	target_include_directories(${TGT} PRIVATE ${PRT_INCLUDE_PATH})
	target_link_libraries(${TGT} PRIVATE ${PRT_LINK_LIBRARIES})

	# promote the PRT library paths so we can use them for the install command
	set( PRT_LIBRARIES ${PRT_LIBRARIES} PARENT_SCOPE )
	set( PRT_EXT_LIBRARIES ${PRT_EXT_LIBRARIES} PARENT_SCOPE )
endfunction()


### HOUDINI dependency

# get Houdini fundamentals from conan as global variables
set(HOUDINI_ROOT    ${CONAN_HOUDINI_ROOT})
set(HOUDINI_VERSION ${CONAN_USER_HOUDINI_version})
message(STATUS "HOUDINI_VERSION = ${HOUDINI_VERSION}")

string(REPLACE "." ";" VL ${HOUDINI_VERSION})
list(GET VL 0 HOUDINI_MAJOR_VERSION)
list(GET VL 1 HOUDINI_MINOR_VERSION)
list(GET VL 2 HOUDINI_BUILD_VERSION)

function(pld_add_dependency_houdini TGT)
	# setup paths
	if(PLD_WINDOWS)
		# TODO
	elseif(PLD_LINUX)
		set(HOUDINI_BINARY_DIR  ${HOUDINI_ROOT}/bin)
		set(HOUDINI_INCLUDE_DIR ${HOUDINI_ROOT}/toolkit/include)
		set(HOUDINI_LIBRARY_DIR ${HOUDINI_ROOT}/dsolib)
	elseif(PLD_MACOS)
		set(HOUDINI_ROOT /Applications/Houdini/Houdini${HOUDINI_VERSION})
		set(HOUDINI_BINARY_DIR ${HOUDINI_FRAMEWORK}/Resources/bin)
		set(HOUDINI_FRAMEWORK ${HOUDINI_ROOT}/Frameworks/Houdini.framework/Versions/${HOUDINI_VERSION})
		set(HOUDINI_INCLUDE_DIR ${HOUDINI_FRAMEWORK}/Resources/toolkit/include)
		set(HOUDINI_LIBRARY_DIR ${HOUDINI_FRAMEWORK}/Libraries)
		#set(HOUDINI_TOOLKIT_DIR ${HOUDINI_ROOT}/Frameworks/Houdini.framework/Versions/${HOUDINI_VERSION}/Resources/toolkit)
	endif()

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

	if(PLD_WINDOWS)
		# TODO

	elseif(PLD_LINUX)
		#target_compile_options(${TGT} PRIVATE
		#		-Wl,-rpath=\$ORIGIN:${HOUDINI_ROOT}/dsolib)

		# below has been extracted from "HFS=/opt/hfs16.0 bin/hcustom -c"
		# TODO: ideally call hcustom at configuration time
		# OR maybe sidefx will provide cmake scripts eventually...
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
		target_compile_definitions(${TGT} PRIVATE
				${PLD_SESI_TAG} -DMBSD_INTEL -DGCC3 -DGCC4 -DMBSD -DMAKING_DSO
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


