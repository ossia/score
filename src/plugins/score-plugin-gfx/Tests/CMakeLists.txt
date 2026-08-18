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

# The baked-GLSL -> external-sampler rewrite shared by the two external-image
# decoders. The plugin is built with hidden visibility and does not export
# toExternalSamplerEssl, so the TU that defines it is compiled into the test,
# as tests/gfx does for the engine sources it needs.
# ... but only when the plugin is a shared library. A static plugin archive
# already carries that translation unit, and visibility does not apply to an
# archive, so compiling it a second time is a duplicate-symbol link error.
if(SCORE_STATIC_PLUGINS OR NOT BUILD_SHARED_LIBS)
  set(_gfx_essl_extra_sources)
else()
  set(_gfx_essl_extra_sources
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-gfx/Gfx/Graph/decoders/NV12ExternalOES.cpp")
endif()

score_add_test(test_gfx_external_essl
  SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/ExternalEsslRewriteTest.cpp"
    ${_gfx_essl_extra_sources}
  PLUGINS score_plugin_gfx score_plugin_media
  LIBS ${QT_PREFIX}::Gui)

# The plane-layout, pitch and rung-gating decisions DmaBufImportCapture makes
# before it imports anything. Linux-only, like the header.
score_add_test(test_gfx_dmabuf_layout
  SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/DmaBufLayoutTest.cpp"
  PLUGINS score_plugin_gfx)
