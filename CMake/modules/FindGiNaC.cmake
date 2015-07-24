# GINAC_FOUND		CiNaC has been successfully found
# GINAC_INCLUDE_DIRS    the include directories
# GINAC_LIBRARIES       GiNaC library and its dependencies

if (GINAC_INCLUDE_DIRS AND GINAC_LIBRARIES)
	set(GINAC_FIND_QUIETLY TRUE)
endif()

function(_ginac_headers_version _out_major _out_minor _out_patch _version_h)
	file(STRINGS ${_version_h} _ginac_vinfo REGEX "^#define[\t ]+GINACLIB_.*_VERSION.*")
	if (NOT _ginac_vinfo)
		message(FATAL_ERROR "include file ${_version_h} does not exist")
	endif()
	string(REGEX REPLACE "^.*GINACLIB_MAJOR_VERSION[ \t]+([0-9]+).*" "\\1" ${_out_major} "${_ginac_vinfo}")
	string(REGEX REPLACE "^.*GINACLIB_MINOR_VERSION[ \t]+([0-9]+).*" "\\1" ${_out_minor} "${_ginac_vinfo}")
	string(REGEX REPLACE "^.*GINACLIB_MICRO_VERSION[ \t]+([0-9]+).*" "\\1" ${_out_patch} "${_ginac_vinfo}")
	if (NOT ${_out_major} MATCHES "[0-9]+")
		message(FATAL_ERROR "failed to determine GINACLIB_MAJOR_VERSION, "
			            "expected a number, got ${${_out_major}}")
	endif()
	if (NOT ${_out_minor} MATCHES "[0-9]+")
		message(FATAL_ERROR "failed to determine GINACLIB_MINOR_VERSION, "
			            "expected a number, got ${${_out_minor}}")
	endif()
	if (NOT ${_out_patch} MATCHES "[0-9]+")
		message(FATAL_ERROR "failed to determine GINACLIB_MINOR_VERSION, "
			            "expected a number, got ${${_out_patch}}")
	endif()
	set(${_out_major} ${${_out_major}} PARENT_SCOPE)
	set(${_out_minor} ${${_out_minor}} PARENT_SCOPE)
	set(${_out_patch} ${${_out_patch}} PARENT_SCOPE)
endfunction()

set(GINAC_FOUND)
set(GINAC_INCLUDE_DIRS)
set(GINAC_LIBRARIES)
set(GINAC_LIBRARY_DIRS)
set(GINAC_VERSION)

include(FindPkgConfig)
find_package(CLN 1.2.2)

if (PKG_CONFIG_FOUND)
	pkg_check_modules(_ginac ginac)
else()
	set(_ginac_LIBRARIES ginac cln gmp)
endif()

if (NOT CLN_FOUND)
	set(GINAC_INCLUDE_DIRS GINAC-NOTFOUND)
	set(GINAC_LIBRARIES GINAC-NOTFOUND)
else()
	find_path(_ginac_include_dir NAMES ginac/ginac.h
				     HINTS ${_ginac_INCLUDE_DIRS}
				           $ENV{GINAC_DIR}/include)
	if (_ginac_include_dir)
		set(GINAC_INCLUDE_DIRS ${_ginac_include_dir}
				       ${_ginac_INCLUDE_DIRS}
				       ${CLN_INCLUDE_DIR})
		list(REMOVE_DUPLICATES GINAC_INCLUDE_DIRS)
	else()
		set(GINAC_INCLUDE_DIRS GINAC-NOTFOUND)
		set(GINAC_LIBRARIES GINAC-NOTFOUND)
		if (NOT GINAC_FIND_QUIETLY)
			message(ERROR "couldn't find ginac.h")
		endif()
	endif()

	if (GINAC_INCLUDE_DIRS)
		find_library(_ginac_lib NAMES libginac ginac
					HINTS ${_ginac_LIBRARY_DIRS}
					      $ENV{GINAC_DIR}/lib)
		if (_ginac_lib)
			set(GINAC_LIBRARIES ${_ginac_lib} ${CLN_LIBRARIES})
			list(REMOVE_DUPLICATES GINAC_LIBRARIES)
		else()
			set(GINAC_LIBRARIES GINAC-NOTFOUND)
			set(GINAC_INCLUDE_DIRS GINAC-NOTFOUND)
			if (NOT GINAC_FIND_QUIETLY)
				message(ERROR "couldn't find libginac")
			endif()
		endif()
	endif()
