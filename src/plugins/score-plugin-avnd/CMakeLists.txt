cmake_minimum_required(VERSION 3.13 FATAL_ERROR)

project(score_plugin_avnd LANGUAGES CXX)

if("${CMAKE_CXX_COMPILER_ID}" MATCHES ".*Clang")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 14)
    return()
  endif()
elseif("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 12)
    return()
  endif()
endif()

# Check for coroutines support
include(CheckCXXSourceCompiles)
if("${CMAKE_CXX_COMPILER_ID}" MATCHES ".*Clang")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 14)
    set(CMAKE_REQUIRED_FLAGS "-std=c++20 -fcoroutines-ts")
  else()
    set(CMAKE_REQUIRED_FLAGS "-std=c++20")
  endif()
elseif("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
  set(CMAKE_REQUIRED_FLAGS "-std=c++20 -fcoroutines")
endif()

check_cxx_source_compiles("#include <coroutine>\nint main() {}" COROUTINES_WORKING)
if(NOT COROUTINES_WORKING)
  check_cxx_source_compiles("#include <experimental/coroutine>\nint main() {}" EXPERIMENTAL_COROUTINES_WORKING)
  if(NOT EXPERIMENTAL_COROUTINES_WORKING)
    message("score_plugin_avnd: coroutines not supported")
    set(AVND_DISABLE_COROUTINES 1)
  endif()
endif()

unset(CMAKE_REQUIRED_FLAGS)
include_directories("${3RDPARTY_FOLDER}/avendish/include")
add_library(
  score_plugin_avnd

  Crousti/Attributes.hpp
  Crousti/Concepts.hpp
  Crousti/GpuNode.hpp
  Crousti/Executor.hpp
  Crousti/Layer.hpp
  Crousti/Layer.cpp
  Crousti/Metadatas.hpp
  Crousti/ProcessModel.hpp

  score_plugin_avnd.hpp
  score_plugin_avnd.cpp
)

target_include_directories(score_plugin_avnd PUBLIC "${3RDPARTY_FOLDER}/avendish/include")
target_link_libraries(score_plugin_avnd PRIVATE score_plugin_engine)
if(TARGET score_plugin_gfx)
  target_link_libraries(score_plugin_avnd PRIVATE score_plugin_engine)
endif()

if(NOT AVND_DISABLE_COROUTINES)
  if("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
    target_compile_options(
        score_plugin_avnd
        PUBLIC
          -fcoroutines
    )
  elseif("${CMAKE_CXX_COMPILER_ID}" MATCHES ".*Clang")
    target_compile_options(
        score_plugin_avnd
        PUBLIC
          -fcoroutines-ts
          )
  endif()
else()
  target_compile_definitions(score_plugin_avnd PUBLIC AVND_DISABLE_COROUTINES)
endif()
setup_score_plugin(score_plugin_avnd)
