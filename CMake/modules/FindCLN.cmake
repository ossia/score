# CLN_FOUND		CLN has been successfully found
# CLN_INCLUDE_DIR       the include directories
# CLN_LIBRARIES         CLN library and its dependencies (if any)

if (CLN_INCLUDE_DIR AND CLN_LIBRARIES)
  set(CLN_FIND_QUIETLY TRUE)
endif()

function(_cl_get_version _out_major _out_minor _out_patch _cl_version_h)
  file(STRINGS ${_cl_version_h} _cl_vinfo REGEX "^#define[\t ]+CL_VERSION_.*")
  if (NOT _cl_vinfo)
    message(FATAL_ERROR "include file ${_cl_version_h} does not exist")
  endif()
  string(REGEX REPLACE "^.*CL_VERSION_MAJOR[ \t]+([0-9]+).*" "\\1" ${_out_major} "${_cl_vinfo}")
  string(REGEX REPLACE "^.*CL_VERSION_MINOR[ \t]+([0-9]+).*" "\\1" ${_out_minor} "${_cl_vinfo}")
  string(REGEX REPLACE "^.*CL_VERSION_PATCHLEVEL[ \t]+([0-9]+).*" "\\1" ${_out_patch} "${_cl_vinfo}")
  if (NOT ${_out_major} MATCHES "[0-9]+")
    message(FATAL_ERROR "failed to determine CL_VERSION_MAJOR, "
                  "expected a number, got ${${_out_major}}")
  endif()
  if (NOT ${_out_minor} MATCHES "[0-9]+")
    message(FATAL_ERROR "failed to determine CL_VERSION_MINOR, "
                  "expected a number, got ${${_out_minor}}")
  endif()
  if (NOT ${_out_patch} MATCHES "[0-9]+")
    message(FATAL_ERROR "failed to determine CL_VERSION_PATCHLEVEL, "
                  "expected a number, got ${${_out_patch}}")
  endif()
  message(STATUS "found CLN [${_cl_version_h}], version ${${_out_major}}.${${_out_minor}}.${${_out_patch}}")
  set(${_out_major} ${${_out_major}} PARENT_SCOPE)
  set(${_out_minor} ${${_out_minor}} PARENT_SCOPE)
  set(${_out_patch} ${${_out_patch}} PARENT_SCOPE)
endfunction()

set(CLN_FOUND)
set(CLN_INCLUDE_DIR)
set(CLN_LIBRARIES)

include(FindPkgConfig)
if (PKG_CONFIG_FOUND)
  pkg_check_modules(_cln cln)
endif()

find_path(CLN_INCLUDE_DIR NAMES cln/cln.h
        HINTS ${_cln_INCLUDE_DIRS}
        $ENV{CLN_DIR}/include)
find_library(CLN_LIBRARIES NAMES libcln cln
         HINTS ${_cln_LIBRARY_DIR}
               ${_cln_LIBRARY_DIRS}
         $ENV{CLN_DIR}/lib)

if (CLN_INCLUDE_DIR)
  _cl_get_version(CLN_VERSION_MAJOR
      CLN_VERSION_MINOR
      CLN_VERSION_PATCHLEVEL
      ${CLN_INCLUDE_DIR}/cln/version.h)
  set(CLN_VERSION ${CLN_VERSION_MAJOR}.${CLN_VERSION_MINOR}.${CLN_VERSION_PATCHLEVEL})
  # Check if the version reported by pkg-config is the same
  # as the one read from the header. This prevents us from
  # picking the wrong version of CLN (say, if several versions
  # are installed)
  if (_cln_FOUND AND NOT CLN_VERSION VERSION_EQUAL _cln_VERSION)
    if (NOT CLN_FIND_QUIETLY)
      message(ERROR "pkg-config and version.h disagree, "
              "${_cln_VERSION} vs ${CLN_VERSION}, "
              "please check your installation")
    endif()
    set(CLN_LIBRARIES CLN-NOTFOUND)
    set(CLN_INCLUDE_DIR CLN-NOTFOUND)
    set(CLN_LIBRARY_DIRS)
    set(CLN_VERSION)
  endif()
endif()

# Check if the version embedded into the library is the same as the one in the headers.
if (CLN_INCLUDE_DIR AND CLN_LIBRARIES AND NOT CMAKE_CROSSCOMPILING)
  include(CheckCXXSourceRuns)
  set(_save_required_includes ${CMAKE_REQUIRED_INCLUDES})
  set(_save_required_libraries ${CMAKE_REQUIRED_LIBRARIES})
  set(CMAKE_REQUIRED_INCLUDES ${CMAKE_REQUIRED_INCLUDES} ${CLN_INCLUDE_DIR})
  set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} ${CLN_LIBRARIES})
  check_cxx_source_runs("
    #include <cln/version.h>
    int main() {
      return (CL_VERSION_MAJOR == cln::version_major) &&
             (CL_VERSION_MINOR == cln::version_minor) &&
             (CL_VERSION_PATCHLEVEL == cln::version_patchlevel) ? 0 : 1;
    }
    "
    _cl_version_matches)
  set(CMAKE_REQUIRED_LIBRARIES ${_save_required_libraries})
  set(CMAKE_REQUIRED_INCLUDES ${_save_required_includes})
  if (NOT _cl_version_matches)
    if (NOT CLN_FIND_QUIETLY)
      message(ERROR "header (version differs from the library one, "
              "please check your installation.")
    endif()
    set(CLN_INCLUDE_DIR CLN-NOTFOUND)
    set(CLN_LIBRARIES CLN-NOTFOUND)
    set(CLN_LIBRARY_DIRS)
    set(CLN_VERSION)
  endif()
endif()

if (CLN_LIBRARIES AND CLN_INCLUDE_DIR)
  set(_cln_library_dirs)
  foreach(_l ${CLN_LIBRARIES})
    get_filename_component(_d "${_l}" PATH)
    list(APPEND _cln_library_dirs "${_d}")
  endforeach()
  list(REMOVE_DUPLICATES _cln_library_dirs)
  set(CLN_LIBRARY_DIRS ${_cln_library_dirs})
endif()

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(CLN REQUIRED_VARS CLN_LIBRARIES CLN_INCLUDE_DIR
              VERSION_VAR CLN_VERSION)

