#taken from https://gist.github.com/sivachandran/3a0de157dccef822a230
include(CMakeParseArguments)

# Function to wrap a given string into multiple lines at the given column position.
# Parameters:
#   VARIABLE    - The name of the CMake variable holding the string.
#   AT_COLUMN   - The column position at which string will be wrapped.
function(WRAP_STRING)
    set(oneValueArgs VARIABLE AT_COLUMN)
    cmake_parse_arguments(WRAP_STRING "" "${oneValueArgs}" "" ${ARGN})

    string(REGEX REPLACE "(.{${WRAP_STRING_AT_COLUMN}})" "\\1\n" wrapped "${${WRAP_STRING_VARIABLE}}")
    set(${WRAP_STRING_VARIABLE} "${wrapped}" PARENT_SCOPE)
endfunction()

# Function to embed contents of a file as byte array in C/C++ header file(.h). The header file
# will contain a byte array and integer variable holding the size of the array.
# Parameters
#   SOURCE_FILE     - The path of source file whose contents will be embedded in the header file.
#   VARIABLE_NAME   - The name of the variable for the byte array. The string "_SIZE" will be append
#                     to this name and will be used a variable name for size variable.
#   HEADER_FILE     - The path of header file.
#   APPEND          - If specified appends to the header file instead of overwriting it
#   NULL_TERMINATE  - If specified a null byte(zero) will be append to the byte array. This will be
#                     useful if the source file is a text file and we want to use the file contents
#                     as string. But the size variable holds size of the byte array without this
#                     null byte.
# Usage:
#   bin2h(SOURCE_FILE "Logo.png" HEADER_FILE "Logo.h" VARIABLE_NAME "LOGO_PNG")
function(BIN2H)
  set(options APPEND NULL_TERMINATE)
  set(oneValueArgs SOURCE_FILE VARIABLE_NAME HEADER_FILE)
  cmake_parse_arguments(BIN2H "${options}" "${oneValueArgs}" "" ${ARGN})

  # reads source file contents as hex string
  file(READ ${BIN2H_SOURCE_FILE} hexString HEX)
  string(LENGTH ${hexString} hexStringLength)

  # appends null byte if asked
  if(BIN2H_NULL_TERMINATE)
      set(hexString "${hexString}00")
  endif()

  # wraps the hex string into multiple lines at column 32(i.e. 16 bytes per line)
  wrap_string(VARIABLE hexString AT_COLUMN 32)
  math(EXPR arraySize "${hexStringLength} / 2")

  # adds '0x' prefix and comma suffix before and after every byte respectively
  string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1, " arrayValues ${hexString})
  # removes trailing comma
  string(REGEX REPLACE ", $" "" arrayValues ${arrayValues})

  # converts the variable name into proper C identifier
  string(MAKE_C_IDENTIFIER "${BIN2H_VARIABLE_NAME}" BIN2H_VARIABLE_NAME)

  # declares byte array and the length variables
  set(arrayDefinition "static const unsigned char ${BIN2H_VARIABLE_NAME}[] = { ${arrayValues} };")
  set(arraySizeDefinition "static const std::size_t ${BIN2H_VARIABLE_NAME}_SIZE = ${arraySize};")

  set(declarations "${arrayDefinition}\n\n${arraySizeDefinition}\n\n")

  set("${BIN2H_HEADER_FILE}" "${${BIN2H_HEADER_FILE}}\n${declarations}" PARENT_SCOPE)
endfunction()

# score_register_license(<name> [URL <url>] [HEADER <text>]
#                         [FILES <file>...] [NOTICES <source file>...])
# Adds a component to the About dialog's license list. FILES are embedded
# whole; NOTICES embed only the first comment block of a source file that
# carries a copyright or license statement, for vendored code whose notice
# lives in its header. Call it where the component is actually built, so the
# list follows the configuration. The first registration of a name wins.
function(score_register_license _name)
  cmake_parse_arguments(_lic "" "URL;HEADER" "FILES;NOTICES" ${ARGN})
  string(MAKE_C_IDENTIFIER "${_name}" _id)
  get_property(_known GLOBAL PROPERTY SCORE_LICENSE_IDS)
  if("${_id}" IN_LIST _known)
    return()
  endif()

  foreach(_f IN LISTS _lic_FILES _lic_NOTICES)
    if(NOT EXISTS "${_f}")
      message(WARNING "License of ${_name}: ${_f} does not exist")
    endif()
  endforeach()

  set_property(GLOBAL APPEND PROPERTY SCORE_LICENSE_IDS "${_id}")
  set_property(GLOBAL PROPERTY SCORE_LICENSE_${_id}_NAME "${_name}")
  set_property(GLOBAL PROPERTY SCORE_LICENSE_${_id}_URL "${_lic_URL}")
  set_property(GLOBAL PROPERTY SCORE_LICENSE_${_id}_HEADER "${_lic_HEADER}")
  set_property(GLOBAL PROPERTY SCORE_LICENSE_${_id}_FILES "${_lic_FILES}")
  set_property(GLOBAL PROPERTY SCORE_LICENSE_${_id}_NOTICES "${_lic_NOTICES}")
