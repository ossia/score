# Which add-on targets _score_avnd_apply_shared_pch lets reuse the shared
# avendish PCH. Configured on its own by test_cmake_avnd_shared_pch: the
# configure fails when a target gets the PCH it must not, or the reverse.
cmake_minimum_required(VERSION 3.25)
project(score_avnd_pch_test LANGUAGES CXX)

include("${CMAKE_CURRENT_LIST_DIR}/../../ScoreAvndHelper.cmake")

file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/empty.cpp" CONTENT "")
file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/pch.hpp" CONTENT "#pragma once\n")
set(_src "${CMAKE_CURRENT_BINARY_DIR}/empty.cpp")

add_library(score_plugin_avnd_pch STATIC "${_src}")
target_precompile_headers(score_plugin_avnd_pch PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/pch.hpp")
target_compile_definitions(score_plugin_avnd_pch PUBLIC AVND_SHARED=1)

add_library(dep_defines_halp INTERFACE)
target_compile_options(dep_defines_halp INTERFACE -DHALP_TEST=1)
add_library(dep_fast_math INTERFACE)
target_compile_options(dep_fast_math INTERFACE "$<$<CXX_COMPILER_ID:Clang,GNU>:-ffast-math>")
add_library(dep_harmless INTERFACE)
target_compile_options(dep_harmless INTERFACE -Wall -DUNRELATED=1)

set(_expect_reuse)
set(_expect_own)
macro(consumer _name _reuses)
  add_library(${_name} STATIC "${_src}")
  set_property(GLOBAL APPEND PROPERTY SCORE_AVND_PCH_CONSUMERS ${_name})
  if(${_reuses})
    list(APPEND _expect_reuse ${_name})
  else()
    list(APPEND _expect_own ${_name})
  endif()
endmacro()

consumer(same_definitions ON)
target_compile_definitions(same_definitions PRIVATE AVND_SHARED=1 UNRELATED)

consumer(harmless_dependency ON)
target_link_libraries(harmless_dependency PRIVATE dep_harmless)

consumer(own_macro OFF)
target_compile_definitions(own_macro PRIVATE OSSIA_TEST=1)

consumer(dependency_option_macro OFF)
target_link_libraries(dependency_option_macro PRIVATE dep_defines_halp)

consumer(dependency_fast_math OFF)
target_link_libraries(dependency_fast_math PRIVATE "$<LINK_ONLY:dep_fast_math>")

consumer(no_rtti_in_debug OFF)
target_compile_options(no_rtti_in_debug PRIVATE "$<$<CONFIG:Debug>:-fno-rtti>")

consumer(target_features OFF)
target_compile_options(target_features PRIVATE -mavx2)

consumer(other_standard OFF)
set_target_properties(other_standard PROPERTIES CXX_STANDARD 17)

add_subdirectory(directory_definitions)
add_subdirectory(directory_options)
list(APPEND _expect_own directory_definitions directory_options)

_score_avnd_apply_shared_pch()

set(_wrong)
foreach(_t IN LISTS _expect_reuse _expect_own)
  get_target_property(_from ${_t} PRECOMPILE_HEADERS_REUSE_FROM)
  if(_t IN_LIST _expect_reuse AND NOT _from STREQUAL "score_plugin_avnd_pch")
    list(APPEND _wrong "${_t} does not reuse the shared PCH")
  elseif(_t IN_LIST _expect_own AND _from)
    list(APPEND _wrong "${_t} reuses the shared PCH")
  endif()
endforeach()
if(_wrong)
  list(JOIN _wrong "\n" _wrong)
  message(FATAL_ERROR "${_wrong}")
endif()
message(STATUS "shared avendish PCH: every target as expected")
