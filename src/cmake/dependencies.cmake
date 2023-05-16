### setup conan

set(PLD_CONAN_TOOLS "${CMAKE_SOURCE_DIR}/../conan")
include(${PLD_CONAN_TOOLS}/conan-0.18.1.cmake)
conan_check(VERSION 1.20 REQUIRED)


### select Houdini version and required toolchain

if (NOT PLD_HOUDINI_VERSION)
	set(PLD_HOUDINI_VERSION "19.5") # use Houdini 19.5 by default
endif()

string(REPLACE "." ";" PLD_HDN_VER "${PLD_HOUDINI_VERSION}")
list(GET PLD_HDN_VER 0 PLD_HDN_VER_MAJ)
list(GET PLD_HDN_VER 1 PLD_HDN_VER_MIN)

# Houdini 19.5
if (${PLD_HDN_VER_MAJ} STREQUAL "19" AND ${PLD_HDN_VER_MIN} STREQUAL "5")
	message(STATUS "Asking Conan for Houdini 19.5...")
	set(PLD_CONANFILE "conanfile-h195.py")

# Houdini 19.0
elseif (${PLD_HDN_VER_MAJ} STREQUAL "19" AND ${PLD_HDN_VER_MIN} STREQUAL "0")
	message(STATUS "Asking Conan for Houdini 19.0...")
	set(PLD_CONANFILE "conanfile-h190.py")

# Houdini 18.5
elseif (${PLD_HDN_VER_MAJ} STREQUAL "18" AND ${PLD_HDN_VER_MIN} STREQUAL "5")
	message(STATUS "Asking Conan for Houdini 18.5...")
	set(PLD_CONANFILE "conanfile-h185.py")

endif()


### select required CE SDK version

option(PLD_CESDK_DIR:PATH "Path to local CESDK distribution. Must point to the directory containing prtConfig.cmake, i.e. the 'cmake' sub-directory.")
if(PLD_CESDK_DIR)
	# make conan ignore cesdk package, we'll set its path manually
	set(PLD_CONAN_ENV "PLD_CONAN_SKIP_CESDK=1")
endif()


### run conan

if(PLD_WINDOWS)
	set(PLD_CONAN_PROFILE "${PLD_CONAN_TOOLS}/profiles/windows-v142")
elseif(PLD_LINUX)
	set(PLD_CONAN_PROFILE "${PLD_CONAN_TOOLS}/profiles/linux-gcc93")
endif()

if (PLD_GEN_VISUAL_STUDIO AND NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "RelWithDebInfo") # workaround for using conan with the Visual Studio multi-generator
endif()

conan_cmake_run(CONANFILE ${PLD_CONANFILE} PROFILE ${PLD_CONAN_PROFILE} BASIC_SETUP NO_OUTPUT_DIRS CMAKE_TARGETS ENV ${PLD_CONAN_ENV})


### PRT dependency

if(PLD_CESDK_DIR)
	set(prt_DIR "${PLD_CESDK_DIR}")
	message(STATUS "Ignoring conan package for cesdk, using local path: ${prt_DIR}")
else()
	message(STATUS "CONAN_CESDK_ROOT = ${CONAN_CESDK_ROOT}")
	list(APPEND CMAKE_PREFIX_PATH "${CONAN_CESDK_ROOT}/esri_ce_sdk/cmake")
endif()

find_package(prt CONFIG REQUIRED)

# filter import libraries (Windows)
set(PLD_PRT_LIBRARIES ${PRT_LIBRARIES})
list(FILTER PLD_PRT_LIBRARIES EXCLUDE REGEX "\\.lib")
install(FILES ${PLD_PRT_LIBRARIES} DESTINATION ${HOUDINI_RELATIVE_PALLADIO_PATH})

# allow-list required PRT extension libraries
set(PLD_PRT_EXT_LIBRARIES ${PRT_EXT_LIBRARIES})
list(FILTER PLD_PRT_EXT_LIBRARIES INCLUDE REGEX "com\\.esri\\.prt\\.(codecs|oda|usd|adaptors)|libcrypto|tbb|usd_ms")
install(FILES ${PLD_PRT_EXT_LIBRARIES} DESTINATION "${HOUDINI_RELATIVE_PALLADIO_PATH}/${PRT_RELATIVE_EXTENSION_PATH}")

