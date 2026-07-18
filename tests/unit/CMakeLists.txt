# Unit tests: pure logic, no application context. Fast to build and run.

score_add_test(test_unit_address
  SOURCES AddressParseTest.cpp
  PLUGINS score_lib_state)

# --- regression: per-app interface-list lookup in port deserialization -----
# Boots two headless apps in one process and round-trips ControlInlet /
# ControlOutlet through Process::load_inlet / load_outlet in each: a stale
# function-local static cache of the first app's PortFactoryList would be a
# heap-use-after-free in the second round. Needs the full app (APP) so the
# dynamic plugins that register the port factories (score_plugin_dataflow)
# are loaded from <build>/plugins.
score_add_test(test_unit_port_factory_reuse
  SOURCES PortFactoryListReuseTest.cpp
  APP
  PLUGINS score_lib_process)

# --- core data model (score-lib-state / score-lib-process, P3R2) -----------
# Value & serialization round-trips of the state model (Address / Message /
# Domain / value<->string) plus corrupt-buffer robustness.
score_add_test(test_unit_state_serialization
  SOURCES StateSerializationTest.cpp
  PLUGINS score_lib_state)

# Port / Cable / ProcessModel object graph + DataStream & JSON round-trips.
score_add_test(test_unit_process_model
  SOURCES ProcessModelTest.cpp
  PLUGINS score_lib_process score_lib_state score_lib_device)


# --- P3R2: deterministic value processors (curve / automation / spline /
# mapping). Pure-math sampling tests are app-free; serialization and process
# construction resolve factories through the app components, so those run in
# the headless app fixture (APP).
score_add_test(test_unit_curve_sample
  SOURCES CurveSampleTest.cpp
  PLUGINS score_plugin_curve)

score_add_test(test_unit_curve_roundtrip
  SOURCES CurveRoundtripTest.cpp
  APP
  PLUGINS score_plugin_curve score_lib_process)

score_add_test(test_unit_automation_values
  SOURCES AutomationValueTest.cpp
  APP
  PLUGINS score_plugin_automation score_plugin_curve score_lib_process)

score_add_test(test_unit_mapping_values
  SOURCES MappingTest.cpp
  APP
  PLUGINS score_plugin_mapping score_plugin_curve score_lib_process)

score_add_test(test_unit_spline_values
  SOURCES SplineValueTest.cpp
  PLUGINS score_plugin_spline)

# --- P3R2: score-plugin-midi + score-plugin-dataflow value tests -----------
# Midi::Note / the serialization template specializations / parsePatterns are
# not exported from the midi plugin (-fvisibility=hidden), so the relevant
# translation units are compiled directly into the test. A minimal
# score::ApplicationInterface defined in the test provides the PortFactoryList
# needed by the DataStream/JSON deserializers.
if(TARGET score_plugin_midi)
  set(_midi_src "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-midi")
  score_add_test(test_unit_midi_message
    SOURCES
      MidiMessageTest.cpp
      "${_midi_src}/Midi/MidiNote.cpp"
      "${_midi_src}/Midi/MidiProcess.cpp"
      "${_midi_src}/Patternist/PatternParsing.cpp"
    PLUGINS score_plugin_midi score_lib_process
    LIBS ${QT_PREFIX}::Gui)
endif()

# Dataflow: cable/port round-trips through the real (exported) dataflow port
# factories + ossia graph-edge value propagation.
if(TARGET score_plugin_dataflow)
  score_add_test(test_unit_dataflow_value
    SOURCES DataflowValueTest.cpp
    PLUGINS score_plugin_dataflow score_lib_process)
endif()


