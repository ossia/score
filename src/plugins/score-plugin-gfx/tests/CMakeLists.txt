# App-free unit tests for score-plugin-gfx (Catch2, driven by score_add_test).
score_add_test(test_gfx_capture_sync
  SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/CaptureSyncGroupTest.cpp"
  PLUGINS score_plugin_gfx)

# The description -> device-tree control-group builder (V4L2 / render /
# Argus controls), fresh build and reload adoption.
score_add_test(test_gfx_control_tree
  SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/ControlTreeTest.cpp"
  PLUGINS score_plugin_gfx)

# The shared dlopen'd libv4l2 loader and its ioctl retry wrapper.
score_add_test(test_gfx_v4l2_loader
  SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/V4L2LoaderTest.cpp"
  PLUGINS score_plugin_gfx)

# The render-clock abstraction (timer / vsync / external genlock). The genlock
# clock's hardware wait is an injected function, so its thread contract runs
# with no card.
score_add_test(test_gfx_render_clock
  SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/RenderClockTest.cpp"
  PLUGINS score_plugin_gfx)

# The borrowed-host zero-copy capture rung's init() refusal ladder, on the
# Null QRhi (which genuinely has no host-import path, like GL/D3D11).
score_add_test(test_gfx_borrowed_host_import
  SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/BorrowedHostImportTest.cpp"
  PLUGINS score_plugin_gfx
  LIBS ${QT_PREFIX}::Gui)

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

# The KMS sink's own ports. Registered only where KmsOutputNode.cpp is actually
# compiled: without libdrm the plugin builds an inert stub whose constructor
# creates no ports at all, and asserting on that would be asserting the stub.
# SCORE_LIBDRM_FOUND comes from the parent scope (pkg_check_modules above
# add_subdirectory(Tests)).
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND SCORE_LIBDRM_FOUND)
  score_add_test(test_gfx_kms_output_port
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/KmsOutputPortTest.cpp"
    PLUGINS score_plugin_gfx
    LIBS ${QT_PREFIX}::Gui)
  target_include_directories(test_gfx_kms_output_port
    PRIVATE ${SCORE_LIBDRM_INCLUDE_DIRS})
endif()
# The slot rotation behind TextureShare. Header-only and atomic-only, so the
# state machine that keeps the producer's render target and the consumer's
# sampled texture apart is checkable exhaustively, with no QRhi and no GPU.
score_add_test(test_gfx_triple_buffer_index
  SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/TripleBufferIndexTest.cpp"
  PLUGINS score_plugin_gfx)

# The ring/fence policy surface every GPU-direct video path consults before it
# moves a byte: the BAR1 depth rule, the substituted no-op DMA lock, what a
# stub fence is allowed to claim, and both slot rings' config gates. Uses
# QRhi's Null backend for the bookkeeping around QRhi only -- never to conclude
# anything about rendering.
score_add_test(test_gfx_interop_ring_policy
  SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/InteropRingPolicyTest.cpp"
  PLUGINS score_plugin_gfx
  LIBS ${QT_PREFIX}::Gui)
