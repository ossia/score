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

# Device explorer tree conversion: which ossia parameters become editable
# Device::Node values (regression #2204: audio gains).
score_add_test(test_unit_device_explorer_node
  SOURCES DeviceExplorerNodeTest.cpp
  PLUGINS score_lib_device)

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

if(TARGET score_plugin_mapping)
  score_add_test(test_unit_mapping_values
    SOURCES MappingTest.cpp
    APP
    PLUGINS score_plugin_mapping score_plugin_curve score_lib_process)
endif()

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

  # Patternist's step engine, driven directly through pattern_node.
  score_add_test(test_unit_patternist_node
    SOURCES PatternistNodeTest.cpp
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

# --- cross-implementation quantification grid parity ------------------------
# ossia::token_request vs halp::tick_musical: a native node and an avendish
# plug-in on the same score must snap to the same samples.
score_add_test(test_unit_quantification_parity
  SOURCES QuantificationParityTest.cpp
  PLUGINS score_lib_process)
target_include_directories(test_unit_quantification_parity SYSTEM PRIVATE
  "${SCORE_ROOT_SOURCE_DIR}/3rdparty/avendish/include")

# --- P3R3: audio DSP value-asserting tests ---------------------------------
score_add_test(test_unit_avnd_audio_effects
  SOURCES AvndAudioEffectsTest.cpp)
target_include_directories(test_unit_avnd_audio_effects SYSTEM PRIVATE
  "${SCORE_ROOT_SOURCE_DIR}/3rdparty/avendish/include"
  "${SCORE_ROOT_SOURCE_DIR}/3rdparty/avendish/examples")

score_add_test(test_unit_avnd_midi_envelope
  SOURCES AvndMidiEnvelopeTest.cpp)
target_include_directories(test_unit_avnd_midi_envelope SYSTEM PRIVATE
  "${SCORE_ROOT_SOURCE_DIR}/3rdparty/avendish/include"
  "${SCORE_ROOT_SOURCE_DIR}/3rdparty/avendish/examples")

score_add_test(test_unit_avnd_midi_humanizer
  SOURCES AvndMidiHumanizerTest.cpp)
target_include_directories(test_unit_avnd_midi_humanizer SYSTEM PRIVATE
  "${SCORE_ROOT_SOURCE_DIR}/3rdparty/avendish/include"
  "${SCORE_ROOT_SOURCE_DIR}/3rdparty/avendish/examples")

if(TARGET score_plugin_fx)
  score_add_test(test_unit_fx_audio_dsp
    SOURCES FxAudioDspTest.cpp
    PLUGINS score_plugin_fx)
endif()

score_add_test(test_unit_audio_gain_pan
  SOURCES AudioGainPanTest.cpp)

# --- graphics control dragging (pointer lock / infinite scroller) -----------
# Drives InfiniteScroller against a fake score::PointerLock. InfiniteScroller.cpp
# is compiled into the test so that its calls bind to the fake rather than to the
# platform backend in score_lib_base.
score_add_test(test_unit_infinite_scroller
  STANDALONE
  SOURCES
    InfiniteScrollerTest.cpp
    "${SCORE_ROOT_SOURCE_DIR}/src/lib/score/graphics/InfiniteScroller.cpp"
  LIBS
    ${QT_PREFIX}::Gui
    ${QT_PREFIX}::GuiPrivate
    ${QT_PREFIX}::Widgets)
target_compile_definitions(test_unit_infinite_scroller
  PRIVATE SCORE_LIB_BASE_STATIC_DEFINE)

# Drives the note chooser through its own mouse handlers with the real
# InfiniteScroller: on the offscreen platform no pointer lock is granted, so the
# drag goes through the pointer positions.
score_add_test(test_unit_note_chooser
  SOURCES NoteChooserTest.cpp
  APP)

# The spinbox maps the drag itself instead of going through
# InfiniteScroller::move(); APP because its header pulls in the skin.
score_add_test(test_unit_spinbox_drag
  SOURCES SpinboxDragTest.cpp
  APP)

score_add_test(test_unit_avnd_audio_roundtrip
  SOURCES AvndAudioRoundtripTest.cpp
  PLUGINS score_lib_process
  APP)

# --- LV2 loading & processing, against the fixture bundle -------------------
if(TARGET score_plugin_lv2)
  score_add_test(test_unit_lv2_loading
    SOURCES LV2LoadingTest.cpp
    APP
    PLUGINS score_plugin_lv2 score_lib_process
    LIBS Lilv)
  add_dependencies(test_unit_lv2_loading score_test_gain_lv2)
  target_compile_definitions(test_unit_lv2_loading PRIVATE
    SCORE_TEST_LV2_BUNDLE="${SCORE_ROOT_BINARY_DIR}/tests/lv2/score-test-gain.lv2")
  target_include_directories(test_unit_lv2_loading PRIVATE
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-lv2"
    "${LV2_PATH}"
    $<TARGET_PROPERTY:Suil,INTERFACE_INCLUDE_DIRECTORIES>
    "${OSSIA_3RDPARTY_FOLDER}/readerwriterqueue")
endif()
