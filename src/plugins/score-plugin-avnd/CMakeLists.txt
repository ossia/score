cmake_minimum_required(VERSION 3.13 FATAL_ERROR)

project(score_plugin_avnd LANGUAGES CXX)

if(SCORE_FAST_DEV_BUILD)
  return()
endif()

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
    set(COROUTINES_FLAGS " -fcoroutines-ts")
  else()
    set(CMAKE_REQUIRED_FLAGS "-std=c++20")
  endif()
elseif("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
  set(CMAKE_REQUIRED_FLAGS "-std=c++20 -fcoroutines")
  set(COROUTINES_FLAGS " -fcoroutines")
endif()


if(TARGET clang-cpp)
  add_executable(avnd_source_parser SourceParser/SourceParser.cpp)

  target_link_libraries(avnd_source_parser PRIVATE
    clang
    clang-cpp
    LLVM
    fmt
  )

  if(SCORE_PCH)
    target_precompile_headers(avnd_source_parser PRIVATE

       <clang/ASTMatchers/ASTMatchFinder.h>
       <clang/ASTMatchers/ASTMatchers.h>
       <clang/Frontend/FrontendActions.h>
       <clang/Tooling/CommonOptionsParser.h>
       <clang/Tooling/Tooling.h>
       <llvm/Support/CommandLine.h>

       <cstdio>
       <iostream>
    )
  endif()
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

if(CMAKE_CXX_SCAN_FOR_MODULES)
add_library(avnd_halp_module STATIC)
target_sources(avnd_halp_module
  PUBLIC
    FILE_SET CXX_MODULES
    FILES
      "halp.cppm"
)
target_link_libraries(avnd_halp_module PRIVATE
    score_plugin_engine
    score_plugin_media
    libremidi
    $<BUILD_INTERFACE:dspfilters>
    $<BUILD_INTERFACE:gamma>
)
if(TARGET kfr_dft)
  target_link_libraries(avnd_halp_module PRIVATE
    "$<BUILD_INTERFACE:kfr>"
  )
  link_whole_archive(avnd_halp_module PRIVATE kfr_dft)
  link_whole_archive(avnd_halp_module PRIVATE kfr_dsp)
endif()
if(TARGET score_plugin_gfx)
  target_link_libraries(avnd_halp_module PUBLIC score_plugin_gfx)
endif()
target_compile_features(avnd_halp_module PRIVATE cxx_std_20 INTERFACE cxx_std_20)
endif()

set(HALP_HEADERS
  "${AVND_FOLDER}/include/gpp/meta.hpp"
  "${AVND_FOLDER}/include/gpp/commands.hpp"
  "${AVND_FOLDER}/include/gpp/generators.hpp"
  "${AVND_FOLDER}/include/gpp/ports.hpp"
  "${AVND_FOLDER}/include/gpp/layout.hpp"

  "${AVND_FOLDER}/include/halp/compat/gamma.hpp"

  "${AVND_FOLDER}/include/halp/attributes.hpp"
  "${AVND_FOLDER}/include/halp/audio.hpp"
  "${AVND_FOLDER}/include/halp/callback.hpp"
  "${AVND_FOLDER}/include/halp/controls.hpp"
  "${AVND_FOLDER}/include/halp/controls.basic.hpp"
  "${AVND_FOLDER}/include/halp/controls.sliders.gcc10.hpp"
  "${AVND_FOLDER}/include/halp/controls.sliders.hpp"
  "${AVND_FOLDER}/include/halp/controls.typedefs.hpp"
  "${AVND_FOLDER}/include/halp/controls_fmt.hpp"
  "${AVND_FOLDER}/include/halp/curve.hpp"
  "${AVND_FOLDER}/include/halp/custom_widgets.hpp"
  "${AVND_FOLDER}/include/halp/device.hpp"
  "${AVND_FOLDER}/include/halp/dynamic_port.hpp"
  "${AVND_FOLDER}/include/halp/fft.hpp"
  "${AVND_FOLDER}/include/halp/file_port.hpp"
  "${AVND_FOLDER}/include/halp/geometry.hpp"
  "${AVND_FOLDER}/include/halp/gradient_port.hpp"
  "${AVND_FOLDER}/include/halp/inline.hpp"
  "${AVND_FOLDER}/include/halp/layout.hpp"
  "${AVND_FOLDER}/include/halp/log.hpp"
  "${AVND_FOLDER}/include/halp/mappers.hpp"
  "${AVND_FOLDER}/include/halp/messages.hpp"
  "${AVND_FOLDER}/include/halp/meta.hpp"
  "${AVND_FOLDER}/include/halp/midi.hpp"
  "${AVND_FOLDER}/include/halp/midifile_port.hpp"
  "${AVND_FOLDER}/include/halp/modules.hpp"
  "${AVND_FOLDER}/include/halp/polyfill.hpp"
  "${AVND_FOLDER}/include/halp/reactive_value.hpp"
  "${AVND_FOLDER}/include/halp/sample_accurate_controls.hpp"
  "${AVND_FOLDER}/include/halp/schedule.hpp"
  "${AVND_FOLDER}/include/halp/shared_instance.hpp"
  "${AVND_FOLDER}/include/halp/smooth_controls.hpp"
  "${AVND_FOLDER}/include/halp/smoothers.hpp"
  "${AVND_FOLDER}/include/halp/soundfile_port.hpp"
  "${AVND_FOLDER}/include/halp/static_string.hpp"
  "${AVND_FOLDER}/include/halp/texture.hpp"
  "${AVND_FOLDER}/include/halp/texture_formats.hpp"
  "${AVND_FOLDER}/include/halp/value_types.hpp"
)
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

  AvndDevices/AvndDevice.hpp
  Crousti/Attributes.hpp
  Crousti/Concepts.hpp
  Crousti/CpuAnalysisNode.hpp
  Crousti/CpuFilterNode.hpp
  Crousti/CpuGeneratorNode.hpp
  Crousti/Executor.hpp
  Crousti/ExecutorPortSetup.hpp
  Crousti/ExecutorUpdateControlValueInUi.hpp
  Crousti/Geometry.hpp
  Crousti/GfxNode.hpp
  Crousti/GpuComputeNode.hpp
  Crousti/GpuNode.hpp
  Crousti/GpuUtils.cpp
  Crousti/GpuUtils.hpp
  Crousti/Layer.hpp
  Crousti/MessageBus.hpp
  Crousti/Metadata.hpp
  Crousti/Metadatas.hpp
  Crousti/Painter.hpp
  Crousti/ProcessModel.hpp
  Crousti/ProcessModelPortInit.hpp
  Crousti/ScoreLayer.hpp

# score_plugin_avnd.graph.cpp
  score_plugin_avnd.aether.cpp
  score_plugin_avnd.convolver.cpp
  score_plugin_avnd.filters.cpp
  score_plugin_avnd.tests.cpp
  score_plugin_avnd.tests2.cpp

  score_plugin_avnd.hpp
  score_plugin_avnd.cpp


  "${AVND_FOLDER}/include/avnd/binding/ossia/all.hpp"
  "${AVND_FOLDER}/include/avnd/binding/ossia/builtin_ports.hpp"
  "${AVND_FOLDER}/include/avnd/binding/ossia/callbacks.hpp"
  "${AVND_FOLDER}/include/avnd/binding/ossia/configure.hpp"
  "${AVND_FOLDER}/include/avnd/binding/ossia/data_node.hpp"
  "${AVND_FOLDER}/include/avnd/binding/ossia/dynamic_ports.hpp"
  "${AVND_FOLDER}/include/avnd/binding/ossia/ffts.hpp"
  "${AVND_FOLDER}/include/avnd/binding/ossia/from_value.hpp"
  "${AVND_FOLDER}/include/avnd/binding/ossia/geometry.hpp"
  "${AVND_FOLDER}/include/avnd/binding/ossia/message_process.hpp"
  "${AVND_FOLDER}/include/avnd/binding/ossia/mono_audio_node.hpp"
  "${AVND_FOLDER}/include/avnd/binding/ossia/node.hpp"
  "${AVND_FOLDER}/include/avnd/binding/ossia/ossia_audio_node.hpp"
  "${AVND_FOLDER}/include/avnd/binding/ossia/ossia_to_curve.hpp"
  "${AVND_FOLDER}/include/avnd/binding/ossia/poly_audio_node.hpp"
  "${AVND_FOLDER}/include/avnd/binding/ossia/port_base.hpp"
  "${AVND_FOLDER}/include/avnd/binding/ossia/port_run_postprocess.hpp"
  "${AVND_FOLDER}/include/avnd/binding/ossia/port_run_preprocess.hpp"
  "${AVND_FOLDER}/include/avnd/binding/ossia/port_setup.hpp"
  "${AVND_FOLDER}/include/avnd/binding/ossia/qt.hpp"
  "${AVND_FOLDER}/include/avnd/binding/ossia/soundfiles.hpp"
  "${AVND_FOLDER}/include/avnd/binding/ossia/time_controls.hpp"
  "${AVND_FOLDER}/include/avnd/binding/ossia/to_value.hpp"
  "${AVND_FOLDER}/include/avnd/binding/ossia/uuid.hpp"
  "${AVND_FOLDER}/include/avnd/binding/ossia/value.hpp"
  "${AVND_FOLDER}/include/avnd/binding/ossia/window_functions.hpp"
  "${AVND_FOLDER}/include/avnd/common/export.hpp"
  "${AVND_FOLDER}/include/avnd/common/span_polyfill.hpp"
  "${AVND_FOLDER}/include/avnd/common/dummy.hpp"
  "${AVND_FOLDER}/include/avnd/common/index_sequence.hpp"
  "${AVND_FOLDER}/include/avnd/common/concepts_polyfill.hpp"
  "${AVND_FOLDER}/include/avnd/common/errors.hpp"
  "${AVND_FOLDER}/include/avnd/common/function_reflection.hpp"
  "${AVND_FOLDER}/include/avnd/common/limited_string.hpp"
  "${AVND_FOLDER}/include/avnd/common/limited_string_view.hpp"
  "${AVND_FOLDER}/include/avnd/common/member_reflection.hpp"
  "${AVND_FOLDER}/include/avnd/common/widechar.hpp"
  "${AVND_FOLDER}/include/avnd/common/enums.hpp"
  "${AVND_FOLDER}/include/avnd/common/enum_reflection.hpp"
  "${AVND_FOLDER}/include/avnd/common/struct_reflection.hpp"
  "${AVND_FOLDER}/include/avnd/common/aggregates.hpp"
  "${AVND_FOLDER}/include/avnd/common/array.hpp"
  "${AVND_FOLDER}/include/avnd/common/coroutines.hpp"
  "${AVND_FOLDER}/include/avnd/common/index.hpp"
  "${AVND_FOLDER}/include/avnd/common/optionals.hpp"
  "${AVND_FOLDER}/include/avnd/common/for_nth.hpp"
  "${AVND_FOLDER}/include/avnd/concepts/all.hpp"
  "${AVND_FOLDER}/include/avnd/concepts/curve.hpp"
  "${AVND_FOLDER}/include/avnd/concepts/fft.hpp"
  "${AVND_FOLDER}/include/avnd/concepts/gfx.hpp"
  "${AVND_FOLDER}/include/avnd/concepts/logger.hpp"
  "${AVND_FOLDER}/include/avnd/concepts/message_bus.hpp"
  "${AVND_FOLDER}/include/avnd/concepts/painter.hpp"
  "${AVND_FOLDER}/include/avnd/concepts/port.hpp"
  "${AVND_FOLDER}/include/avnd/concepts/synth.hpp"
  "${AVND_FOLDER}/include/avnd/concepts/ui.hpp"
  "${AVND_FOLDER}/include/avnd/concepts/modules.hpp"
  "${AVND_FOLDER}/include/avnd/concepts/audio_processor.hpp"
  "${AVND_FOLDER}/include/avnd/concepts/callback.hpp"
  "${AVND_FOLDER}/include/avnd/concepts/channels.hpp"
  "${AVND_FOLDER}/include/avnd/concepts/midi.hpp"
  "${AVND_FOLDER}/include/avnd/concepts/midi_port.hpp"
  "${AVND_FOLDER}/include/avnd/concepts/object.hpp"
  "${AVND_FOLDER}/include/avnd/concepts/generic.hpp"
  "${AVND_FOLDER}/include/avnd/concepts/processor.hpp"
  "${AVND_FOLDER}/include/avnd/concepts/audio_port.hpp"
  "${AVND_FOLDER}/include/avnd/concepts/layout.hpp"
  "${AVND_FOLDER}/include/avnd/concepts/message.hpp"
  "${AVND_FOLDER}/include/avnd/concepts/midifile.hpp"
  "${AVND_FOLDER}/include/avnd/concepts/parameter.hpp"
  "${AVND_FOLDER}/include/avnd/concepts/soundfile.hpp"
  "${AVND_FOLDER}/include/avnd/concepts/file_port.hpp"
  "${AVND_FOLDER}/include/avnd/concepts/worker.hpp"
  "${AVND_FOLDER}/include/avnd/wrappers/avnd.hpp"
  "${AVND_FOLDER}/include/avnd/wrappers/soundfile_storage.hpp"
  "${AVND_FOLDER}/include/avnd/wrappers/process/base.hpp"
  "${AVND_FOLDER}/include/avnd/wrappers/process/per_channel_arg.hpp"
  "${AVND_FOLDER}/include/avnd/wrappers/process/per_channel_port.hpp"
  "${AVND_FOLDER}/include/avnd/wrappers/process/per_sample_arg.hpp"
  "${AVND_FOLDER}/include/avnd/wrappers/process/poly_arg.hpp"
  "${AVND_FOLDER}/include/avnd/wrappers/process/poly_port.hpp"
  "${AVND_FOLDER}/include/avnd/wrappers/process/per_sample_port.hpp"
  "${AVND_FOLDER}/include/avnd/wrappers/process_adapter.hpp"
  "${AVND_FOLDER}/include/avnd/wrappers/controls_double.hpp"
  "${AVND_FOLDER}/include/avnd/wrappers/ranges.hpp"
  "${AVND_FOLDER}/include/avnd/wrappers/controls.hpp"
  "${AVND_FOLDER}/include/avnd/wrappers/bus_host_process_adapter.hpp"
  "${AVND_FOLDER}/include/avnd/wrappers/configure.hpp"
  "${AVND_FOLDER}/include/avnd/wrappers/control_display.hpp"
  "${AVND_FOLDER}/include/avnd/wrappers/controls_fp.hpp"
  "${AVND_FOLDER}/include/avnd/wrappers/controls_storage.hpp"
  "${AVND_FOLDER}/include/avnd/wrappers/effect_container.hpp"
  "${AVND_FOLDER}/include/avnd/wrappers/prepare.hpp"
  "${AVND_FOLDER}/include/avnd/wrappers/widgets.hpp"
  "${AVND_FOLDER}/include/avnd/wrappers/audio_channel_manager.hpp"
  "${AVND_FOLDER}/include/avnd/wrappers/callbacks_adapter.hpp"
  "${AVND_FOLDER}/include/avnd/wrappers/metadatas.hpp"
  "${AVND_FOLDER}/include/avnd/wrappers/process_execution.hpp"
  "${AVND_FOLDER}/include/avnd/introspection/layout.hpp"
  "${AVND_FOLDER}/include/avnd/introspection/channels.hpp"
  "${AVND_FOLDER}/include/avnd/introspection/gfx.hpp"
  "${AVND_FOLDER}/include/avnd/introspection/input.hpp"
  "${AVND_FOLDER}/include/avnd/introspection/midi.hpp"
  "${AVND_FOLDER}/include/avnd/introspection/modules.hpp"
  "${AVND_FOLDER}/include/avnd/introspection/output.hpp"
  "${AVND_FOLDER}/include/avnd/introspection/port.hpp"
  "${AVND_FOLDER}/include/avnd/introspection/widgets.hpp"
  "${AVND_FOLDER}/include/avnd/introspection/messages.hpp"
  "${AVND_FOLDER}/include/avnd/prefix.hpp"
  ${HALP_HEADERS}
)

