# L3 GPU render + readback tests.
#
# Build an offscreen ISF pipeline (score::gfx graph), render a few frames and
# assert on the read-back RGBA8 pixels. Needs a working QRhi backend; when none
# is available headless the tests SKIP cleanly (CI-safe).
#
# GUI mode: links Qt Gui/Widgets and runs from the build root so the dynamic
# gfx plugin resources are discovered. The offscreen platform is supplied by the
# ctest invocation (QT_QPA_PLATFORM=offscreen).
# --- port note ---------------------------------------------------------------
# The suite was written against a tree where ISFNode / programFromISFFragment-
# ShaderPath / ... carried SCORE_PLUGIN_GFX_EXPORT. The current tree builds the
# plugin with hidden visibility and does not export them, so (as the threedim
# loader unit tests already do for the same reason) the needed TUs are compiled
# straight into a static glue lib linked by every gfx test target. src/ is not
# modified.
set(_gfx_src "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-gfx")
add_library(test_gfx_engine_glue STATIC
  "${_gfx_src}/Gfx/Graph/ISFNode.cpp"
  "${_gfx_src}/Gfx/Graph/RenderedISFNode.cpp"
  "${_gfx_src}/Gfx/Graph/SimpleRenderedISFNode.cpp"
  "${_gfx_src}/Gfx/Graph/RenderedCSFNode.cpp"
  "${_gfx_src}/Gfx/Graph/RenderedRawRasterPipelineNode.cpp"
  "${_gfx_src}/Gfx/Graph/RenderedVSANode.cpp"
  "${_gfx_src}/Gfx/Graph/CustomMesh.cpp"
  "${_gfx_src}/Gfx/Graph/GPUBufferScatter.cpp"
  "${_gfx_src}/Gfx/ShaderProgram.cpp")
target_link_libraries(test_gfx_engine_glue PUBLIC
  score_plugin_gfx score_lib_process ${QT_PREFIX}::Gui)
set_target_properties(test_gfx_engine_glue PROPERTIES
  FOLDER "Tests" POSITION_INDEPENDENT_CODE ON)

score_add_test(test_gfx_isf
  SOURCES Gfx.cpp
  GUI
  PLUGINS score_plugin_gfx score_plugin_scenario score_lib_process
  LIBS test_gfx_engine_glue)

