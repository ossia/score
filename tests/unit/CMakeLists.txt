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
