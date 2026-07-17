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
score_add_test(test_unit_state_serialization
  SOURCES StateSerializationTest.cpp
  PLUGINS score_lib_state)

# Port / Cable / ProcessModel object graph + DataStream & JSON round-trips.
score_add_test(test_unit_process_model
  SOURCES ProcessModelTest.cpp
  PLUGINS score_lib_process score_lib_state score_lib_device)

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

if(TARGET score_plugin_dataflow)
  score_add_test(test_unit_dataflow_value
    SOURCES DataflowValueTest.cpp
    PLUGINS score_plugin_dataflow score_lib_process)
endif()


# --- analysis DSP ------------------------------------------------------------
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
score_add_test(test_unit_avnd_audio_effects
  SOURCES AvndAudioEffectsTest.cpp)
target_include_directories(test_unit_avnd_audio_effects SYSTEM PRIVATE
  "${SCORE_ROOT_SOURCE_DIR}/3rdparty/avendish/include"
  "${SCORE_ROOT_SOURCE_DIR}/3rdparty/avendish/examples")

if(TARGET score_plugin_fx)
  score_add_test(test_unit_fx_audio_dsp
    SOURCES FxAudioDspTest.cpp
    PLUGINS score_plugin_fx)
endif()

score_add_test(test_unit_audio_gain_pan
  SOURCES AudioGainPanTest.cpp)

score_add_test(test_unit_avnd_audio_roundtrip
  SOURCES AvndAudioRoundtripTest.cpp
  PLUGINS score_lib_process
  APP)
# AssetTable is pure CPU logic; TextureLoader upload paths run on QRhi's Null
# backend (score_plugin_gfx links Qt::GuiPrivate PUBLIC, providing qrhi_p.h).
score_add_test(test_unit_gfx_assettable
  SOURCES AssetTableTest.cpp
  PLUGINS score_plugin_gfx
  LIBS ${QT_PREFIX}::Gui)

# libisf importer branches (shadertoy / shadertoy-json / glsl-sandbox /
# write_isf / float-input min-max inference). isf::parser is exported from
# score_plugin_gfx, whose PUBLIC include dirs provide <isf.hpp>.
score_add_test(test_unit_isf_importers
  SOURCES IsfImportersTest.cpp
  PLUGINS score_plugin_gfx)

score_add_test(test_unit_video_pixel_format
  SOURCES VideoPixelFormatTest.cpp
  PLUGINS score_plugin_gfx score_plugin_media
  LIBS avutil)