# Absolute path to the committed corpus shaders so the tests are self-contained
# and do not depend on the developer's ~/Documents checkout.
target_compile_definitions(test_gfx_isf PRIVATE
  GFX_TEST_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/corpus")

# L3 regression tests: one per render-engine bug fixed on split/gfx-scene. Each
# renders a real offscreen pipeline (fan-out / diamond / mid-run graph mutation)
# and asserts the readback across every available RHI backend.
score_add_test(test_gfx_regressions
  SOURCES GfxRegressions.cpp
  GUI
  PLUGINS score_plugin_gfx score_plugin_scenario score_lib_process
  LIBS test_gfx_engine_glue)

target_compile_definitions(test_gfx_regressions PRIVATE
  GFX_TEST_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/corpus")

# -----------------------------------------------------------------------------
# ISF feature-surface L3 tests (Priority 2). One focused target per feature
# group; each renders committed corpus shaders offscreen and asserts the readback
# on EVERY available RHI backend (Catch2 GENERATE over platform_backends()).
#
#   *_image        image inputs / passthrough (two-images, image-depth)
#   *_mrt          multiple render targets (depth+color, g-buffer)
#   *_multipass    multipass (three-pass, fixed/expr pass size)
#   *_persistent   persistent / feedback accumulation
#   *_filter       nearest sampler filtering
#   *_uniforms     time/built-in uniforms + long numeric input
#   *_unsupported  honest SKIPs for inputs the fixture can't feed (audio/cubemap/
#                  control-value/uniform_input/self-feedback)
#
# The two *finding* targets below are ISOLATED on purpose: they hold corpus
# shaders that currently render incorrectly (RED) — kept separate so those
# failures (incl. a Vulkan crash) can't take down the passing groups.
#   *_findings       multipass storage/persistent-SSBO final pass renders black
#   *_mrt_persistent MRT + persistent fragment SSBO (OpenGL outB blank; Vulkan crash)
function(score_add_isf_test name source)
  score_add_test(test_gfx_isf_${name}
    SOURCES ${source}
    GUI
    PLUGINS score_plugin_gfx score_plugin_scenario score_lib_process
    LIBS test_gfx_engine_glue)
  target_compile_definitions(test_gfx_isf_${name} PRIVATE
    GFX_TEST_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/corpus")
endfunction()

score_add_isf_test(image          IsfImageInputs.cpp)

# Regression guard for the storage-buffer-into-uniform_input descriptor mismatch:
# invalid on Vulkan (VUID-VkWriteDescriptorSet-descriptorType-00330, then a
# SIGSEGV in setShaderResources) and silently nondeterministic on OpenGL.
score_add_isf_test(buffer_usage_mismatch GfxBufferUsageMismatch.cpp)
# CSF (compute-shader) L3 tests reuse the same helper (GUI app, corpus dir).
function(score_add_csf_test name source)
  score_add_test(test_gfx_csf_${name}
    SOURCES ${source}
    GUI
    PLUGINS score_plugin_gfx score_plugin_scenario score_lib_process
    LIBS test_gfx_engine_glue)
  target_compile_definitions(test_gfx_csf_${name} PRIVATE
    GFX_TEST_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/corpus")
endfunction()

# CSF compute IMAGE-output readback (2D image formats + texture sampling).
score_add_csf_test(image    CsfImage.cpp)
# CSF 3D_IMAGE dispatch -> ISF sampler3D slice viewer. ISOLATED and EXPECTED RED:
# the 3D image path is CORRECT on Vulkan but reads back all-black on OpenGL (a
# real GL-specific engine bug) — kept as an honest failing finding, like
# test_gfx_isf_findings, so the RED is attributable and can't pull down the
# green 2D group. See the FINDING block in CsfImage3d.cpp.
score_add_csf_test(image3d  CsfImage3d.cpp)
# CSF cube + array storage-image compute write, sampled downstream. ISOLATED like
# image3d: the GL layered-storage workaround must cover cube/array (824a08d39),
# else faces/layers past 0 read black on OpenGL. Green on both backends post-fix.
score_add_csf_test(cube_array CsfCubeArray.cpp)
# CSF geometry / storage-buffer producers: parse + port-surface coverage; the
# readback itself SKIPs (no Geometry/Buffer-consuming sink in this fixture yet).
score_add_csf_test(geometry CsfGeometry.cpp)

score_add_isf_test(mrt            IsfMrt.cpp)
score_add_isf_test(multipass      IsfMultipass.cpp)
score_add_isf_test(persistent     IsfPersistent.cpp)
score_add_isf_test(filter         IsfFilter.cpp)
score_add_isf_test(uniforms       IsfUniforms.cpp)
score_add_isf_test(unsupported    IsfUnsupported.cpp)
score_add_isf_test(findings       IsfFindings.cpp)
score_add_isf_test(mrt_persistent IsfMrtPersistent.cpp)

# -----------------------------------------------------------------------------
# RAW-RASTER (RenderPipeline) + VSA + EXECUTION_MODEL L3 tests (Priority 4).
#
# The raw-raster node is a score::gfx::ISFNode built with a RawRaster descriptor
# (RenderedRawRasterPipelineNode); it consumes a Types::Geometry edge from a CSF
# geometry producer and renders it into color attachment(s). VSA is the same
# ISFNode class with a VSA descriptor (SimpleRenderedVSANode). All of these live
# in score_plugin_gfx — no threedim dependency is needed because we build the
# graph node directly (ProgramCache + ISFNode), mirroring the process models.
#
#   test_gfx_raster    CSF-geometry -> raw-raster -> texture (basic / stride /
#                      MRT / auxiliary). Validates raw-raster AND, indirectly,
#                      the CSF geometry producer (drawn pixels reflect geometry).
#   test_gfx_vsa       VSA vertex-shader-art incl. the cull-mode regression R3-N4
#                      (front face visible + OpenGL/Vulkan agreement).
#   test_gfx_execmodel EXECUTION_MODEL (PER_MIP/PER_CUBE_FACE/PER_LAYER) parse +
#                      port-surface coverage, and a procedural PER_LAYER render
#                      smoke test (R3-N7 per-invocation SRB does not UAF/crash).
function(score_add_gfx_test name source)
  score_add_test(test_gfx_${name}
    SOURCES ${source}
    GUI
    PLUGINS score_plugin_gfx score_plugin_scenario score_lib_process
    LIBS test_gfx_engine_glue)
  target_compile_definitions(test_gfx_${name} PRIVATE
    GFX_TEST_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/corpus")
endfunction()

score_add_gfx_test(raster    GfxRaster.cpp)
score_add_gfx_test(vsa       GfxVsa.cpp)
score_add_gfx_test(execmodel GfxExecutionModel.cpp)

# Vertical orientation of the texture-producing paths (single-output ISF, MRT,
# CSF sampled relay), asserted per row against each shader's closed-form ramp on
# every backend. Green on OpenGL and Vulkan.
score_add_gfx_test(orientation GfxOrientation.cpp)

# ISOLATED, EXPECTED-RED on OpenGL: the compute storage-image origin. A CSF that
# only imageStore()s is flipped on OpenGL because a raw texel index and a
# bottom-up GL render target disagree, and neither of the two one-line fixes is
# correct (both measured — see the FINDING block in the file). Own target so the
# attributable RED cannot take the green orientation group with it.
score_add_gfx_test(orientation_findings GfxOrientationFindings.cpp)

# ISOLATED, EXPECTED-RED: the VSA triangle cull-mode cross-backend divergence
# (R3-N4 area). A front-facing VSA triangle is culled on exactly one backend
# regardless of winding (OpenGL draws / Vulkan culls, or vice-versa) — there is
# no winding visible on both. Kept in its own target, like test_gfx_isf_findings
# / test_gfx_csf_image3d, so the attributable RED can't take down the green VSA
# group. See the FINDING block in GfxVsaCull.cpp.
score_add_gfx_test(vsa_cull  GfxVsaCull.cpp)

# -----------------------------------------------------------------------------
# CONTROL-VALUE injection (Priority 2 inputs) + INCREMENTAL graph-edit (Priority
# 5). Both reuse the fixture (GUI app, corpus dir).
#
#   test_gfx_control     ISF float/color/point2D/point3D/bool/long control inlets
#                        injected via setControl()/render_isf_controls() (the same
#                        material-UBO path the exec engine drives) and asserted
#                        analytically per backend. Closes the "control-value inputs
#                        are not injectable" SKIP in IsfUnsupported.cpp.
#   test_gfx_incremental incremental graph-edit path (SAFE/green): resize a sink
#                        mid-render (resizeSink -> recreateOutputRenderList) and
#                        reconnect after disconnect. No crash + correct pixels
#                        per backend, on OpenGL and Vulkan.
#   test_gfx_incremental_add add a node + edge mid-render (addEdgeIncremental):
#                        the new node's output appears at a previously-idle sink.
#                        ISOLATED (own process) so its Vulkan teardown can't
#                        perturb the green cases (see GfxIncrementalFindings).
#   test_gfx_incremental_findings ISOLATED, EXPECTED-RED: the incremental
#                        add-of-a-new-output path leaks a VkRenderPass on Vulkan;
#                        a subsequent in-process resize dereferences the stale
#                        handle and SIGSEGVs. OpenGL green; Vulkan crash contained.
score_add_gfx_test(control              GfxControl.cpp)
score_add_gfx_test(audio                GfxAudio.cpp)
score_add_gfx_test(incremental          GfxIncremental.cpp)
score_add_gfx_test(incremental_add      GfxIncrementalAdd.cpp)
score_add_gfx_test(incremental_findings GfxIncrementalFindings.cpp)

# -----------------------------------------------------------------------------
# ISOLATED regression guards for the split/gfx-scene engine fixes (REVIEW-ROUND2).
# Each is its OWN executable so a crash-on-failure on the pre-fix engine (the
# RED-proof) cannot perturb its neighbours — mirroring test_gfx_isf_mrt_persistent
# / test_gfx_csf_image3d / test_gfx_incremental_findings.
#
#   rt_changed   R2-#4  surgical rt_changed must rebuild an intermediate node's
#                       own output passes (3155a7f61).
#   node_removal R2-#11 node removal must release transitively-unreachable
#                       upstream renderers (48a9a1a7b).
#   storage_vis  R2-#6  storage-visibility filter drift (8ab01f368).
#   depth_sampler R2-#7 sampler-index miscount across SamplableDepth (8ab01f368).
#   multiview    R2-#1  multiview UBO binding collision (8ab01f368).
#   csf_cube_array R2-#10 GL layered rebind covers cube/array storage images
#                       (824a08d39).
score_add_gfx_test(rt_changed      GfxRtChanged.cpp)
score_add_gfx_test(node_removal    GfxNodeRemoval.cpp)
score_add_gfx_test(depth_sampler   GfxDepthSampler.cpp)
score_add_gfx_test(storage_visibility GfxStorageVisibility.cpp)
score_add_gfx_test(multiview       GfxMultiview.cpp)
score_add_gfx_test(dynamic_slot    GfxDynamicSlot.cpp)
# The three scene-filter nodes added by 45d1202327, which had zero references
# anywhere under tests/ while being user-facing processes.
score_add_gfx_test(scene_filter_nodes SceneFilterNodes.cpp)
# GpuResourceRegistry's default ctor is inline and value-initialises a
# unique_ptr<OffsetAllocator::Allocator>, so instantiating a registry in the test
# TU needs OffsetAllocator::Allocator::~Allocator() — compiled into the gfx plugin
# but not exported. Compile the (tiny, self-contained) OffsetAllocator source into
# this test target directly (the header says it is meant to be dropped in as-is).
# offsetAllocator.cpp has no include guard of its own and defines file-scope
# helpers that collide once several translation units are concatenated, so it
# must stay out of the unity blob.
set_source_files_properties(
  "${SCORE_ROOT_SOURCE_DIR}/3rdparty/OffsetAllocator/offsetAllocator.cpp"
  PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)

target_sources(test_gfx_dynamic_slot PRIVATE
  "${SCORE_ROOT_SOURCE_DIR}/3rdparty/OffsetAllocator/offsetAllocator.cpp")
target_include_directories(test_gfx_dynamic_slot PRIVATE
  "${SCORE_ROOT_SOURCE_DIR}/3rdparty/OffsetAllocator")

# The scalar-arena allocate/free/isLive surface and the mesh-slab arena, which
# GfxDynamicSlot leaves inert (it calls sweepMeshSlabs but never
# acquireMeshSlab). Same OffsetAllocator inclusion as above, for the same reason.
score_add_gfx_test(mesh_slabs GfxMeshSlabs.cpp)
target_sources(test_gfx_mesh_slabs PRIVATE
  "${SCORE_ROOT_SOURCE_DIR}/3rdparty/OffsetAllocator/offsetAllocator.cpp")
target_include_directories(test_gfx_mesh_slabs PRIVATE
  "${SCORE_ROOT_SOURCE_DIR}/3rdparty/OffsetAllocator")

# R2-#8 (GfxContext incremental edge-to-not-yet-present-node) is a DOCUMENTED
# GAP: the two-channel GfxContext baseline-diff bug is not reproducible through
# the Graph-level L3 fixture. This target holds the honest SKIP anchor + rationale.
score_add_gfx_test(incremental_edge_defer GfxIncrementalEdgeDefer.cpp)

# -----------------------------------------------------------------------------
# UNIT regression guards (REVIEW-ROUND2, round 2 fixes). These are non-GPU pure
# logic/unit tests — no display, no QRhi — so they build as plain score_add_test
# targets (Catch2WithMain), each its own executable.
#
#   assettable_eviction  F12/R2-#12 AssetTable: a staged-but-never-acquired
#                         entry must be trimmable (a7464ffef). Clean RED->GREEN
#                         unit test constructing AssetTable directly (needs the
#                         QImage member in DecodedAsset -> link Qt Gui).
#   scatter_dispatch      F5/R2-#5  GPUBufferScatter dispatch-dims clamp + shader
#                         index linearization (0efeab422). Pure-math UNIT test of
#                         the algorithm (replicates the private helper + GLSL
#                         formula; see the file header for scope/honesty).
#   gstreamer_bus_classify F9/R2-#9 GStreamer bus WARNING-vs-ERROR classification
#                         + feed/finalize decoupling (257035968). DOCUMENTED GAP
#                         (device behind SCORE_HAS_GSTREAMER/dlopen, unexported)
#                         + behavioral-model guard of the fix's contract.
score_add_test(test_gfx_assettable_eviction
  SOURCES GfxAssetTableEviction.cpp
  PLUGINS score_plugin_gfx
  LIBS ${QT_PREFIX}::Gui)
score_add_test(test_gfx_scatter_dispatch  SOURCES GfxScatterDispatch.cpp)
score_add_test(test_gfx_gstreamer_bus_classify SOURCES GfxGStreamerBusClassify.cpp)


# toExternalSamplerEssl rewrites baked GLSL into the GL_OES_EGL_image_external
# form for the NV12/Bayer external-texture decoders. Its "this is not baked
# GLSL" guard is the interesting part: it is the only thing between a non-baked
# input and a shader with the extension pragma spliced in at an arbitrary
# offset.
score_plugin_hidden_sources(_external_oes_hidden
    "${_gfx_src}/Gfx/Graph/decoders/NV12ExternalOES.cpp")
score_add_test(test_gfx_external_sampler_essl
  SOURCES
    ExternalSamplerEssl.cpp
    ${_external_oes_hidden}
  APP
  PLUGINS score_plugin_gfx score_plugin_media score_lib_process)
# -----------------------------------------------------------------------------
# Multi-window output editing canvases (Gfx/Window). Pure geometry driven
# through QGraphicsScene::sendEvent: coordinate mapping, snapping, canvas
# clamping, lock modes, re-indexing, blend handles and 4-corner warp. No QRhi
# and no visible window -- run_in_app boots the headless offscreen stack.
#
# OutputMapping.cpp / DesktopLayout.cpp are not exported from the dynamic
# score_plugin_gfx (hidden visibility), so both translation units are compiled
# into the test target, as test_gfx_engine_glue does for the render nodes.
score_plugin_hidden_sources(_window_layout_hidden
    "${_gfx_src}/Gfx/Window/OutputMapping.cpp"
    "${_gfx_src}/Gfx/Window/DesktopLayout.cpp"
    "${_gfx_src}/Gfx/Window/TestCard.cpp")
score_add_test(test_gfx_window_layout
  SOURCES
    WindowLayoutCanvas.cpp
    ${_window_layout_hidden}
  APP
  PLUGINS score_plugin_gfx score_lib_process)

# -----------------------------------------------------------------------------
# PRESENTED-SWAPCHAIN output tests. Everything else in this directory renders
# into an offscreen BackgroundNode and reads it back, which leaves
# score::gfx::Window, ScreenNode and MultiWindowNode -- i.e. every path that
# only exists because a real platform surface is in front of the renderer --
# entirely unexercised. These two targets terminate the same score::gfx::Graph
# on a real window instead.
#
# They are GUI targets (label "gui", real display, no forced offscreen QPA) and
# SKIP cleanly when no native window can be exposed. Split in two so a hang or
# a crash in the multi-window case cannot take the single-window one with it,
# and so the ctest timeout applies per group.
score_add_gfx_test(screen_output      ScreenOutput.cpp)
score_add_gfx_test(multi_window       MultiWindowOutput.cpp)

# ISOLATED, EXPECTED-RED: a live swapchain-flag toggle on an output that is part
# of a Graph frees the QRhi under the RenderList built against it. Own target,
# like test_gfx_isf_findings / test_gfx_csf_image3d / test_gfx_vsa_cull, so the
# attributable RED cannot pull down the two green suites above. See the FINDING
# block at the top of the file.
score_add_gfx_test(screen_findings    ScreenOutputFindings.cpp)

# -----------------------------------------------------------------------------
# The multi-window preview windows (Gfx/Window/OutputPreview.cpp): four preview
# contents painted through a corner-warp + rotation/mirror transform with
# soft-edge blend overlays, plus the window-per-mapping bookkeeping. Pure
# QPainter work, so QWidget::grab() renders it offscreen and the pixels are
# asserted numerically (the blend overlay has the closed form the shader uses).
score_plugin_hidden_sources(_output_preview_hidden
    "${_gfx_src}/Gfx/Window/OutputPreview.cpp"
    "${_gfx_src}/Gfx/Window/TestCard.cpp")
score_add_test(test_gfx_output_preview
  SOURCES
    OutputPreviewPaint.cpp
    ${_output_preview_hidden}
  APP
  PLUGINS score_plugin_gfx score_lib_process)

# The protocol settings widget of the window output device (1161 lines, 0%):
# mode switching, the swapchain flag/format combos, the per-output inspector
# (position/size/fullscreen/source rect/blend/transform/lock mode/screen), the
# output selector buttons and the add/remove path, plus the WindowSettings
# round-trip getSettings()/setSettings() sits on top of. Offscreen, no GPU.
score_plugin_hidden_sources(_window_settings_hidden
    "${_gfx_src}/Gfx/Window/WindowSettingsWidget.cpp"
    "${_gfx_src}/Gfx/Window/OutputMapping.cpp"
    "${_gfx_src}/Gfx/Window/DesktopLayout.cpp"
    "${_gfx_src}/Gfx/Window/OutputPreview.cpp"
    "${_gfx_src}/Gfx/Window/TestCard.cpp")
score_add_test(test_gfx_window_settings_ui
  SOURCES
    WindowSettingsUi.cpp
    ${_window_settings_hidden}
  APP
  PLUGINS score_plugin_gfx score_lib_process score_plugin_deviceexplorer)

# The FFmpeg (libav) input/output device: its settings widget (781 lines, 0%)
# and the preset enumerators (301 lines, 0%). Most of the widget is libav
# introspection — a muxer choice filters the encoder lists, an encoder choice
# repopulates the pixel/sample format lists from the codec's declared config,
# and the validation label comes from av_guess_format /
# avcodec_find_encoder_by_name. Offscreen, no GPU.
if(TARGET score_plugin_media)
  score_plugin_hidden_sources(_libav_settings_hidden
      "${_gfx_src}/Gfx/Libav/LibavSettingsWidget.cpp"
      "${_gfx_src}/Gfx/Libav/LibavDevice.cpp"
      "${_gfx_src}/Gfx/Libav/LibavEncoder.cpp"
      "${_gfx_src}/Gfx/Libav/LibavEncoderNode.cpp"
      "${_gfx_src}/Gfx/Libav/LibavPresets.cpp")
  score_add_test(test_gfx_libav_settings_ui
    SOURCES
      LibavSettingsUi.cpp
      ${_libav_settings_hidden}
    APP
    PLUGINS score_plugin_gfx score_lib_process score_plugin_media)
endif()