# --- analysis DSP ------------------------------------------------------------
# The analysis nodes are avnd wrappers over Analysis::GistState (header-only)
# + the vendored Gist DSP library. Gist's symbols are hidden inside
# libscore_plugin_analysis.so, so compile the Gist translation units directly
# into the test (same idiom as the threedim loaders above). Linking the plugin
# provides the Analysis/ + engine include dirs and the exported ossia::fft.
if(TARGET score_plugin_analysis)
  set(_gist_dir "${3RDPARTY_FOLDER}/Gist/src")
  score_add_test(test_unit_analysis_gist
    SOURCES
      AnalysisGistTest.cpp
      "${_gist_dir}/CoreFrequencyDomainFeatures.cpp"
      "${_gist_dir}/CoreTimeDomainFeatures.cpp"
      "${_gist_dir}/Gist.cpp"
      "${_gist_dir}/MFCC.cpp"
      "${_gist_dir}/OnsetDetectionFunction.cpp"
      "${_gist_dir}/WindowFunctions.cpp"
      "${_gist_dir}/Yin.cpp"
    PLUGINS score_plugin_analysis score_plugin_avnd)
  target_compile_definitions(test_unit_analysis_gist PRIVATE USE_OSSIA_FFT=1)
  target_include_directories(test_unit_analysis_gist SYSTEM PRIVATE "${_gist_dir}")
  target_include_directories(test_unit_analysis_gist PRIVATE
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-analysis")
endif()

# --- faust JIT DSP -----------------------------------------------------------
# The plugin compiles Faust programs with libfaust's LLVM JIT and maps the dsp
# UI onto score controls via the header-only Faust/Utils.hpp. The plugin
# *target* is absent in sanitizer builds (its CMakeLists returns early under
# SCORE_HAS_SANITIZERS), so the test links system libfaust directly, with the
# same FAUSTFLOAT=double the plugin sets PUBLIC, and score_lib_process for the
# Process::Inlet layer. Requires ASAN_OPTIONS=protect_shadow_gap=0 at runtime
# (LLVM JIT + ASAN).
if(TARGET score_plugin_faust)
  score_add_test(test_unit_faust_dsp
    SOURCES FaustDspTest.cpp
    PLUGINS score_plugin_faust)
  target_include_directories(test_unit_faust_dsp PRIVATE
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-faust")
else()
  find_package(Faust QUIET)
  if(FAUST_FOUND AND TARGET score_lib_process)
    score_add_test(test_unit_faust_dsp
      SOURCES FaustDspTest.cpp
      PLUGINS score_lib_process
      LIBS ${FAUST_LIBRARIES})
    target_compile_definitions(test_unit_faust_dsp PRIVATE FAUSTFLOAT=double)
    target_include_directories(test_unit_faust_dsp PRIVATE
      "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-faust")
    target_include_directories(test_unit_faust_dsp SYSTEM PRIVATE
      ${FAUST_INCLUDE_DIR})
  endif()
endif()

# --- P3R3: audio DSP value-asserting tests ---------------------------------
# avnd (avendish/halp) audio effects bundled through score-plugin-avnd
# (ao::Gain / AudioSum / MonoMix / StereoMixer / Silence). The nodes are plain
# header-only C++ structs driven directly; only the avendish include dirs are
# needed.
score_add_test(test_unit_avnd_audio_effects
  SOURCES AvndAudioEffectsTest.cpp)
target_include_directories(test_unit_avnd_audio_effects SYSTEM PRIVATE
  "${SCORE_ROOT_SOURCE_DIR}/3rdparty/avendish/include"
  "${SCORE_ROOT_SOURCE_DIR}/3rdparty/avendish/examples")

# score-plugin-fx audio-path nodes (MathAudioFilter / Envelope / LFO v2):
# header-only avnd structs; linking the plugin provides the Fx/ + Engine/
# include dirs and ossia::math_expression.
if(TARGET score_plugin_fx)
  score_add_test(test_unit_fx_audio_dsp
    SOURCES FxAudioDspTest.cpp
    PLUGINS score_plugin_fx)
endif()

# ossia::audio_outlet::post_process (gain * pan output stage of every audio
# process) + ossia::mix (cable summing): the DSP behind the gain/pan/mix path
# of the score audio graph.
score_add_test(test_unit_audio_gain_pan
  SOURCES AudioGainPanTest.cpp)

# Serialization round-trip of avnd audio process models (ao::Gain,
# MathAudioFilter): controls survive save/load through the real
# ProcessFactoryList / deserialize_interface path. Needs the headless app for
# the process & port factories (APP => runs from build root, offscreen).
score_add_test(test_unit_avnd_audio_roundtrip
  SOURCES AvndAudioRoundtripTest.cpp
  PLUGINS score_lib_process
  APP)
