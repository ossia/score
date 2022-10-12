cmake_minimum_required(VERSION 3.13 FATAL_ERROR)

project(score_plugin_avnd LANGUAGES CXX)

if("${CMAKE_CXX_COMPILER_ID}" MATCHES ".*Clang")
  # Basically what does not work is clang 13 + libstdc++ due to coroutines
  if(NOT APPLE AND NOT WIN32 AND NOT EMSCRIPTEN AND NOT SCORE_DEPLOYMENT_BUILD)
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 14)
      return()
    endif()
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

set(AVND_FOLDER "${3RDPARTY_FOLDER}/avendish")
unset(CMAKE_REQUIRED_FLAGS)
include_directories("${AVND_FOLDER}/include")
add_library(
  score_plugin_avnd

  "${AVND_FOLDER}/examples/Advanced/Utilities/ADSR.hpp"
  "${AVND_FOLDER}/examples/Advanced/Utilities/AudioFilters.hpp"
  "${AVND_FOLDER}/examples/Advanced/Utilities/Bitcrush.hpp"
  "${AVND_FOLDER}/examples/Advanced/Utilities/Convolver.hpp"
  "${AVND_FOLDER}/examples/Advanced/Utilities/Dynamics.hpp"
  "${AVND_FOLDER}/examples/Advanced/Utilities/Echo.hpp"
  "${AVND_FOLDER}/examples/Advanced/Utilities/Flanger.hpp"


  Avnd/Factories.hpp

  Crousti/Attributes.hpp
  Crousti/Concepts.hpp
  Crousti/Executor.hpp
  Crousti/GfxNode.hpp
  Crousti/GpuNode.hpp
  Crousti/GpuComputeNode.hpp
  Crousti/GpuUtils.hpp
  Crousti/GpuUtils.cpp
  Crousti/Layer.hpp
  Crousti/Layer.cpp
  Crousti/Layout.hpp
  Crousti/Metadatas.hpp
  Crousti/Painter.hpp
  Crousti/ProcessModel.hpp
  Crousti/Widgets.hpp

  score_plugin_avnd.gradient_scrub.cpp

  score_plugin_avnd.graph.cpp

  score_plugin_avnd.hpp
  score_plugin_avnd.cpp
)

target_include_directories(score_plugin_avnd
  PUBLIC
    "${AVND_FOLDER}/include"
)
target_link_libraries(score_plugin_avnd
  PUBLIC
    score_plugin_engine
    score_plugin_media
    libremidi
    $<BUILD_INTERFACE:dspfilters>
    $<BUILD_INTERFACE:gamma>
  )
if(TARGET kfr_dft)
  target_link_libraries(score_plugin_avnd PRIVATE
    "$<BUILD_INTERFACE:kfr>"
    "$<BUILD_INTERFACE:kfr_dft>"
  )
endif()
if(TARGET score_plugin_gfx)
  target_link_libraries(score_plugin_avnd PUBLIC score_plugin_gfx)
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

target_precompile_headers(score_plugin_avnd PRIVATE
  Avnd/Factories.hpp

  Crousti/Attributes.hpp
  Crousti/Concepts.hpp
  Crousti/Executor.hpp
  Crousti/GfxNode.hpp
  Crousti/GpuNode.hpp
  Crousti/GpuComputeNode.hpp
  Crousti/GpuUtils.hpp
  Crousti/Layer.hpp
  Crousti/Layout.hpp
  Crousti/Metadatas.hpp
  Crousti/Painter.hpp
  Crousti/ProcessModel.hpp
  Crousti/Widgets.hpp
)


set(AVND_ADDITIONAL_CLASSES)
set(AVND_CUSTOM_FACTORIES)
function(avnd_make_score)

  cmake_parse_arguments(AVND "" "TARGET;MAIN_FILE;MAIN_CLASS;NAMESPACE" "" ${ARGN})

  if(AVND_NAMESPACE)
    set(AVND_QUALIFIED "${AVND_NAMESPACE}::${AVND_MAIN_CLASS}")
  else()
    set(AVND_QUALIFIED "${AVND_MAIN_CLASS}")
  endif()
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/prototype.cpp.in"
    "${CMAKE_BINARY_DIR}/${AVND_TARGET}_avnd.cpp"
    @ONLY
    NEWLINE_STYLE LF
  )

  target_sources(score_plugin_avnd PRIVATE
    "${AVND_MAIN_FILE}"
    "${CMAKE_BINARY_DIR}/${AVND_TARGET}_avnd.cpp"
  )

  if(AVND_NAMESPACE)
    set(txt "namespace ${AVND_NAMESPACE} { struct ${AVND_MAIN_CLASS}; } \n")
    set(txtf "::oscr::custom_factories<${AVND_NAMESPACE}::${AVND_MAIN_CLASS}>(fx, ctx, key); \n")
  else()
    set(txt "struct ${AVND_MAIN_CLASS}; \n")
    set(txtf "::oscr::custom_factories<${AVND_MAIN_CLASS}>(fx, ctx, key); \n")
  endif()

  set(AVND_ADDITIONAL_CLASSES "${AVND_ADDITIONAL_CLASSES}\n${txt}\n" PARENT_SCOPE)
  set(AVND_CUSTOM_FACTORIES "${AVND_CUSTOM_FACTORIES}\n${txtf}\n" PARENT_SCOPE)
endfunction()

avnd_make_score(
  MAIN_FILE "${AVND_FOLDER}/examples/Advanced/MidiScaler/MidiReader.hpp"
  TARGET midireader
  MAIN_CLASS MidiFileReader
  NAMESPACE mtk
)

avnd_make_score(
  MAIN_FILE "${AVND_FOLDER}/examples/Advanced/MidiScaler/MidiFilter.hpp"
  TARGET midifilter
  MAIN_CLASS MidiFilter
  NAMESPACE mtk
)

avnd_make_score(
  MAIN_FILE "${AVND_FOLDER}/examples/Advanced/MidiScaler/MidiScroller.hpp"
  TARGET midiscroller
  MAIN_CLASS MidiScroller
  NAMESPACE mtk
)

avnd_make_score(
  MAIN_FILE "${AVND_FOLDER}/examples/Advanced/MidiScaler/MidiScaler.hpp"
  TARGET midiscaler
  MAIN_CLASS MidiScaler
  NAMESPACE mtk
)

file(CONFIGURE OUTPUT
     "${CMAKE_BINARY_DIR}/include.avnd.cpp"
     CONTENT "${AVND_ADDITIONAL_CLASSES}\nvoid all_custom_factories(auto& fx, auto& ctx, auto& key) { ${AVND_CUSTOM_FACTORIES} }\n"
     NEWLINE_STYLE LF)

