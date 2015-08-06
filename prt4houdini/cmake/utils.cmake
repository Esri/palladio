set(CMAKE_INCLUDE_CURRENT_DIR ON)

macro (add_sources)
	foreach (_src ${ARGN})
		list (APPEND SOURCES "${_src}")
	endforeach()
	set(SOURCES ${SOURCES} PARENT_SCOPE)
endmacro()


macro (add_subdirectory_source_group thedir)
	add_subdirectory(${thedir})
	add_sf_sources_to_source_group(${thedir})				# msvc sln
endmacro()


macro (add_sf_sources)
	file(GLOB HDR "*.h")
	add_sources(${HDR})

	file(GLOB FS_FILESH RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.h")
	file(GLOB FS_FILESC RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.cpp")

	set(FS_FILES ${FS_FILESH} ${FS_FILESC})

	foreach (_src ${FS_FILES})
		list (APPEND SF_SOURCEStmp "${_src}")
	endforeach()
	set(SF_SOURCES ${SF_SOURCEStmp} PARENT_SCOPE)
endmacro()


function (add_sf_sources_to_source_group prefix)
	foreach (_src ${SF_SOURCES})
		list (APPEND SF_EXPANDED "${prefix}/${_src}")
	endforeach()
	STRING(REGEX REPLACE "/" "\\\\" SourceGroup ${prefix} )
	SOURCE_GROUP(src\\${SourceGroup} FILES ${SF_EXPANDED})
endfunction()


# proudly stolen from http://www.cmake.org/pipermail/cmake/2010-November/041034.html
macro(TODAY RESULT)
    if(WIN32)
        execute_process(COMMAND "cmd" " /C date /T" OUTPUT_VARIABLE DATE)
        execute_process(COMMAND "cmd" " /C time /T" OUTPUT_VARIABLE TIME)
        string(REGEX REPLACE "([ ]?\r?\n)+$" "" DATE ${DATE})
        string(REGEX REPLACE "([ ]?\r?\n)+$" "" TIME ${TIME})
        set(${RESULT} "${DATE} ${TIME}")
    elseif(UNIX OR APPLE)
        execute_process(COMMAND "date" "+%Y-%m-%d %H:%M" OUTPUT_VARIABLE ${RESULT}) 
        string(REGEX REPLACE "(\r?\n)+$" "" ${RESULT} ${${RESULT}})
    else()
        message(SEND_ERROR "date not implemented")
        set(${RESULT} 000000)
    endif()
endmacro(TODAY)


# proudly stolen from CGAL's cmake build system
macro(prt_get_compiler_version)
    set(search_dirs "")
    #message("Compiler version:")
    set(version "Unknown compiler. Cannot display its version")
    if(MSVC)
      execute_process(COMMAND "${CMAKE_CXX_COMPILER}"
        RESULT_VARIABLE ok
        ERROR_VARIABLE out_version
        TIMEOUT 5)
      if(ok EQUAL 0)
        set(version "${out_version}")
      endif()
    else()
      foreach(flag "-V" "--version" "-v")
        execute_process(COMMAND "${CMAKE_CXX_COMPILER}" ${flag}
          RESULT_VARIABLE ok
          OUTPUT_VARIABLE out_version
          ERROR_VARIABLE out_version
          TIMEOUT 5)
        if(ok EQUAL 0)
          if("${out_version}" MATCHES "^clang")
            execute_process(COMMAND "${CMAKE_CXX_COMPILER}" -print-search-dirs
              RESULT_VARIABLE ok
              OUTPUT_VARIABLE out_search_dirs
              TIMEOUT 5)
            if(ok EQUAL 0)
              set(search_dirs "${out_search_dirs}")
            endif()
          endif()
          set(version "${out_version}")
          break()
        endif()
      endforeach()
    endif()
    #message("${version}")
    set(PRT_COMPILER_VERSION ${version})
    if(search_dirs)
      message("Search dirs:")
      message("${search_dirs}")
    endif()
endmacro(prt_get_compiler_version)


macro(prt_install_clear)
	if(PRT_BUILD_INSTALL_WIPE EQUAL 1)
		install(CODE "MESSAGE(STATUS \"(PRT) wiping installation dir\")")
		install(CODE "file(REMOVE_RECURSE ${CMAKE_INSTALL_PREFIX})")
	endif()
endmacro()


function(prt_get_dependency REPOID GROUPID ARTIFACTID VERSION CLASSIFIER CONFIG_VAR)
	set(PRT_ARTIFACT_REPOSITORY_URI "http://zrh-rcs.esri.com:8081/nexus/content/repositories/${REPOID}")
	if(NOT PRT_DEPENDENCY_CACHE_PATH)
		message(FATAL_ERROR "(PRT) Please set PRT_DEPENDENCY_CACHE_PATH")
	endif()
	if(NOT EXISTS ${PRT_DEPENDENCY_CACHE_PATH})
		execute_process(COMMAND "cmake -E make_directory ${PRT_DEPENDENCY_CACHE_PATH}")
	endif()

	set(DEP_BASENAME	"${ARTIFACTID}-${VERSION}-${CLASSIFIER}")
	set(DEP_BASEDIR		"${PRT_DEPENDENCY_CACHE_PATH}/${DEP_BASENAME}")

	string(REPLACE "." "/" GROUP_PATH ${GROUPID})
	set(DEP_URL			"${PRT_ARTIFACT_REPOSITORY_URI}/${GROUP_PATH}/${ARTIFACTID}/${VERSION}/${DEP_BASENAME}.zip")
	#message("DEP_URL = ${DEP_URL}")

	set(${CONFIG_VAR} "${DEP_BASEDIR}/cmake" CACHE INTERNAL ${CONFIG_VAR})
	#message("${CONFIG_VAR} = ${${CONFIG_VAR}}")
	if(NOT EXISTS ${${CONFIG_VAR}})
		message(STATUS "(PRT) ${DEP_BASENAME} not found in local dependency cache, downloading...")

		file(
			DOWNLOAD ${DEP_URL} "${DEP_BASEDIR}.zip"
			STATUS DOWNLOADSTATUSLIST
			#SHOW_PROGRESS
		)
		list(GET DOWNLOADSTATUSLIST 0 DOWNLOADSTATUS)
		if(NOT ${DOWNLOADSTATUS} EQUAL 0)
			message(FATAL_ERROR "(PRT) Cannot download dependency ${DEP_URL}")
		endif()

		execute_process(
			COMMAND ${CMAKE_COMMAND} -E tar xf ${DEP_BASEDIR}.zip
			WORKING_DIRECTORY ${PRT_DEPENDENCY_CACHE_PATH}
			ERROR_VARIABLE EXTRACTSTATUS
		)
		if(${EXTRACTSTATUS})
			message(FATAL_ERROR "(PRT) Cannot extract dependency ${DEP_BASEDIR}.zip")
		endif()

		execute_process(
			COMMAND ${CMAKE_COMMAND} -E remove ${DEP_BASEDIR}.zip
			WORKING_DIRECTORY ${PRT_DEPENDENCY_CACHE_PATH}
			ERROR_VARIABLE RMSTATUS
		)
		if(${RMSTATUS})
			message(FATAL_ERROR "(PRT) Cannot remove dependency archive ${DEP_BASEDIR}.zip")
		endif()
	endif()

endfunction()


# temporary workaround hack
function(get_dep_classifier_components OS TC RESULT_OS RESULT_TC)
	string(REPLACE "_" ";" L ${TC})
	list(GET L 2 S)
	string(TOLOWER ${S} LS)

	set(${RESULT_OS} "${OS}" PARENT_SCOPE)
	if (${OS} STREQUAL "linux")
		set(LS gcc44)
		set(${RESULT_OS} "rhel6" PARENT_SCOPE)
	elseif (${OS} STREQUAL "macosx")
		set(LS gcc42)
	elseif (${LS} STREQUAL "vc9")
		set(LS vc90)
	elseif(${LS} STREQUAL "vc10")
		set(LS vc100)
	elseif(${LS} STREQUAL "vc11")
		set(LS vc110)
	endif()

	set(${RESULT_TC} ${LS} PARENT_SCOPE)
endfunction()