target_include_directories(score_plugin_avnd
  PUBLIC
    "${AVND_FOLDER}/include"
  PRIVATE
    "$<BUILD_INTERFACE:${3RDPARTY_FOLDER}/csv2/include>"
)
target_link_libraries(score_plugin_avnd
  PUBLIC
    score_plugin_engine
    score_plugin_media
    libremidi
    $<BUILD_INTERFACE:dspfilters>
    $<BUILD_INTERFACE:gamma>
    $<$<BOOL:${CMAKE_CXX_SCAN_FOR_MODULES}>:$<BUILD_INTERFACE:avnd_halp_module>>

  PRIVATE
    $<BUILD_INTERFACE:rubberband>
)
if(TARGET kfr_dft)
  target_link_libraries(score_plugin_avnd PRIVATE
    "$<BUILD_INTERFACE:kfr>"
  )
  link_whole_archive(score_plugin_avnd PRIVATE kfr_dft)
  link_whole_archive(score_plugin_avnd PRIVATE kfr_dsp)
endif()
if(TARGET score_plugin_gfx)
  target_link_libraries(score_plugin_avnd PUBLIC score_plugin_gfx)
endif()

if(NOT AVND_DISABLE_COROUTINES)
  if(COROUTINES_FLAG)
    target_compile_options(
      score_plugin_avnd
      PUBLIC
        ${COROUTINES_FLAG}
    )
  endif()