endfunction()

# First /* */ or // comment block of a file mentioning a copyright, a license
# or a permission, without its comment markers.
# Writes to _out the third-party components a Qt build lists in the SPDX
# SBOMs it installs (QT_GENERATE_SBOM), with their version, license and
# copyright. Returns FALSE in _ok when there is no SBOM to read.
function(score_qt_sbom_attributions _dir _out _ok)
  file(GLOB _sboms "${_dir}/*.spdx")
  set(${_ok} FALSE PARENT_SCOPE)
  if(NOT _sboms)
    return()
  endif()
  list(SORT _sboms)

  set(_text "Third-party components of Qt, from the SBOM of the Qt build:\n")
  set(_seen "")
  foreach(_f IN LISTS _sboms)
    file(READ "${_f}" _src)
    # Turned into a list of packages: the list separator and the brackets,
    # which group list items in CMake, are hidden until written back.
    string(REPLACE ";" "@score_semi@" _src "${_src}")
    string(REPLACE "[" "@score_lb@" _src "${_src}")
    string(REPLACE "]" "@score_rb@" _src "${_src}")
    string(REPLACE "\nPackageName: " ";" _packages "${_src}")
    foreach(_p IN LISTS _packages)
      if(NOT _p MATCHES "\nSPDXID: SPDXRef-Package-[^\n]*-qt-3rdparty-sources-")
        continue()
      endif()
      if(NOT _p MATCHES "\n +Name: ([^\n]*)")
        continue()
      endif()
      set(_name "${CMAKE_MATCH_1}")
      set(_version "")
      if(_p MATCHES "\nPackageVersion: ([^\n]*)")
        set(_version "${CMAKE_MATCH_1}")
      endif()
      if("${_name} ${_version}" IN_LIST _seen)
        continue()
      endif()
      list(APPEND _seen "${_name} ${_version}")

      string(APPEND _text "\n${_name}")
      if(_version AND NOT _version MATCHES "^(NOASSERTION|unknown)$")
        string(APPEND _text " ${_version}")
      endif()
      string(APPEND _text "\n")
      if(_p MATCHES "\n +License: ([^\n]*)")
        string(APPEND _text "License: ${CMAKE_MATCH_1}\n")
      endif()
      string(FIND "${_p}" "PackageCopyrightText: <text>" _c)
      if(_c GREATER -1)
        math(EXPR _c "${_c} + 28")
        string(SUBSTRING "${_p}" ${_c} -1 _copyright)
        string(FIND "${_copyright}" "</text>" _end)
        string(SUBSTRING "${_copyright}" 0 ${_end} _copyright)
        string(APPEND _text "${_copyright}\n")
      endif()
    endforeach()
  endforeach()

  string(REPLACE "@score_semi@" ";" _text "${_text}")
  string(REPLACE "@score_lb@" "[" _text "${_text}")
  string(REPLACE "@score_rb@" "]" _text "${_text}")
  file(WRITE "${_out}" "${_text}")
  set(${_ok} TRUE PARENT_SCOPE)
endfunction()

function(_score_license_notice _file _out)
  file(READ "${_file}" _src LIMIT 131072)
  string(REPLACE "\r" "" _src "${_src}")
  set(_res "")
  while(TRUE)
    string(FIND "${_src}" "/*" _c)
    string(FIND "${_src}" "//" _l)
    if(_c EQUAL -1 AND _l EQUAL -1)
      break()
    endif()
    if(_l EQUAL -1 OR (NOT _c EQUAL -1 AND _c LESS _l))
      string(SUBSTRING "${_src}" ${_c} -1 _src)
      string(FIND "${_src}" "*/" _e)
      if(_e EQUAL -1)
        break()
      endif()
      math(EXPR _e "${_e} + 2")
      string(SUBSTRING "${_src}" 0 ${_e} _block)
      string(SUBSTRING "${_src}" ${_e} -1 _src)
    else()
      # A run of lines that all start with //
      string(SUBSTRING "${_src}" ${_l} -1 _src)
      set(_block "")
      while(_src MATCHES "^[ \t]*//")
        string(FIND "${_src}" "\n" _e)
        if(_e EQUAL -1)
          string(APPEND _block "${_src}")
          set(_src "")
          break()
        endif()
        math(EXPR _e "${_e} + 1")
        string(SUBSTRING "${_src}" 0 ${_e} _line)
        string(APPEND _block "${_line}")
        string(SUBSTRING "${_src}" ${_e} -1 _src)
      endwhile()
    endif()
    if(_block MATCHES "[Cc]opyright|COPYRIGHT|[Ll]icen[cs]e|LICEN[CS]E|[Pp]ermission")
      set(_res "${_block}")
      break()
    endif()
  endwhile()

  string(REGEX REPLACE "^/\\*+!?" "" _res "${_res}")
  string(REGEX REPLACE "\\*+/[ \t]*$" "" _res "${_res}")
  string(REGEX REPLACE "(^|\n)[ \t]*(//+|\\*+)[ \t]?" "\\1" _res "${_res}")
  # Boxed comments: trailing * at the end of each line
  string(REGEX REPLACE "[ \t]+\\*+[ \t]*(\n|$)" "\\1" _res "${_res}")
  string(STRIP "${_res}" _res)
  set(${_out} "${_res}" PARENT_SCOPE)
