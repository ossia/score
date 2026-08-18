# App-free unit tests for score-plugin-gfx (Catch2, driven by score_add_test).
score_add_test(test_gfx_capture_sync
  SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/CaptureSyncGroupTest.cpp"
  PLUGINS score_plugin_gfx)

score_add_test(test_gfx_capture_correlator
  SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/CaptureCorrelatorTest.cpp"
  PLUGINS score_plugin_gfx)

# The slot handoff and the format seqlock every capture strategy is built on.
score_add_test(test_gfx_capture_slot_lifetime
  SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/CaptureSlotLifetimeTest.cpp"
  PLUGINS score_plugin_gfx)

# The output-side pacing ring and its frame-ownership bookkeeping.
score_add_test(test_gfx_paced_frame_pump
  SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/PacedFramePumpTest.cpp"
  PLUGINS score_plugin_gfx)

score_add_test(test_gfx_host_frame_pool
  SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/HostFramePoolTest.cpp"
  PLUGINS score_plugin_gfx)