else()
  target_compile_definitions(score_plugin_avnd PUBLIC AVND_DISABLE_COROUTINES)
endif()

setup_score_plugin(score_plugin_avnd)

if(SCORE_PCH)
if(NOT TARGET avnd_source_parser)
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
  Crousti/Metadatas.hpp
  Crousti/Metadata.hpp
  Crousti/Painter.hpp
  Crousti/ProcessModel.hpp
  "${AVND_FOLDER}/include/avnd/prefix.hpp"
  ${HALP_HEADERS}
)
endif()
endif()

set(AVND_ADDITIONAL_CLASSES)
set(AVND_CUSTOM_FACTORIES)
function(avnd_make_score)

  cmake_parse_arguments(AVND "DEVICE" "TARGET;MAIN_CLASS;NAMESPACE" "SOURCES" ${ARGN})

  if(AVND_NAMESPACE)
    set(AVND_QUALIFIED "${AVND_NAMESPACE}::${AVND_MAIN_CLASS}")
  else()
    set(AVND_QUALIFIED "${AVND_MAIN_CLASS}")
  endif()

  list(GET AVND_SOURCES 0 AVND_MAIN_FILE)

  if(TARGET avnd_source_parser)
    set(AVND_REFLECTION_HELPERS_PRE "${CMAKE_BINARY_DIR}/${AVND_TARGET}_avnd.refl.pre.hpp")
    set(AVND_REFLECTION_HELPERS "${CMAKE_BINARY_DIR}/${AVND_TARGET}_avnd.refl.hpp")
    add_custom_command(
      OUTPUT "${AVND_REFLECTION_HELPERS}"
      COMMAND avnd_source_parser "${AVND_QUALIFIED}" "${AVND_MAIN_FILE}" "${AVND_REFLECTION_HELPERS}"
      DEPENDS avnd_source_parser
    )
  endif()

  if(AVND_DEVICE)
    set(source_proto "${CMAKE_CURRENT_SOURCE_DIR}/device-prototype.cpp.in")
  else()
    set(source_proto "${CMAKE_CURRENT_SOURCE_DIR}/prototype.cpp.in")
  endif()
  configure_file(
    "${source_proto}"
    "${CMAKE_BINARY_DIR}/${AVND_TARGET}_avnd.cpp"
    @ONLY
    NEWLINE_STYLE LF
  )

  target_sources(score_plugin_avnd PRIVATE
    ${AVND_SOURCES}
    "${CMAKE_BINARY_DIR}/${AVND_TARGET}_avnd.cpp"
  )

  if(TARGET avnd_source_parser)
    target_sources(score_plugin_avnd PRIVATE
      "${AVND_REFLECTION_HELPERS}"
    )
    target_compile_definitions(score_plugin_avnd PUBLIC AVND_USE_TUPLET_TUPLE=1)
  endif()

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
  SOURCES "${AVND_FOLDER}/examples/Advanced/AI/PromptComposer.hpp"
  TARGET prompt_composer
  MAIN_CLASS PromptComposer
  NAMESPACE ai
)
if(1)
avnd_make_score(
  SOURCES "${AVND_FOLDER}/examples/Advanced/Utilities/ADSR.hpp"
  TARGET adsr
  MAIN_CLASS ADSR
  NAMESPACE ao
)
avnd_make_score(
  SOURCES "${AVND_FOLDER}/examples/Advanced/Utilities/Bitcrush.hpp"
  TARGET bitcrush
  MAIN_CLASS Bitcrush
  NAMESPACE ao
)
avnd_make_score(
  SOURCES "${AVND_FOLDER}/examples/Advanced/Utilities/Dynamics.hpp"
  TARGET compressor
  MAIN_CLASS Compressor
  NAMESPACE ao
)
avnd_make_score(
  SOURCES "${AVND_FOLDER}/examples/Advanced/Utilities/Dynamics.hpp"
  TARGET limiter
  MAIN_CLASS Limiter
  NAMESPACE ao
)
avnd_make_score(
  SOURCES "${AVND_FOLDER}/examples/Advanced/Utilities/Echo.hpp"
  TARGET echo
  MAIN_CLASS Echo
  NAMESPACE ao
)
avnd_make_score(
  SOURCES "${AVND_FOLDER}/examples/Advanced/Utilities/Flanger.hpp"
  TARGET flanger
  MAIN_CLASS Flanger
  NAMESPACE ao
)
avnd_make_score(
  SOURCES "${AVND_FOLDER}/examples/Advanced/Utilities/Gain.hpp"
  TARGET gain
  MAIN_CLASS Gain
  NAMESPACE ao
)
avnd_make_score(
  SOURCES "${AVND_FOLDER}/examples/Advanced/Utilities/StereoMixer.hpp"
  TARGET stereo_mixer
  MAIN_CLASS StereoMixer
  NAMESPACE ao
)


