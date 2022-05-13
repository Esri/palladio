### setup conan

set(PLD_CONAN_TOOLS "${CMAKE_SOURCE_DIR}/../conan")
include(${PLD_CONAN_TOOLS}/conan-0.15.cmake)
conan_check(VERSION 1.20 REQUIRED)


### select Houdini version and required toolchain

if (NOT PLD_HOUDINI_VERSION)
	set(PLD_HOUDINI_VERSION "19.0") # use Houdini 19.0 by default
endif()

string(REPLACE "." ";" PLD_HDN_VER "${PLD_HOUDINI_VERSION}")
list(GET PLD_HDN_VER 0 PLD_HDN_VER_MAJ)
list(GET PLD_HDN_VER 1 PLD_HDN_VER_MIN)

# Houdini 19.0
if (${PLD_HDN_VER_MAJ} STREQUAL "19" AND ${PLD_HDN_VER_MIN} STREQUAL "0")
	message(STATUS "Asking Conan for Houdini 19.0...")
	set(PLD_CONANFILE "conanfile-h190.py")

# Houdini 18.5
elseif (${PLD_HDN_VER_MAJ} STREQUAL "18" AND ${PLD_HDN_VER_MIN} STREQUAL "5")
	message(STATUS "Asking Conan for Houdini 18.5...")
	set(PLD_CONANFILE "conanfile-h185.py")

# Houdini 18.0
elseif (${PLD_HDN_VER_MAJ} STREQUAL "18" AND ${PLD_HDN_VER_MIN} STREQUAL "0")
	message(STATUS "Asking Conan for Houdini 18.0...")
	set(PLD_CONANFILE "conanfile-h180.py")

endif()


### select required CE SDK version

if(PLD_CONAN_CESDK_DIR)
	# make conan ignore cesdk package, we'll set its path manually
	set(PLD_CONAN_ENV "PLD_CONAN_SKIP_CESDK=1")
endif()


### run conan

if(PLD_WINDOWS)
	set(PLD_CONAN_PROFILE "${PLD_CONAN_TOOLS}/profiles/windows-v142")
elseif(PLD_LINUX)
	set(PLD_CONAN_PROFILE "${PLD_CONAN_TOOLS}/profiles/linux-gcc93")
endif()

conan_cmake_run(CONANFILE ${PLD_CONANFILE} PROFILE ${PLD_CONAN_PROFILE} BASIC_SETUP CMAKE_TARGETS ENV ${PLD_CONAN_ENV})


### PRT dependency

if(PLD_CONAN_CESDK_DIR)
	set(prt_DIR "${PLD_CONAN_CESDK_DIR}/cmake")
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

# whitelist required PRT extension libraries
set(PLD_PRT_EXT_LIBRARIES ${PRT_EXT_LIBRARIES})
list(FILTER PLD_PRT_EXT_LIBRARIES INCLUDE REGEX "com\\.esri\\.prt\\.codecs|com\\.esri\\.prt\\.adaptors|VueExport")
install(FILES ${PLD_PRT_EXT_LIBRARIES} DESTINATION "${HOUDINI_RELATIVE_PALLADIO_PATH}/${PRT_RELATIVE_EXTENSION_PATH}")

function(pld_add_dependency_prt TGT)
	target_compile_definitions(${TGT} PRIVATE -DPRT_VERSION_MAJOR=${PRT_VERSION_MAJOR} -DPRT_VERSION_MINOR=${PRT_VERSION_MINOR})
	target_include_directories(${TGT} PRIVATE ${PRT_INCLUDE_PATH})
	target_link_libraries(${TGT} PRIVATE ${PRT_LINK_LIBRARIES})
endfunction()


### Houdini dependency

list(APPEND CMAKE_PREFIX_PATH "${CONAN_HOUDINI_ROOT}/toolkit/cmake")
find_package(Houdini REQUIRED)

function(pld_add_dependency_houdini TGT)
	target_compile_definitions(${TGT} PRIVATE -DHOUDINI_VERSION_MAJOR=${PLD_HDN_VER_MAJ} -DHOUDINI_VERSION_MINOR=${PLD_HDN_VER_MIN})

	if(${Houdini_VERSION_MAJOR} STREQUAL "19")
		target_compile_definitions(${TGT} PRIVATE -DPLD_BOOST_NS=hboost)
		if(PLD_WINDOWS)
			set(houdini_lib_path "${_houdini_install_root}/custom/houdini/dsolib")
			set(houdini_hboost_fs hboost_filesystem-mt-x64)
		elseif(PLD_LINUX)
			set(houdini_lib_path "${_houdini_install_root}/dsolib")
			set(houdini_hboost_fs hboost_filesystem)
		endif()
		find_library(HBOOST_FILESYSTEM ${houdini_hboost_fs} ${houdini_lib_path})
		target_link_libraries(${TGT} PRIVATE Houdini ${HBOOST_FILESYSTEM})
		houdini_configure_target(${TGT} INSTDIR ${HOUDINI_USER_PATH})
	elseif(${Houdini_VERSION_MAJOR} STREQUAL "18")
		target_compile_definitions(${TGT} PRIVATE -DPLD_BOOST_NS=hboost)
		if(PLD_WINDOWS)
			set(houdini_lib_path "${_houdini_install_root}/custom/houdini/dsolib")
			if (${Houdini_VERSION_MINOR} STREQUAL "5")
				set(houdini_hboost_fs hboost_filesystem-mt-x64)
			else()
				set(houdini_hboost_fs hboost_filesystem-mt)
			endif()
		elseif(PLD_LINUX)
			set(houdini_lib_path "${_houdini_install_root}/dsolib")
			set(houdini_hboost_fs hboost_filesystem)
		endif()
		find_library(HBOOST_FILESYSTEM ${houdini_hboost_fs} ${houdini_lib_path})
		target_link_libraries(${TGT} PRIVATE Houdini ${HBOOST_FILESYSTEM})
		houdini_configure_target(${TGT} INSTDIR ${HOUDINI_USER_PATH})
	elseif(${Houdini_VERSION_MAJOR} STREQUAL "17")
		target_compile_definitions(${TGT} PRIVATE -DPLD_BOOST_NS=hboost)
		if(PLD_WINDOWS)
			set(houdini_lib_path "${_houdini_install_root}/custom/houdini/dsolib")
			set(houdini_hboost_fs hboost_filesystem-mt)
		elseif(PLD_LINUX)
			set(houdini_lib_path "${_houdini_install_root}/dsolib")
			set(houdini_hboost_fs hboost_filesystem)
		endif()
		find_library(HBOOST_FILESYSTEM ${houdini_hboost_fs} ${houdini_lib_path})
		target_link_libraries(${TGT} PRIVATE Houdini ${HBOOST_FILESYSTEM})
		houdini_configure_target(${TGT})
	elseif(${Houdini_VERSION_MAJOR} STREQUAL "16")
		target_link_libraries(${TGT} PRIVATE Houdini)
		target_compile_definitions(${TGT} PRIVATE -DPLD_BOOST_NS=boost)
	endif()
endfunction()


### catch2 dependency

function(pld_add_dependency_catch2 TGT)
	target_link_libraries(${TGT} PRIVATE CONAN_PKG::catch2)
endfunction()