endif()

if (GINAC_INCLUDE_DIRS)
	_ginac_headers_version(GINACLIB_MAJOR_VERSION
			       GINACLIB_MINOR_VERSION
			       GINACLIB_MICRO_VERSION
			       ${_ginac_include_dir}/ginac/version.h)
	set(GINAC_VERSION ${GINACLIB_MAJOR_VERSION}.${GINACLIB_MINOR_VERSION}.${GINACLIB_MICRO_VERSION})
	# Check if the version reported by pkg-config is the same
	# as the one read from the header. This prevents us from
	# picking the wrong version of GINAC (say, if several versions
	# are installed)
	if (PKG_CONFIG_FOUND AND NOT GINAC_VERSION VERSION_EQUAL _ginac_VERSION)
		if (NOT CLN_FIND_QUIETLY)
			message(ERROR "pkg-config and version.h disagree, "
				      "${_ginac_VERSION} vs ${GINAC_VERSION}, "
				      "please check your installation")
		endif()
		set(GINAC_LIBRARIES GINAC-NOTFOUND)
		set(GINAC_INCLUDE_DIRS GINAC-NOTFOUND)
		set(GINAC_LIBRARY_DIRS)
		set(GINAC_VERSION)
	endif()
endif()

# Check if the version embedded into the library is the same as the one in the headers.
if (GINAC_INCLUDE_DIRS AND GINAC_LIBRARIES AND NOT CMAKE_CROSSCOMPILING)
	include(CheckCXXSourceRuns)
	set(_save_required_includes ${CMAKE_REQUIRED_INCLUDES})
	set(_save_required_libraries ${CMAKE_REQUIRED_LIBRARIES})
	set(CMAKE_REQUIRED_INCLUDES ${CMAKE_REQUIRED_INCLUDES} ${GINAC_INCLUDE_DIRS})
	set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} ${GINAC_LIBRARIES})
	check_cxx_source_runs("
		#include <ginac/version.h>
		#include <cln/version.h>
		#include <stdio.h>
		int main() {
			return (CL_VERSION_MAJOR == cln::version_major) &&
			       (CL_VERSION_MINOR == cln::version_minor) &&
			       (CL_VERSION_PATCHLEVEL == cln::version_patchlevel) &&
			       (GINACLIB_MAJOR_VERSION == GiNaC::version_major) &&
			       (GINACLIB_MINOR_VERSION == GiNaC::version_minor) &&
			       (GINACLIB_MICRO_VERSION == GiNaC::version_micro) ? 0 : 1;
		}
		"
		_ginac_version_matches)
	set(CMAKE_REQUIRED_LIBRARIES ${_save_required_libraries})
	set(CMAKE_REQUIRED_INCLUDES ${_save_required_includes})
	if (NOT _ginac_version_matches)
		if (NOT GINAC_FIND_QUIETLY)
			message(ERROR "header version differs from the library one, "
				      "please check your installation.")
		endif()
		set(GINAC_INCLUDE_DIRS GINAC-NOTFOUND)
		set(GINAC_LIBRARIES GINAC_NOTFOUND)
		set(GINAC_LIBRARY_DIRS)
		set(GINAC_VERSION)
	endif()
endif()

if (GINAC_LIBRARIES AND GINAC_INCLUDE_DIRS)
	set(_ginac_library_dirs)
	foreach(_l ${GINAC_LIBRARIES})
		get_filename_component(_d "${_l}" PATH)
		list(APPEND _ginac_library_dirs "${_d}")
	endforeach()
	list(REMOVE_DUPLICATES _ginac_library_dirs)
	set(GINAC_LIBRARY_DIRS ${_ginac_library_dirs})
endif()

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(GiNaC REQUIRED_VARS GINAC_LIBRARIES GINAC_INCLUDE_DIRS
					VERSION_VAR GINAC_VERSION)