avnd_make_score(
  SOURCES "${AVND_FOLDER}/examples/Advanced/MidiScaler/MidiReader.hpp"
  TARGET midireader
  MAIN_CLASS MidiFileReader
  NAMESPACE mtk
)

avnd_make_score(
  SOURCES "${AVND_FOLDER}/examples/Advanced/MidiScaler/MidiFilter.hpp"
  TARGET midifilter
  MAIN_CLASS MidiFilter
  NAMESPACE mtk
)

avnd_make_score(
  SOURCES "${AVND_FOLDER}/examples/Advanced/MidiScaler/MidiScroller.hpp"
  TARGET midiscroller
  MAIN_CLASS MidiScroller
  NAMESPACE mtk
)

avnd_make_score(
  SOURCES "${AVND_FOLDER}/examples/Advanced/MidiScaler/MidiScaler.hpp"
  TARGET midiscaler
  MAIN_CLASS MidiScaler
  NAMESPACE mtk
)

avnd_make_score(
  SOURCES "${AVND_FOLDER}/examples/Midi/MidiGlide.hpp"
  TARGET midi_glide
  MAIN_CLASS MidiGlide
  NAMESPACE mo
)
avnd_make_score(
  SOURCES "${AVND_FOLDER}/examples/Ports/Essentia/stats/Entropy.hpp"
  TARGET essentia_entropy
  MAIN_CLASS Entropy
  NAMESPACE essentia_ports
)