endfunction()

function(_score_c_string _in _out)
  string(REPLACE "\\" "\\\\" _s "${_in}")
  string(REPLACE "\"" "\\\"" _s "${_s}")
  string(REPLACE "\n" "\\n" _s "${_s}")
  set(${_out} "\"${_s}\"" PARENT_SCOPE)
endfunction()

# Writes the header read by Licenses.cpp, once every component is registered.
function(score_generate_licenses _header)
  get_property(_ids GLOBAL PROPERTY SCORE_LICENSE_IDS)
  set(_dir "${CMAKE_BINARY_DIR}/licenses")
  file(MAKE_DIRECTORY "${_dir}")

  set(_arrays "")
  set(_entries "")
  foreach(_id IN LISTS _ids)
    get_property(_name GLOBAL PROPERTY SCORE_LICENSE_${_id}_NAME)
    get_property(_url GLOBAL PROPERTY SCORE_LICENSE_${_id}_URL)
    get_property(_head GLOBAL PROPERTY SCORE_LICENSE_${_id}_HEADER)
    get_property(_files GLOBAL PROPERTY SCORE_LICENSE_${_id}_FILES)
    get_property(_notices GLOBAL PROPERTY SCORE_LICENSE_${_id}_NOTICES)

    set(_text "")
    foreach(_f IN LISTS _files)
      if(EXISTS "${_f}")
        file(READ "${_f}" _content)
        if(NOT _text STREQUAL "")
          string(APPEND _text "\n\n----------------------------------------\n\n")
        endif()
        string(APPEND _text "${_content}")
      endif()
    endforeach()
    foreach(_f IN LISTS _notices)
      if(EXISTS "${_f}")
        _score_license_notice("${_f}" _content)
        if(_content STREQUAL "")
          message(WARNING "License of ${_name}: no notice found in ${_f}")
          continue()
        endif()
        if(NOT _text STREQUAL "")
          string(APPEND _text "\n\n----------------------------------------\n\n")
        endif()
        string(APPEND _text "${_content}\n")
      endif()
    endforeach()

    if(_text STREQUAL "" AND _head STREQUAL "")
      message(WARNING "License of ${_name}: neither a license text nor a header")
      continue()
    endif()

    if(_text STREQUAL "")
      string(APPEND _arrays "\nstatic const unsigned char score_license_${_id}[] = { 0 };\n\nstatic const std::size_t score_license_${_id}_SIZE = 0;\n\n")
    else()
      file(WRITE "${_dir}/${_id}.txt" "${_text}")
      bin2h(
        SOURCE_FILE "${_dir}/${_id}.txt"
        HEADER_FILE _arrays
        VARIABLE_NAME "score_license_${_id}"
        APPEND
        NULL_TERMINATE)
    endif()

    _score_c_string("${_name}" _cname)
    _score_c_string("${_url}" _curl)
    _score_c_string("${_head}" _chead)
    string(APPEND _entries "  {${_cname}, ${_curl}, ${_chead}, score_license_${_id}, score_license_${_id}_SIZE},\n")
  endforeach()

  set(_content "#pragma once\n#include <cstddef>\n${_arrays}\n")
  string(APPEND _content "struct score_license_entry\n{\n  const char* name;\n  const char* url;\n  const char* header;\n  const unsigned char* text;\n  std::size_t text_size;\n};\n\n")
  string(APPEND _content "static const score_license_entry score_license_entries[] = {\n${_entries}};\n")
  set(_existing "")
  if(EXISTS "${_header}")
    file(READ "${_header}" _existing)
  endif()
  if(NOT _existing STREQUAL _content)
    file(WRITE "${_header}" "${_content}")
  endif()
endfunction()
