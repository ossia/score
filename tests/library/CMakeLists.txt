# Tests for the process-library model population (staged subtree publish).
# APP tests: they need the full headless application so that the settings
# models and the process factories from every plugin are registered.
score_add_test(test_library_publish
  SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/ProcessLibraryPublishTest.cpp"
  APP
  PLUGINS score_plugin_library score_lib_process)