avnd_make_score(
  SOURCES "${AVND_FOLDER}/examples/Advanced/Patternal/Patternal.hpp"
  TARGET patternal
  MAIN_CLASS Processor
  NAMESPACE patternal
)

avnd_make_score(
  SOURCES "${AVND_FOLDER}/examples/Advanced/Patternal/Melodial.hpp"
  TARGET melodial
  MAIN_CLASS Processor
  NAMESPACE melodial
)

avnd_make_score(
  SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/AvndProcesses/AddressTools.hpp"
  TARGET pattern_unfolder
  MAIN_CLASS PatternUnfolder
  NAMESPACE avnd_tools
)

avnd_make_score(
  SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/AvndProcesses/Combiner.hpp"
  TARGET pattern_combiner
  MAIN_CLASS PatternCombiner
  NAMESPACE avnd_tools
)

avnd_make_score(
  SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/AvndProcesses/AudioRecorder.hpp"
  TARGET audio_recorder
  MAIN_CLASS AudioRecorder

  NAMESPACE avnd_tools
)

avnd_make_score(
  SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/AvndProcesses/DeviceRecorder.hpp"
  TARGET device_recorder
  MAIN_CLASS DeviceRecorder
  NAMESPACE avnd_tools
)

avnd_make_score(
  SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/AvndProcesses/Sweeper.hpp"
  TARGET pattern_sweeper
  MAIN_CLASS PatternSweeper
  NAMESPACE avnd_tools
)