# the USD codecs need resource files
install(DIRECTORY ${PRT_EXTENSION_PATH}/usd DESTINATION "${HOUDINI_RELATIVE_PALLADIO_PATH}/${PRT_RELATIVE_EXTENSION_PATH}")

function(pld_add_dependency_prt TGT)
	target_compile_definitions(${TGT} PRIVATE -DPRT_VERSION_MAJOR=${PRT_VERSION_MAJOR} -DPRT_VERSION_MINOR=${PRT_VERSION_MINOR})
	target_include_directories(${TGT} PRIVATE ${PRT_INCLUDE_PATH})
	target_link_libraries(${TGT} PRIVATE ${PRT_LINK_LIBRARIES})
endfunction()


### Houdini dependency

list(APPEND CMAKE_PREFIX_PATH "${CONAN_HOUDINI_ROOT}/toolkit/cmake")
find_package(Houdini REQUIRED)

# create two dependency lists with Houdini libraries: one for the main module and a patched one for the FS module
get_target_property(PLD_HDN_TGT_LIBS_FS Houdini INTERFACE_LINK_LIBRARIES)
set(PLD_HDN_TGT_LIBS ${PLD_HDN_TGT_LIBS_FS})
list(REMOVE_ITEM PLD_HDN_TGT_LIBS_FS Houdini::HoudiniAPPS3 Houdini::HoudiniAPPS2
		Houdini::HoudiniUI Houdini::HoudiniHARD Houdini::HoudiniHAPIL
		Houdini::HoudiniAPPS1 Houdini::HoudiniDEVICE)

function(pld_select_houdini_dependency_list TGT)
	if("${TGT}" STREQUAL "${TGT_FS}")
		set_property(TARGET Houdini PROPERTY INTERFACE_LINK_LIBRARIES  ${PLD_HDN_TGT_LIBS_FS} )
		message(STATUS "${TGT}: Using filtered Houdini target list for Mantra compatibility: ${PLD_HDN_TGT_LIBS_FS}")
	else()
		set_property(TARGET Houdini PROPERTY INTERFACE_LINK_LIBRARIES  ${PLD_HDN_TGT_LIBS} )
	endif()
endfunction()

# force UT_ASSERT_LEVEL=0 for Windows/MSVC (the UT_ASSERT macros trigger a linker error, probably due to a MSVC compiler bug)
if(PLD_WINDOWS)
	get_target_property(PLD_HDN_TGT_DEFS Houdini INTERFACE_COMPILE_DEFINITIONS)
	list(REMOVE_ITEM PLD_HDN_TGT_DEFS $<$<CONFIG:Release>:UT_ASSERT_LEVEL=0>)
	list(REMOVE_ITEM PLD_HDN_TGT_DEFS $<$<CONFIG:RelWithDebInfo>:UT_ASSERT_LEVEL=1>)
	list(REMOVE_ITEM PLD_HDN_TGT_DEFS $<$<CONFIG:Debug>:UT_ASSERT_LEVEL=2>)
	list(APPEND PLD_HDN_TGT_DEFS UT_ASSERT_LEVEL=0)
	set_property(TARGET Houdini PROPERTY INTERFACE_COMPILE_DEFINITIONS ${PLD_HDN_TGT_DEFS})
endif()

function(pld_add_dependency_houdini TGT)
	target_compile_definitions(${TGT} PRIVATE -DHOUDINI_VERSION_MAJOR=${PLD_HDN_VER_MAJ} -DHOUDINI_VERSION_MINOR=${PLD_HDN_VER_MIN})
	pld_select_houdini_dependency_list(${TGT})
	target_link_libraries(${TGT} PRIVATE Houdini)
endfunction()


### catch2 dependency

function(pld_add_dependency_catch2 TGT)
	target_link_libraries(${TGT} PRIVATE CONAN_PKG::catch2)
endfunction()
