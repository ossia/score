# L1 node/process sweep tests: instantiate + serialize-roundtrip every
# registered factory. Run from the build root (APP mode) so the dynamic plugins
# in <build>/plugins are discovered at runtime and ALL factories are registered.

score_add_test(test_nodes_processes
  SOURCES Processes.cpp
  APP
  PLUGINS score_plugin_scenario score_lib_process)

# The views of every process: layers in every mode and size, nodes,
# inspectors; then the monitoring views while the document plays.
score_add_test(test_nodes_process_views
  SOURCES ProcessViews.cpp
  GUI
  APP
  PLUGINS score_plugin_scenario score_plugin_engine score_plugin_audio score_lib_process score_lib_inspector)
target_compile_definitions(test_nodes_process_views PRIVATE
  GFX_TEST_CORPUS_DIR="${SCORE_ROOT_SOURCE_DIR}/tests/gfx/corpus"
  THREEDIM_TEST_DIR="${SCORE_ROOT_SOURCE_DIR}/tests/threedim")
# The monitors of the Geometry viewer and of the texture views, header only
target_include_directories(test_nodes_process_views PRIVATE
  "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-threedim"
  "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-ui")

score_add_test(test_nodes_curves
  SOURCES Curves.cpp
  APP
  PLUGINS score_plugin_curve score_lib_process)

score_add_test(test_nodes_devices
  SOURCES Devices.cpp
  APP
  PLUGINS score_lib_device)

score_add_test(test_nodes_veltonote
  SOURCES VelToNoteTest.cpp VelToNoteBindingTest.cpp
  PLUGINS score_plugin_fx)

score_add_test(test_nodes_ossia_midi
  SOURCES "${SCORE_ROOT_SOURCE_DIR}/3rdparty/avendish/tests/test_ossia_midi.cpp"
  PLUGINS score_plugin_fx)