avnd_make_score(
  SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/AvndProcesses/Spammer.hpp"
  TARGET pattern_spammer
  MAIN_CLASS Spammer
  NAMESPACE avnd_tools
)

avnd_make_score(
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/AvndProcesses/MIDISync.hpp"
    TARGET avnd_midi_sync
    MAIN_CLASS MidiSync
    NAMESPACE mtk
)

avnd_make_score(
  SOURCES "${AVND_FOLDER}/examples/Helpers/GradientScrubber.hpp"
  TARGET gradient_scrub
  MAIN_CLASS GradientScrub
  NAMESPACE examples::helpers
)

avnd_make_score(
  SOURCES "${AVND_FOLDER}/examples/Advanced/Utilities/Trigger.hpp"
  TARGET trigger
  MAIN_CLASS Trigger
  NAMESPACE examples
)
avnd_make_score(
  SOURCES "${AVND_FOLDER}/examples/Advanced/Utilities/Combine.hpp"
  TARGET combine
  MAIN_CLASS Combine
  NAMESPACE ao
)
avnd_make_score(
  SOURCES "${AVND_FOLDER}/examples/Advanced/Utilities/Spread.hpp"
  TARGET spread
  MAIN_CLASS Spread
  NAMESPACE ao
)
avnd_make_score(
  SOURCES "${AVND_FOLDER}/examples/Raw/Interpolator.hpp"
  TARGET exponential_smoothing
  MAIN_CLASS ExponentialSmoothing
  NAMESPACE examples
)

