# Tests for the process-library model population (staged subtree publish).
# APP tests: they need the full headless application so that the settings
# models and the process factories from every plugin are registered.
score_add_test(test_library_publish
  SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/ProcessLibraryPublishTest.cpp"
  APP
  PLUGINS score_plugin_library score_lib_process)

# The library's presets and their files: one read per scan, renamed and deleted
# in the user library only. Sandboxed: it deletes files.
score_add_test(test_library_preset_files
  SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/LibraryPresetFilesTest.cpp"
  APP
  SANDBOXED
  PLUGINS score_plugin_library score_plugin_fx score_plugin_scenario score_lib_process)
