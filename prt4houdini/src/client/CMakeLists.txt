cmake_minimum_required(VERSION 3.2)
cmake_policy(SET CMP0015 NEW)


### project definition

set(PROJECT prt4houdini)
project(${PROJECT})
include(${CMAKE_CURRENT_LIST_DIR}/../cmake/common.cmake)

set(P4H_VERSION_MAJOR 0)
set(P4H_VERSION_MINOR 5)
set(P4H_VERSION_MICRO 0) #TODO


### external dependencies

set(BOOST_DEP_BASENAME boost-1.55.0-${P4H_OS}-${P4H_TC}-${P4H_ARCH})
set(BOOST_DEP_ARCHIVE "${P4H_DEP_CACHE}/${BOOST_DEP_BASENAME}.zip")
if (P4H_LINUX)
	message(STATUS "Using system boost...")
elseif(P4H_WINDOWS)
	set(BOOST_DEP_URL "https://esri.box.com/shared/static/c0uauskq22lmm9yp438sloj004rr8apv.zip")
	set(BOOST_ROOT "${P4H_DEP_CACHE}/${BOOST_DEP_BASENAME}")
	set(BOOST_INCLUDEDIR "${P4H_DEP_CACHE}/${BOOST_DEP_BASENAME}/boost")
	set(BOOST_LIBRARYDIR "${P4H_DEP_CACHE}/${BOOST_DEP_BASENAME}/lib")
	set(Boost_NO_SYSTEM_PATHS 1)
endif()
if(NOT EXISTS ${BOOST_DEP_ARCHIVE})
	message(STATUS "Boost 1.55 for Houdini not found in dependency cache, downloading from esri.box.com...")
	file(DOWNLOAD "${BOOST_DEP_URL}" ${BOOST_DEP_ARCHIVE})
endif()
if(NOT EXISTS ${P4H_DEP_CACHE}/${BOOST_DEP_BASENAME}/boost/config.hpp)
	execute_process(
		COMMAND ${CMAKE_COMMAND} -E tar xf ${BOOST_DEP_ARCHIVE}
		WORKING_DIRECTORY ${P4H_DEP_CACHE}
		ERROR_VARIABLE UNZIP_CHECK
	)
	if(UNZIP_CHECK)
		message(FATAL_ERROR "unzip failed")
	endif()
endif()

set(Boost_USE_STATIC_LIBS 1)
set(Boost_USE_STATIC_RUNTIME 0)
find_package(Boost 1.55 REQUIRED COMPONENTS filesystem)
include_directories(${Boost_INCLUDE_DIR})
link_directories(${Boost_LIBRARY_DIR})


### houdini specific configuration

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
	COMMAND ${HOUDINI_ROOT}/bin/sesitag -c -f taginfo
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	OUTPUT_VARIABLE P4H_SESI_TAG
)
message(STATUS "generated sesi tag: ${P4H_SESI_TAG}")


### setup build target

if(P4H_WINDOWS)
	set(CMAKE_CXX_FLAGS "-EHsc -nologo -TP -Zc:forScope -wd4355 -w14996")
	set(CMAKE_CXX_FLAGS_Release "-O2 -DNDEBUG -MD -GR -bigobj")
	add_definitions(${P4H_SESI_TAG} -DMAKING_DSO -DI386 -DWIN32 -DSWAP_BITFIELDS -D_WIN32_WINNT=0x0502 -DNOMINMAX -DSTRICT -DWIN32_LEAN_AND_MEAN -D_USE_MATH_DEFINES -D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEPRECATE -D_SCL_SECURE_NO_WARNINGS -DBOOST_ALL_NO_LIB -DSESI_LITTLE_ENDIAN -DAMD64 -DSIZEOF_VOID_P=8 -DFBX_ENABLED=1 -DOPENCL_ENABLED=1 -DOPENVDB_ENABLED=1)
elseif(P4H_LINUX)
	set(CMAKE_CXX_FLAGS "-std=c++11 -Wl,-rpath,${HOUDINI_ROOT}/dsolib -m64 -fPIC -Wall -W -Wno-parentheses -Wno-sign-compare -Wno-reorder -Wno-uninitialized -Wunused -Wno-unused-parameter -Wno-deprecated -fno-strict-aliasing")
	set(CMAKE_CXX_FLAGS_Release "-O2")
	add_definitions(${P4H_SESI_TAG} -DMAKING_DSO -DGCC3 -DGCC4 -DAMD64 -DSIZEOF_VOID_P=${CMAKE_SIZEOF_VOID_P} -DVERSION=\"14.0.444\" -DDLLEXPORT="" -D_GNU_SOURCE -DLINUX -DSESI_LITTLE_ENDIAN -DENABLE_THREADS -DUSE_PTHREADS -DENABLE_UI_THREADS -DFBX_ENABLED=1 -DOPENCL_ENABLED=1 -DOPENVDB_ENABLED=1)
endif()

set(CMAKE_INCLUDE_CURRENT_DIR ON) # in case we generate source files
include_directories(${PRT_INCLUDE_PATH})
include_directories(${PRT4HOUDINI_CODEC_INCLUDE_PATH})
include_directories(${HOUDINI_ROOT}/toolkit/include)
include_directories(${CMAKE_SOURCE_DIR})

if(P4H_WINDOWS)
	file(GLOB_RECURSE HOUDINI_LIBS
		"${HOUDINI_ROOT}/custom/houdini/dsolib/*.a"
		"${HOUDINI_ROOT}/custom/houdini/dsolib/*.lib"
	)
elseif(P4H_LINUX)
	link_directories(${HOUDINI_ROOT}/dsolib)
	set(HOUDINI_LIBS "HoudiniUI;HoudiniOPZ;HoudiniOP3;HoudiniOP2;HoudiniOP1;HoudiniSIM;HoudiniGEO;HoudiniPRM;HoudiniUT;GLU;GL;X11;Xext;Xi;dl;pthread")
endif()

add_library(${PROJECT} SHARED 
	SOP_prt4houdini.cpp
	utils.cpp
)

target_link_libraries(${PROJECT} ${HOUDINI_LIBS} ${PRT_LINK_LIBRARIES} ${Boost_LIBRARIES})

if(P4H_WINDOWS)
	#target_link_libraries(${PROJECT} IPHlpApi Psapi DbgHelp)
elseif(P4H_LINUX)
	set_target_properties(${PROJECT} PROPERTIES PREFIX "")
endif()
set_target_properties(${PROJECT} PROPERTIES LINKER_LANGUAGE CXX)


### setup install target

install(TARGETS ${PROJECT} 
	RUNTIME DESTINATION ${HOUDINI_DSO_PATH}
	LIBRARY DESTINATION ${HOUDINI_DSO_PATH}
)
install(FILES ${PRT_LIBRARIES} DESTINATION ${HOUDINI_DSO_PATH})
install(FILES ${PRT_EXT_LIBRARIES} DESTINATION "${HOUDINI_DSO_PATH}/prtlib")


# install some test data

#file(GLOB TESTDATA "${CMAKE_CURRENT_LIST_DIR}/../../../testdata/*.rpk")
#install(FILES ${TESTDATA} DESTINATION "${HOUDINI_DSO_PATH}/prtdata")