avnd_make_score(
  SOURCES "${AVND_FOLDER}/examples/Advanced/Utilities/MapTool.hpp"
  TARGET map_tool
  MAIN_CLASS MapTool
  NAMESPACE ao
)


avnd_make_score(
  SOURCES "${AVND_FOLDER}/examples/Advanced/Utilities/ArrayTool.hpp"
  TARGET array_tool
  MAIN_CLASS ArrayTool
  NAMESPACE ao
)
avnd_make_score(
  SOURCES "${AVND_FOLDER}/examples/Advanced/Utilities/ArrayCombiner.hpp"
  TARGET array_combiner
  MAIN_CLASS ArrayCombiner
  NAMESPACE ao
)

avnd_make_score(
  SOURCES "${AVND_FOLDER}/examples/Advanced/Utilities/ArrayFlattener.hpp"
  TARGET array_flattener
  MAIN_CLASS ArrayFlattener
  NAMESPACE ao
)

avnd_make_score(
  SOURCES "${AVND_FOLDER}/examples/Advanced/Utilities/ArrayRecombiner.hpp"
  TARGET array_recombiner
  MAIN_CLASS ArrayRecombiner
  NAMESPACE ao
)


avnd_make_score(
  SOURCES "${AVND_FOLDER}/examples/Advanced/Utilities/Tweener.hpp"
  TARGET tweener
  MAIN_CLASS Tweener
  NAMESPACE ao
)

avnd_make_score(
  SOURCES "${AVND_FOLDER}/examples/Advanced/Utilities/Calibrator.hpp"
  TARGET calibrator
  MAIN_CLASS Calibrator
  NAMESPACE ao
)

avnd_make_score(
  SOURCES "${AVND_FOLDER}/examples/Advanced/Utilities/RangeFilter.hpp"
  TARGET range_filter
  MAIN_CLASS RangeFilter
  NAMESPACE ao
)

avnd_make_score(
  SOURCES "${AVND_FOLDER}/examples/Advanced/Utilities/AudioSum.hpp"
  TARGET audio_sum
  MAIN_CLASS AudioSum
  NAMESPACE ao
)

avnd_make_score(
  SOURCES "${AVND_FOLDER}/examples/Advanced/Utilities/MonoMix.hpp"
  TARGET mono_mix_8
  MAIN_CLASS MonoMix
  NAMESPACE ao
)

avnd_make_score(
  SOURCES "${AVND_FOLDER}/examples/Advanced/Utilities/QuadPan.hpp"
  TARGET quad_pan
  MAIN_CLASS QuadPan
  NAMESPACE ao
)

avnd_make_score(
  SOURCES "${AVND_FOLDER}/examples/Advanced/Utilities/Silence.hpp"
  TARGET silence
  MAIN_CLASS Silence
  NAMESPACE ao
)

avnd_make_score(
  SOURCES "${AVND_FOLDER}/examples/Advanced/Utilities/LightnessSampler.hpp"
  TARGET lightness_sampler
  MAIN_CLASS LightnessSampler
  NAMESPACE vo
)

avnd_make_score(
  SOURCES "${AVND_FOLDER}/examples/Advanced/Utilities/LightnessComputer.hpp"
  TARGET lightness_computer
  MAIN_CLASS LightnessComputer
  NAMESPACE vo
)

avnd_make_score(
  SOURCES "${AVND_FOLDER}/examples/Gpu/SolidColor.hpp"
  TARGET gpu_solid_color
  MAIN_CLASS GpuSolidColorExample
  NAMESPACE examples
)

avnd_make_score(
  SOURCES "${AVND_FOLDER}/examples/Gpu/Compute.hpp"
  TARGET gpu_compute
  MAIN_CLASS GpuComputeExample
  NAMESPACE examples
)

avnd_make_score(
  SOURCES "${AVND_FOLDER}/examples/Raw/ProcessLauncher.hpp"
  TARGET avnd_process
  MAIN_CLASS ProcessLauncher
  NAMESPACE examples
)

avnd_make_score(
  SOURCES "${AVND_FOLDER}/examples/Raw/Shell.hpp"
  TARGET avnd_shell
  MAIN_CLASS Shell
  NAMESPACE examples
)

avnd_make_score(
  SOURCES "${AVND_FOLDER}/examples/Helpers/ValueDelay.hpp"
  TARGET avnd_value_delay
  MAIN_CLASS ValueDelay
  NAMESPACE examples::helpers
)

