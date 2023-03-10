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

#  score_plugin_avnd.graph.cpp

  score_plugin_avnd.hpp
  score_plugin_avnd.cpp


  "${AVND_FOLDER}/include/avnd/binding/clap/all.hpp"
  "${AVND_FOLDER}/include/avnd/binding/clap/clap.hpp"
  "${AVND_FOLDER}/include/avnd/binding/clap/helpers.hpp"
  "${AVND_FOLDER}/include/avnd/binding/clap/audio_effect.hpp"
  "${AVND_FOLDER}/include/avnd/binding/clap/bus_info.hpp"
  "${AVND_FOLDER}/include/avnd/binding/clap/configure.hpp"
  "${AVND_FOLDER}/include/avnd/binding/example/all.hpp"
  "${AVND_FOLDER}/include/avnd/binding/example/example_processor.hpp"
  "${AVND_FOLDER}/include/avnd/binding/max/all.hpp"
  "${AVND_FOLDER}/include/avnd/binding/max/dsp.hpp"
  "${AVND_FOLDER}/include/avnd/binding/max/atom_iterator.hpp"
  "${AVND_FOLDER}/include/avnd/binding/max/configure.hpp"
  "${AVND_FOLDER}/include/avnd/binding/max/helpers.hpp"
  "${AVND_FOLDER}/include/avnd/binding/max/inputs.hpp"
  "${AVND_FOLDER}/include/avnd/binding/max/message_processor.hpp"
  "${AVND_FOLDER}/include/avnd/binding/max/messages.hpp"
  "${AVND_FOLDER}/include/avnd/binding/max/outputs.hpp"
  "${AVND_FOLDER}/include/avnd/binding/max/init.hpp"
  "${AVND_FOLDER}/include/avnd/binding/max/audio_processor.hpp"
  "${AVND_FOLDER}/include/avnd/binding/ossia/node.hpp"
  "${AVND_FOLDER}/include/avnd/binding/ossia/to_value.hpp"
  "${AVND_FOLDER}/include/avnd/binding/ossia/all.hpp"
  "${AVND_FOLDER}/include/avnd/binding/ossia/geometry.hpp"
  "${AVND_FOLDER}/include/avnd/binding/ossia/mono_audio_node.hpp"
  "${AVND_FOLDER}/include/avnd/binding/ossia/poly_audio_node.hpp"
  "${AVND_FOLDER}/include/avnd/binding/ossia/value.hpp"
  "${AVND_FOLDER}/include/avnd/binding/ossia/ossia_audio_node.hpp"
  "${AVND_FOLDER}/include/avnd/binding/ossia/port_run_preprocess.hpp"
  "${AVND_FOLDER}/include/avnd/binding/ossia/time_controls.hpp"
  "${AVND_FOLDER}/include/avnd/binding/ossia/configure.hpp"
  "${AVND_FOLDER}/include/avnd/binding/ossia/ffts.hpp"
  "${AVND_FOLDER}/include/avnd/binding/ossia/port_run_postprocess.hpp"
  "${AVND_FOLDER}/include/avnd/binding/ossia/from_value.hpp"
  "${AVND_FOLDER}/include/avnd/binding/ossia/data_node.hpp"
  "${AVND_FOLDER}/include/avnd/binding/ossia/port_setup.hpp"
  "${AVND_FOLDER}/include/avnd/binding/ossia/soundfiles.hpp"
  "${AVND_FOLDER}/include/avnd/binding/pd/dsp.hpp"
  "${AVND_FOLDER}/include/avnd/binding/pd/all.hpp"
  "${AVND_FOLDER}/include/avnd/binding/pd/atom_iterator.hpp"
  "${AVND_FOLDER}/include/avnd/binding/pd/configure.hpp"
  "${AVND_FOLDER}/include/avnd/binding/pd/helpers.hpp"
  "${AVND_FOLDER}/include/avnd/binding/pd/init.hpp"
  "${AVND_FOLDER}/include/avnd/binding/pd/inputs.hpp"
  "${AVND_FOLDER}/include/avnd/binding/pd/message_processor.hpp"
  "${AVND_FOLDER}/include/avnd/binding/pd/messages.hpp"
  "${AVND_FOLDER}/include/avnd/binding/pd/outputs.hpp"
  "${AVND_FOLDER}/include/avnd/binding/pd/audio_processor.hpp"
  "${AVND_FOLDER}/include/avnd/binding/python/all.hpp"
  "${AVND_FOLDER}/include/avnd/binding/python/configure.hpp"
  "${AVND_FOLDER}/include/avnd/binding/python/processor.hpp"
  "${AVND_FOLDER}/include/avnd/binding/standalone/all.hpp"
  "${AVND_FOLDER}/include/avnd/binding/standalone/audio.hpp"
  "${AVND_FOLDER}/include/avnd/binding/standalone/standalone.hpp"
  "${AVND_FOLDER}/include/avnd/binding/standalone/configure.hpp"
  "${AVND_FOLDER}/include/avnd/binding/standalone/oscquery_mapper.hpp"
  "${AVND_FOLDER}/include/avnd/binding/ui/qml/toggle_ui.hpp"
  "${AVND_FOLDER}/include/avnd/binding/ui/qml/enum_ui.hpp"
  "${AVND_FOLDER}/include/avnd/binding/ui/qml/float_knob.hpp"
  "${AVND_FOLDER}/include/avnd/binding/ui/qml/float_slider.hpp"
  "${AVND_FOLDER}/include/avnd/binding/ui/qml/int_knob.hpp"
  "${AVND_FOLDER}/include/avnd/binding/ui/qml/int_slider.hpp"
  "${AVND_FOLDER}/include/avnd/binding/ui/qml/enum_control.hpp"
  "${AVND_FOLDER}/include/avnd/binding/ui/qml/float_control.hpp"
  "${AVND_FOLDER}/include/avnd/binding/ui/qml/int_control.hpp"
  "${AVND_FOLDER}/include/avnd/binding/ui/qml/toggle_control.hpp"
  "${AVND_FOLDER}/include/avnd/binding/ui/nuklear/nuklear.hpp"
  "${AVND_FOLDER}/include/avnd/binding/ui/nuklear_layout_ui.hpp"
  "${AVND_FOLDER}/include/avnd/binding/ui/qml_layout_ui.hpp"
  "${AVND_FOLDER}/include/avnd/binding/ui/qml_ui.hpp"
  "${AVND_FOLDER}/include/avnd/binding/vintage/all.hpp"
  "${AVND_FOLDER}/include/avnd/binding/vintage/helpers.hpp"
  "${AVND_FOLDER}/include/avnd/binding/vintage/dispatch.hpp"
  "${AVND_FOLDER}/include/avnd/binding/vintage/polyphonic_synth.hpp"
  "${AVND_FOLDER}/include/avnd/binding/vintage/programs.hpp"
  "${AVND_FOLDER}/include/avnd/binding/vintage/vintage.hpp"
  "${AVND_FOLDER}/include/avnd/binding/vintage/atomic_controls.hpp"
  "${AVND_FOLDER}/include/avnd/binding/vintage/audio_effect.hpp"
  "${AVND_FOLDER}/include/avnd/binding/vintage/configure.hpp"
  "${AVND_FOLDER}/include/avnd/binding/vintage/midi_processor.hpp"
  "${AVND_FOLDER}/include/avnd/binding/vintage/processor_setup.hpp"
  "${AVND_FOLDER}/include/avnd/binding/vintage/synth_helpers.hpp"
  "${AVND_FOLDER}/include/avnd/binding/vst3/all.hpp"
  "${AVND_FOLDER}/include/avnd/binding/vst3/audio_effect.hpp"
  "${AVND_FOLDER}/include/avnd/binding/vst3/refcount.hpp"
  "${AVND_FOLDER}/include/avnd/binding/vst3/types.hpp"
  "${AVND_FOLDER}/include/avnd/binding/vst3/bus_info.hpp"
  "${AVND_FOLDER}/include/avnd/binding/vst3/component.hpp"
  "${AVND_FOLDER}/include/avnd/binding/vst3/component_base.hpp"
  "${AVND_FOLDER}/include/avnd/binding/vst3/configure.hpp"
  "${AVND_FOLDER}/include/avnd/binding/vst3/connection_point.hpp"
  "${AVND_FOLDER}/include/avnd/binding/vst3/controller.hpp"
  "${AVND_FOLDER}/include/avnd/binding/vst3/controller_base.hpp"
  "${AVND_FOLDER}/include/avnd/binding/vst3/factory.hpp"
  "${AVND_FOLDER}/include/avnd/binding/vst3/helpers.hpp"
  "${AVND_FOLDER}/include/avnd/binding/vst3/metadata.hpp"
  "${AVND_FOLDER}/include/avnd/binding/vst3/programs.hpp"
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
  "${AVND_FOLDER}/include/avnd/common/struct_reflection.hpp"
  "${AVND_FOLDER}/include/avnd/common/aggregates.hpp"
  "${AVND_FOLDER}/include/avnd/common/array.hpp"
  "${AVND_FOLDER}/include/avnd/common/coroutines.hpp"
  "${AVND_FOLDER}/include/avnd/common/index.hpp"
  "${AVND_FOLDER}/include/avnd/common/optionals.hpp"
  "${AVND_FOLDER}/include/avnd/common/for_nth.hpp"
  "${AVND_FOLDER}/include/avnd/concepts/all.hpp"
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
  "${AVND_FOLDER}/include/gpp/meta.hpp"
  "${AVND_FOLDER}/include/gpp/commands.hpp"
  "${AVND_FOLDER}/include/gpp/generators.hpp"
  "${AVND_FOLDER}/include/gpp/ports.hpp"
  "${AVND_FOLDER}/include/gpp/layout.hpp"
  "${AVND_FOLDER}/include/halp/midifile_port.hpp"
  "${AVND_FOLDER}/include/halp/midi.hpp"
  "${AVND_FOLDER}/include/halp/callback.hpp"
  "${AVND_FOLDER}/include/halp/compat/gamma.hpp"
  "${AVND_FOLDER}/include/halp/controls_fmt.hpp"
  "${AVND_FOLDER}/include/halp/custom_widgets.hpp"
  "${AVND_FOLDER}/include/halp/fft.hpp"
  "${AVND_FOLDER}/include/halp/file_port.hpp"
  "${AVND_FOLDER}/include/halp/geometry.hpp"
  "${AVND_FOLDER}/include/halp/inline.hpp"
  "${AVND_FOLDER}/include/halp/layout.hpp"
  "${AVND_FOLDER}/include/halp/log.hpp"
  "${AVND_FOLDER}/include/halp/mappers.hpp"
  "${AVND_FOLDER}/include/halp/messages.hpp"
  "${AVND_FOLDER}/include/halp/meta.hpp"
  "${AVND_FOLDER}/include/halp/reactive_value.hpp"
  "${AVND_FOLDER}/include/halp/sample_accurate_controls.hpp"
  "${AVND_FOLDER}/include/halp/static_string.hpp"
  "${AVND_FOLDER}/include/halp/texture.hpp"
  "${AVND_FOLDER}/include/halp/texture_formats.hpp"
  "${AVND_FOLDER}/include/halp/polyfill.hpp"
  "${AVND_FOLDER}/include/halp/audio.hpp"
  "${AVND_FOLDER}/include/halp/controls.hpp"

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

  PRIVATE
    $<BUILD_INTERFACE:rubberband>
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
  Crousti/Layout.hpp
  Crousti/Metadatas.hpp
  Crousti/Painter.hpp
  Crousti/ProcessModel.hpp
  Crousti/Widgets.hpp
)
endif()


set(AVND_ADDITIONAL_CLASSES)
set(AVND_CUSTOM_FACTORIES)
function(avnd_make_score)

  cmake_parse_arguments(AVND "" "TARGET;MAIN_CLASS;NAMESPACE" "SOURCES" ${ARGN})

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

  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/prototype.cpp.in"
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
    target_compile_definitions(score_plugin_avnd PRIVATE AVND_USE_TUPLET_TUPLE=1)
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
  SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/AvndProcesses/Sweeper.hpp"
  TARGET pattern_sweeper
  MAIN_CLASS PatternSweeper
  NAMESPACE avnd_tools
)

avnd_make_score(
  SOURCES "${AVND_FOLDER}/examples/Helpers/GradientScrubber.hpp"
  TARGET gradient_scrub
  MAIN_CLASS GradientScrub
  NAMESPACE examples::helpers
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
