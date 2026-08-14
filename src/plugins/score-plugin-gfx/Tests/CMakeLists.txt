# App-free unit tests for score-plugin-gfx (Catch2, driven by score_add_test).
score_add_test(test_gfx_capture_sync
  SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/CaptureSyncGroupTest.cpp"
  PLUGINS score_plugin_gfx)

score_add_test(test_gfx_capture_correlator
  SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/CaptureCorrelatorTest.cpp"
  PLUGINS score_plugin_gfx)