avnd_make_score(
  SOURCES "${AVND_FOLDER}/examples/Helpers/Poles.hpp"
  TARGET avnd_poles
  MAIN_CLASS Poles
  NAMESPACE examples::helpers
)

avnd_make_score(
  SOURCES
    "${AVND_FOLDER}/examples/Advanced/AudioParticles/AudioParticles.hpp"
    "${AVND_FOLDER}/examples/Advanced/AudioParticles/AudioParticles.cpp"
  TARGET avnd_audio_particles
  MAIN_CLASS AudioParticles
  NAMESPACE ao
)
avnd_make_score(
  SOURCES "${AVND_FOLDER}/examples/Helpers/AudioChannelSelector.hpp"
  TARGET avnd_channel_selector
  MAIN_CLASS AudioChannelSelector
  NAMESPACE examples::helpers
)
avnd_make_score(
  SOURCES
    "${AVND_FOLDER}/examples/Advanced/GeoZones/GeoZones.hpp"
    "${AVND_FOLDER}/examples/Advanced/GeoZones/GeoZones.cpp"
  TARGET geozones
  MAIN_CLASS GeoZones
  NAMESPACE co
)

avnd_make_score(
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/AvndProcesses/Teleplot.hpp"
  TARGET teleplot
  MAIN_CLASS Teleplot
  NAMESPACE avnd_tools
)

avnd_make_score(
  SOURCES "${AVND_FOLDER}/examples/Helpers/SpriteReader.hpp"
  TARGET sprite_reader
  MAIN_CLASS SpriteReader
  NAMESPACE examples::helpers
)

avnd_make_score(
  SOURCES "${AVND_FOLDER}/examples/Advanced/UI/LEDView.hpp"
  TARGET led_view
  MAIN_CLASS LEDView
  NAMESPACE ao
)

avnd_make_score(
  SOURCES "${AVND_FOLDER}/examples/Advanced/UI/2DView.hpp"
  TARGET point2d_view
  MAIN_CLASS Point2DView
  NAMESPACE ao
)

avnd_make_score(
  SOURCES "${AVND_FOLDER}/examples/Advanced/Spatialization/DBAP.hpp"
  TARGET avnd_dbap_2d
  MAIN_CLASS DBAP_2d
  NAMESPACE ao
)
avnd_make_score(
    SOURCES "${AVND_FOLDER}/examples/Advanced/Spatialization/DBAP.hpp"
    TARGET avnd_dbap_3d
    MAIN_CLASS DBAP_3d
    NAMESPACE ao
)

# avnd_make_score(
#   SOURCES
#     "${AVND_FOLDER}/examples/Advanced/Kabang/Kabang.hpp"
#     "${AVND_FOLDER}/examples/Advanced/Kabang/Kabang.cpp"
#     "${AVND_FOLDER}/examples/Advanced/Kabang/Sampler.hpp"
#     "${AVND_FOLDER}/examples/Advanced/Kabang/Sampler.cpp"
#   TARGET kabang
#   MAIN_CLASS Kabang
#   NAMESPACE kbng
# )

file(CONFIGURE OUTPUT
     "${CMAKE_BINARY_DIR}/include.avnd.cpp"
     CONTENT "${AVND_ADDITIONAL_CLASSES}\nstatic void all_custom_factories(auto& fx, auto& ctx, auto& key) { ${AVND_CUSTOM_FACTORIES} }\n"
     NEWLINE_STYLE LF)


### Tests ###
if(TARGET Catch2::Catch2WithMain)
function(addAvndTest TESTNAME TESTSRCS)
    add_executable(Avnd_${TESTNAME} ${TESTSRCS})
    setup_score_common_test_features(Avnd_${TESTNAME})

    target_include_directories(Avnd_${TESTNAME}
      PUBLIC
        "${AVND_FOLDER}/include"
    )
    target_link_libraries(Avnd_${TESTNAME} PRIVATE ossia Catch2::Catch2WithMain score_plugin_avnd)
    add_test(Avnd_${TESTNAME}_target Avnd_${TESTNAME})
endFunction()

# Commands
addAvndTest(ossia_value_Test
             "${CMAKE_CURRENT_SOURCE_DIR}/Tests/from_ossia_value_Test.cpp")
endif()

set(CMAKE_AUTOMOC OFF)

if(SCORE_INSTALL_HEADERS)
  install(
    FILES
      prototype.cpp.in
      plugin_prototype.hpp.in
      plugin_prototype.cpp.in
    DESTINATION lib/cmake/score
    COMPONENT Devel
    OPTIONAL
  )
endif()
