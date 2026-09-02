# L3 GPU render + readback tests.
#
# Build an offscreen ISF pipeline (score::gfx graph), render a few frames and
# assert on the read-back RGBA8 pixels. Needs a working QRhi backend; when none
# is available headless the tests SKIP cleanly (CI-safe).
#
# GUI mode: links Qt Gui/Widgets and runs from the build root so the dynamic
# gfx plugin resources are discovered. The offscreen platform is supplied by the
# ctest invocation (QT_QPA_PLATFORM=offscreen).
set(_gfx_src "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-gfx")

# The gfx test targets link the real plug-in. Symbols they need are marked
# SCORE_PLUGIN_GFX_EXPORT at their declaration; recompiling the TUs into the
# tests instead makes every type ODR-violate against the shared library, which
# ASan aborts on before the first assertion runs.
add_library(test_gfx_engine_glue INTERFACE)
target_link_libraries(test_gfx_engine_glue INTERFACE
  score_plugin_gfx score_lib_process ${QT_PREFIX}::Gui)

score_add_test(test_gfx_isf
  SOURCES Gfx.cpp
  GUI
  PLUGINS score_plugin_gfx score_plugin_scenario score_lib_process
  LIBS test_gfx_engine_glue)

# Absolute path to the committed corpus shaders so the tests are self-contained
# and do not depend on the developer's ~/Documents checkout.
target_compile_definitions(test_gfx_isf PRIVATE
  GFX_TEST_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/corpus")

# The process-wide imported-VkDevice cache the shader previews opt into: five
# consecutive "selections" must cost one vkCreateDevice, not five, and must each
# still render real content.
score_add_test(test_gfx_preview_device_cache
  SOURCES PreviewDeviceCache.cpp
  GUI
  PLUGINS score_plugin_gfx score_plugin_scenario score_lib_process
  LIBS test_gfx_engine_glue)

target_compile_definitions(test_gfx_preview_device_cache PRIVATE
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

# Regression guard for the 256-byte disconnected-uniform_input placeholder: a
# std140 block bigger than the guess left every member past 256 B unbound
# (qrhigles2.cpp bounds-checks and skips them; the other backends read out of
# range).
score_add_isf_test(large_uniform_input GfxLargeUniformInput.cpp)
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
# P1-21: the 3D storage image resized at runtime (64^3 -> 32^3 -> 96^3 in one
# session) keeps writing the closed form at each size with no stale slice.
# GREEN on both backends since the runtime resize landed in RenderedCSFNode
# (previously a literal TODO: the allocate-once guard kept the 64^3 texture
# forever and the 32^3/96^3 phases were red everywhere).
score_add_csf_test(image3d_resize CsfImage3dResize.cpp)
# CSF cube + array storage-image compute write, sampled downstream. ISOLATED like
# image3d: the GL layered-storage workaround must cover cube/array (824a08d39),
# else faces/layers past 0 read black on OpenGL. Green on both backends post-fix.
score_add_csf_test(cube_array CsfCubeArray.cpp)
# CSF geometry / storage-buffer producers: parse + port-surface coverage; the
# readback itself SKIPs (no Geometry/Buffer-consuming sink in this fixture yet).
score_add_csf_test(geometry CsfGeometry.cpp)
# CSF geometry / storage-buffer VALUE readback, through the BufferSinkNode
# fixture (score_test/GfxBufferSink.hpp): closes the SKIP CsfGeometry.cpp
# documents -- until this, CSF compute output values could not be read back at
# all. Also carries the P0-2 instancing counter half (instanceHits SSBO).
score_add_csf_test(buffer_readback GfxBufferReadback.cpp)

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

# MRT correctness against a closed-form pattern: GfxOrientation only reads
# attachment 0 of a shader whose ramp is constant along X, so the horizontal axis,
# the remaining attachments and per-attachment identity are all unpinned there.
score_add_gfx_test(mrt_pattern GfxMrtPattern.cpp)

# The raw-raster (RenderPipeline) twin of mrt_pattern: same closed-form
# per-attachment fit, but through RenderedRawRasterPipelineNode's
# initMRTPass / initMRTBlitPasses. GfxRaster.cpp's "two attachments" case
# accepts ANY two non-identical non-blank images, so an attachment-order or
# Y-flip regression in the MRT blit was invisible until this one.
score_add_gfx_test(raw_raster_mrt_pattern GfxRawRasterMrtPattern.cpp)
# P0-7: the topology of 2026/crash-renderpipeline-mrt.score -- a raw-raster
# feeding a second, MRT raw-raster (raster#1 image out -> raster#2 sampler),
# both attachments closed-form, then a sink resize the graph must survive.
score_add_gfx_test(raster_chain_mrt GfxRasterChainMrt.cpp)
# P0-1: a $USER-driven INSTANCE_COUNT changed mid-render (4 -> 9 -> 2 in one
# session, no rebuild) redraws exactly the right number of instance strips,
# each carrying its closed-form buffer identity (G) and draw identity (B) --
# no stale strip from the previous count.
score_add_gfx_test(instance_count_live GfxInstanceCountLive.cpp)
# P1-14 / G17 pin, [!shouldfail]: packCameraUBO ignores camera_projection and
# always builds reverse-Z perspective, so an orthographic or fulldome camera
# renders as a perspective one. Unit-math level: byte-identical UBOs provably
# imply byte-identical frames (no other reader of camera projection exists).
score_add_gfx_test(camera_projection_pin GfxCameraProjectionPin.cpp)

# The same closed-form pattern, but through ISF -> ScreenNode -> swapchain
# readback: the path the golden gate exercises and the only one the analytic
# tests above do not cover. Needs a real windowing system.
score_add_gfx_test(window_pattern GfxWindowPattern.cpp)

# Orientation as a property of the whole path rather than of one node: the same
# picture through 0, 1 and 2 verbatim passthroughs, on every backend, against a
# four-quadrant key that tells a flip from a transpose from a rotation.
score_add_gfx_test(orientation_matrix GfxOrientationMatrix.cpp)

# Random edit sequences against a live render loop: the incremental tests each
# pin one edit, this one looks for the state a graph only reaches after some
# particular ORDER of connects, disconnects and node additions. Deterministic
# from a seed; SCORE_TORTURE_SECONDS raises the default budget for a soak.
score_add_gfx_test(graph_torture GfxGraphTorture.cpp)

# ISOLATED, EXPECTED-RED on OpenGL: the compute storage-image origin. A CSF that
# only imageStore()s is flipped on OpenGL because a raw texel index and a
# bottom-up GL render target disagree, and neither of the two one-line fixes is
# correct (both measured — see the FINDING block in the file). Own target so the
# attributable RED cannot take the green orientation group with it.
score_add_gfx_test(orientation_findings GfxOrientationFindings.cpp)

# The storage-image macros that make a compute generator portable: they correct
# the read and the store together, on every backend except Vulkan -- measured,
# not derived from where each backend puts the framebuffer origin.
score_add_gfx_test(csf_orient_macros GfxCsfOrientMacros.cpp)

# The same gate, read off the baked shader rather than off the picture. The one
# above can only judge a backend this machine brings up, and the wrong gate was
# invisible on both of the two that Linux brings up; this one resolves all five
# backends' branches through QShaderBaker, which cross-compiles to HLSL and MSL
# on any host. No device, no display, so it is a plain test and not a GUI one.
score_add_test(test_gfx_csf_orient_gate
  SOURCES GfxCsfOrientGate.cpp
  PLUGINS score_plugin_gfx score_lib_process
  LIBS test_gfx_engine_glue)
target_compile_definitions(test_gfx_csf_orient_gate PRIVATE
  GFX_TEST_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/corpus")

# The QRhi<->CUDA fence, against a real device on every backend this box brings
# up. The vendor GPU-direct rungs gate on InteropFence::valid() and trust
# waitOnCuda() before letting a peer DMA-read a buffer, so what each backend's
# implementation does when its bridge is absent is a correctness question, not
# a capability one. The no-GPU half (the fallback stub) lives in
# src/plugins/score-plugin-gfx/tests/InteropRingPolicyTest.cpp.
score_add_test(test_gfx_interop_fence
  SOURCES InteropFenceBackends.cpp
  GUI
  PLUGINS score_plugin_gfx score_lib_process
  LIBS test_gfx_engine_glue)

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
# P0-5: the (now-fixed) add-new-output render-pass leak, made COUNTABLE:
# per-cycle QRhi::statistics().allocCount (Vulkan/D3D12), QT_RHI_LEAK_CHECK=1
# teardown accounting (all backends, unix), and the Vulkan validation layer's
# object tracking when present. The incremental_findings pin only asserts the
# added output's pixels appear; green there is not evidence about the leak.
score_add_gfx_test(render_pass_leak GfxRenderPassLeak.cpp)
# P0-4: the (now-fixed) resize-after-incremental-add Vulkan SIGSEGV, re-run in
# a forked child so a future regression is a recorded verdict ("child killed by
# signal 11") instead of a dead ctest run. The parent stays graphics-free
# before forking; the child boots its own GUI app.
score_add_gfx_test(incremental_resize_fork GfxIncrementalResizeFork.cpp)

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
# P1-13: the FORMAT half of the surgical rt_changed path (rt_changed covers
# size): RGBA8 -> RGBA16F mid-render, oracle = a 2.0 write surviving the
# round trip only in float (halver shader: red 128 clipped vs 255 preserved).
score_add_gfx_test(rt_format_changed GfxRtFormatChanged.cpp)
score_add_gfx_test(node_removal    GfxNodeRemoval.cpp)
# P0-9: removing a CSF geometry producer (and then the geometry filter) while
# the render loop is live: the raster consumer keeps rendering (uniform clear
# or last valid frame, stable across 12 post-removal frames) and every
# transitively-unreachable upstream node's renderedNodes drains to 0 --
# GfxNodeRemoval's structural invariant applied to the geometry chain.
score_add_gfx_test(geometry_producer_removal GfxGeometryProducerRemoval.cpp)
# batch_lifetime: a pending initial resource-update batch must be submitted
# before graph mutations destroy resources it names (pre-fix: UAF inside
# QRhi::enqueueResourceUpdates on the next frame — the GL/D3D11/Vulkan crash
# triplet from the corpus run). ISOLATED so the pre-fix crash is attributable.
score_add_gfx_test(batch_lifetime  GfxBatchLifetime.cpp)
# P0-8: the geometry-buffer sibling of batch_lifetime -- a CSF geometry
# buffer reallocated in the same frame the raster rebinds it renders one of
# the two closed forms (old size / new size), never garbage; plus the
# teardown-with-pending-initial-batch scenario.
score_add_gfx_test(geometry_buffer_rebind GfxGeometryBufferRebind.cpp)
score_add_gfx_test(depth_sampler   GfxDepthSampler.cpp)
score_add_gfx_test(storage_visibility GfxStorageVisibility.cpp)
score_add_gfx_test(multiview       GfxMultiview.cpp)
score_add_gfx_test(dynamic_slot    GfxDynamicSlot.cpp)
# The three scene-filter nodes added by 45d1202327, which had zero references
# anywhere under tests/ while being user-facing processes.
score_add_gfx_test(scene_filter_nodes SceneFilterNodes.cpp)
# P1-4: N producers into ScenePreprocessor's single inlet merge additively and
# memoize on {state.get(), version}: idle frames re-run merge_scenes ZERO
# times (pointer identity), a bump re-merges exactly once, one contributor
# short-circuits to a pointer copy, an env-only rootless producer is kept.
score_add_gfx_test(scene_merge_memo SceneMergeMemo.cpp)

# The Crousti CPU node path (oscr::ProcessModel + oscr::GfxNode). Its renderer
# lives in Crousti/CpuFilterNode.hpp, which nothing in a default ctest run
# touched: the only driver was the tests-scene sweep, and that needs an external
# corpus. The threedim sources are compiled in directly because they are
# hidden-visibility inside the plug-in, as the scene sweep does.
if(TARGET score_plugin_threedim AND TARGET score_plugin_avnd)
  set(_3d_c "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-threedim/Threedim")
  score_plugin_hidden_sources(_crousti_hidden
      "${_3d_c}/Primitive.cpp"
      "${_3d_c}/Camera.cpp"
      "${_3d_c}/Transform3D.cpp"
      "${_3d_c}/SceneGroup.cpp"
      "${_3d_c}/CameraArray.cpp")
  score_add_test(test_gfx_crousti_cpu_nodes
    SOURCES
      CroustiCpuNodes.cpp
      ${_crousti_hidden}
    GUI
    PLUGINS score_plugin_gfx score_plugin_avnd score_plugin_scenario score_lib_process
    LIBS test_gfx_engine_glue)
  target_compile_definitions(test_gfx_crousti_cpu_nodes PRIVATE
    GFX_TEST_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/corpus")
  target_include_directories(test_gfx_crousti_cpu_nodes SYSTEM PRIVATE
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-threedim"
    "${SCORE_ROOT_BINARY_DIR}/src/plugins/score-plugin-threedim"
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-gfx"
    "${SCORE_ROOT_BINARY_DIR}/src/plugins/score-plugin-gfx"
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-avnd"
    "${SCORE_ROOT_BINARY_DIR}/src/plugins/score-plugin-avnd"
    $<TARGET_PROPERTY:score_plugin_threedim,INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:score_plugin_avnd,INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:score_plugin_gfx,INCLUDE_DIRECTORIES>)
endif()
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

# What DMACaptureInputNode::Renderer::init() must report back to its vendor
# backend once the capture ladder has resolved: whether the sync group the
# backend offered actually engaged, and whether the whole-frame external-image
# rung did. Neither is observable from the backend's side and neither shows in a
# rendered frame -- a declined group is a rare tear, a declined external rung is
# a black frame from a path reporting itself engaged. Fake backend + fake rungs;
# the QRhi, the RenderList and the node are real, because init() only runs
# against a live render list.
# Registered by hand rather than through score_add_gfx_test: DMACaptureInputNode's
# backend seam is spelled in Video::ImageFormat / VideoMetadata, so the test needs
# score_plugin_media's headers on top of the usual gfx set.
score_add_test(test_gfx_dmacapture_ladder
  SOURCES DmaCaptureLadderContract.cpp
  GUI
  PLUGINS score_plugin_gfx score_plugin_media score_plugin_scenario score_lib_process
  LIBS test_gfx_engine_glue)

# What the GPU Bayer demosaic reconstructs, per CFA order and bit depth, on the
# real capture path (fake backend, real CpuStagedCapture + wire decoder).
score_add_test(test_gfx_bayer_demosaic
  SOURCES BayerDemosaic.cpp
  GUI
  PLUGINS score_plugin_gfx score_plugin_media score_plugin_scenario score_lib_process
  LIBS test_gfx_engine_glue)
# -----------------------------------------------------------------------------
# What the packed / high-bit-depth GPU video decoders put on screen.
#
# VideoDecoderFactoryTest proves the AVPixelFormat -> decoder-class choice and
# tests/integration/video-decode-correctness.sh proves real pixels, but only for
# formats ffmpeg can round-trip through a container that preserves the pix_fmt.
# y210 / xv30 / vuya / vuyx / yuva444p12 have no such container, so their unpack
# and colour conversion were never asserted. Here the frames are synthesised
# byte by byte (layouts and expected RGB cross-checked against ffmpeg swscale)
# and pushed through a real CameraNode -> VideoNodeRenderer ->
# createGPUVideoDecoder -> offscreen readback, on every available RHI backend.
if(TARGET score_plugin_media)
  score_add_test(test_gfx_video_decoder_pixels
    SOURCES VideoDecoderPixels.cpp
    GUI
    PLUGINS score_plugin_gfx score_plugin_media score_plugin_scenario
            score_lib_process
    LIBS test_gfx_engine_glue avutil)
endif()

# Render-level tests for the Images process: pixel assertions across the
# timing permutations of image-list delivery vs renderer creation (the
# image-does-not-show-until-a-viewport-rebuild class of bugs).
score_add_test(test_gfx_images_render
  SOURCES GfxImagesRender.cpp
  GUI
  PLUGINS score_plugin_gfx score_plugin_scenario score_lib_process
  LIBS test_gfx_engine_glue)

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

# Randomised sequences of the window disruptions a user can inflict -- hide,
# re-show, resize, render-size override, full screen -- across all three output
# modes and both clocks. The paths that set m_running / m_hasSwapChain /
# m_notExposed / m_closed are not symmetric with the paths that clear them, so
# a sequence finds combinations the single-operation cases above do not.
score_add_gfx_test(window_output_torture WindowOutputTorture.cpp)

# Resizing and full-screening a presented output. A resize takes either the
# swapchain-sized-target fast path or a full render-list rebuild, and the two
# exercise entirely different code; full screen changes the surface size from
# the platform side. Reported as a crash on a document of compute shaders and
# point-cloud geometry.
score_add_gfx_test(window_resize_torture WindowResizeTorture.cpp)

# Synthetic, single-feature shaders: each isolates one documented capability so a
# failure names the feature rather than a corpus file that mixes ten of them.
score_add_gfx_test(synthetic_features SyntheticFeatures.cpp)

# Regression guards on the ScreenNode <-> Graph boundary: a live swapchain-flag
# toggle must release the Graph's RenderList before freeing the QRhi it was
# built against, and a window resize must not discard the render-size override.
# Own target so a crash in either cannot take the two suites above with it.
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

  # Every SHIPPED libav preset, in both directions: an input preset through the
  # real device, an output preset through the real Gfx::LibavEncoder, and what
  # the encoder wrote decoded back and compared against the known master.
  # Registered as a media test in tests/hardware terms (below), because the
  # master comes from the --matrix provisioning.
  score_add_test(test_gfx_libav_presets
    SOURCES
      LibavPresets.cpp
      ${_libav_settings_hidden}
    APP
    PLUGINS score_plugin_gfx score_lib_process score_plugin_media
            score_plugin_deviceexplorer
    LIBS avutil avformat avcodec swscale
    NO_CTEST)
endif()

if(TARGET test_gfx_libav_presets)
  score_add_media_test(
    NAME       Media_LibavPresets
    EXECUTABLE test_gfx_libav_presets
    MATRIX
    TIMEOUT    3600
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# -----------------------------------------------------------------------------
# The ossia device trees of the window output device: Gfx/Window/WindowDevice.hpp
# and Gfx/Window/MultiWindowDevice.hpp, both 0%. This is where every window
# property becomes a remote-controllable address and where the input events
# travel back out. Built through the real protocol factory on a real document,
# so it needs a display: GUI, and it SKIPs without one.
score_add_gfx_test(window_device_tree WindowDeviceTree.cpp)

# The window output as the application assembles it: a real WindowDevice in a
# real document, with GfxContext between the window and the graph. The rigs
# above drive a ScreenNode directly and pump frames themselves, which is why a
# window that has stopped presenting in the running app still reads as healthy.
score_add_gfx_test(window_device_lifecycle WindowDeviceLifecycle.cpp)
target_link_libraries(test_gfx_window_device_lifecycle PRIVATE score_plugin_deviceexplorer score_plugin_engine score_plugin_js)
target_link_libraries(test_gfx_window_device_tree PRIVATE score_plugin_deviceexplorer)


# -----------------------------------------------------------------------------
# The GStreamer input device (Gfx/GStreamer/GStreamerDevice.cpp, 1010 lines at
# 4%; GStreamerLoader.hpp, 0%). The device runs its OWN pipeline, so
# `videotestsrc` is producer and reference at once and no external process is
# needed: gst_parse_launch -> appsink classification -> FrameQueue -> AVFrame is
# assertable on the CPU by scaling one decoded frame down to a single RGBA pixel.
#
# Registered in tests/hardware (score_add_media_test) rather than here: the
# runner REQUIRES a working GStreamer, so an absent one is a failure instead of
# a silently skipped suite. NO_CTEST keeps it from also being registered here
# with none of that guaranteed.
if(TARGET score_plugin_media)
  score_plugin_hidden_sources(_gstreamer_device_hidden
      "${_gfx_src}/Gfx/GStreamer/GStreamerDevice.cpp"
      "${_gfx_src}/Gfx/GStreamer/GStreamerOutputDevice.cpp")
  score_add_test(test_gfx_gstreamer_device
    SOURCES
      GStreamerDeviceRender.cpp
      ${_gstreamer_device_hidden}
    APP
    PLUGINS score_plugin_gfx score_lib_process score_plugin_media
            score_plugin_deviceexplorer
    LIBS avutil swscale
    NO_CTEST)

  # The picture sweep and the lifecycle matrix, in their own binary: they walk
  # 25 pixel formats at two geometries through a real file source, which is
  # minutes rather than the seconds the colour sweep above costs. Keeping them
  # apart means a format regression and a teardown hang are separate ctest
  # entries with separate timeouts.
  score_add_test(test_gfx_gstreamer_matrix
    SOURCES
      GStreamerStreamingMatrix.cpp
      ${_gstreamer_device_hidden}
    APP
    PLUGINS score_plugin_gfx score_lib_process score_plugin_media
            score_plugin_deviceexplorer
    LIBS avutil avformat swscale
    NO_CTEST)

  # Every SHIPPED preset, enumerated from the factory rather than copied, so a
  # preset that is added or edited is covered without touching this list.
  score_add_test(test_gfx_gstreamer_presets
    SOURCES
      GStreamerPresets.cpp
      ${_gstreamer_device_hidden}
    APP
    PLUGINS score_plugin_gfx score_lib_process score_plugin_media
            score_plugin_deviceexplorer
    LIBS avutil avformat swscale
    NO_CTEST)
endif()

# --- GStreamer input device (score-plugin-gfx) ------------------------------
# Registered here rather than in tests/hardware because tests/gfx is added
# after it, so the target does not exist yet there. The runner
# REQUIRES gstreamer, so a host without it fails instead of silently dropping
# the only coverage GStreamerDevice.cpp has. No --video is needed -- the device
# runs videotestsrc itself -- but the runner's gst-inspect check is exactly the
# precondition this harness has.
#
# Deliberately NOT run with SCORE_SANITIZE_SKIP_CHECKS: this harness pumps the
# event loop for several seconds, which is exactly what used to deliver
# PM::PluginSettingsModel::firstTimeLibraryDownload()'s modal QMessageBox and
# wedge the run under the offscreen QPA. Leaving the escape hatch off keeps
# that regression visible here as a hang instead of hiding it.
if(TARGET test_gfx_gstreamer_device)
  score_add_media_test(
    NAME       Media_GStreamerDevice
    EXECUTABLE test_gfx_gstreamer_device
    GSTREAMER
    TIMEOUT    600
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# --- GStreamer picture sweep + device lifecycle -----------------------------
# --matrix provides the known-pixel master in a container GStreamer can demux,
# at an even and an odd geometry. The sweep pushes that picture through every
# mapped pixel format and asserts the pixels that came back; the lifecycle half
# bounds every create/start/stop/destroy order by a wall-clock budget, so a
# teardown that wedges fails its own case instead of the whole run.
if(TARGET test_gfx_gstreamer_matrix)
  score_add_media_test(
    NAME       Media_GStreamerMatrix
    EXECUTABLE test_gfx_gstreamer_matrix
    GSTREAMER
    MATRIX
    TIMEOUT    900
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# --- the shipped presets ----------------------------------------------------
# Drives every preset the protocol enumerates: the file and network receivers
# against the --matrix master, videotestsrc against a gst-launch dump of itself,
# and everything else to the furthest point this host allows, with the reason it
# stopped there recorded by name.
if(TARGET test_gfx_gstreamer_presets)
  score_add_media_test(
    NAME       Media_GStreamerPresets
    EXECUTABLE test_gfx_gstreamer_presets
    GSTREAMER
    MATRIX
    TIMEOUT    3600
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# -----------------------------------------------------------------------------
# The Gfx PROCESS layer: the seven Process::ProcessModel subclasses of
# Gfx/{Images,Filter,GeometryFilter,CSF,VSA,Video,Text}, the Change* commands a
# live edit drives through them, and the library/drop handlers that decide which
# file becomes which process. Everything above builds score::gfx nodes by hand;
# none of it ever constructs a process model, so this whole layer had no test.
#
# GUI, not APP: these run undo/redo of commands that save and restore ports and
# cables (Scenario::EditScript), which goes through the document presenter. They
# also deliberately do NOT force the offscreen QPA platform -- offscreen has no
# GL, and a test in this directory that later grows a render assertion would
# then run on the Null RHI backend and pass vacuously.
#
#   *_shader_commands  ChangeShader / ChangeGeometryShader / ChangeCSF /
#                      ChangeVSAShader: port surface after an edit, after an
#                      UNPARSEABLE edit, and across chained undo/redo.
#   *_images           Gfx/Images: index domain driven by the files on disk,
#                      unreadable / corrupt / unsupported entries, the shared
#                      ImageCache refcount, and the list-editor widget's
#                      undoable write-back.
#   *_video_text       Gfx/Video property commands + a real clip's duration
#                      (SKIPs without SCORE_TEST_MEDIA_DIR or ffmpeg), and the
#                      8-inlet contract Gfx/Text's executor hard-codes.
#   *_library          Drop and library handlers by uuid through the real
#                      registries, incl. the two Shadertoy URL guards WITHOUT
#                      touching the network.
function(score_add_gfx_process_test name source)
  score_add_test(test_gfx_process_${name}
    SOURCES ${source}
    GUI
    PLUGINS score_plugin_gfx score_plugin_scenario score_plugin_library
            score_plugin_media score_lib_process score_lib_device
    LIBS test_gfx_engine_glue)
  target_compile_definitions(test_gfx_process_${name} PRIVATE
    GFX_TEST_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/corpus")
endfunction()

score_add_gfx_process_test(shader_commands GfxShaderCommands.cpp)
score_add_gfx_process_test(images          GfxImagesProcess.cpp)
score_add_gfx_process_test(video_text      GfxVideoTextProcess.cpp)
score_add_gfx_process_test(library         GfxProcessLibrary.cpp)

# OPEN-7 pin, its own single-test executable: an empty nodelist mime payload
# SIGABRTs in rapidjson today. The case asserts the CORRECT behaviour (child
# process survives, zero drops) and is [!shouldfail]; the drop runs in a
# forked child because CTest's WILL_FAIL does not invert abnormal termination.
score_add_gfx_process_test(drop_empty_nodelist GfxDropEmptyNodelistAbort.cpp)

# OPEN-3: the run_commands full-rebuild flag must not latch across frames and
# downgrade the next incremental edge diff. Drives the real document
# GfxContext through GfxExecutionAction, observed via SCORE_GFX_TRACE.
score_add_gfx_process_test(edge_consume_latch GfxEdgeConsumeLatch.cpp)

# OPEN-4: within one incremental frame, every renderer (initState) precedes
# every pass creation. Observed via the GFX-RECONCILE / GFX-PASS trace lines.
score_add_gfx_process_test(renderers_before_passes GfxRenderersBeforePasses.cpp)

# -----------------------------------------------------------------------------
# Gfx/Spout (Windows/x86_64) and Gfx/Syphon (macOS). Neither compiles on Linux,
# so this target must build EVERYWHERE and change what it asserts instead of
# whether it exists: on the supporting platform it drives the four protocol
# factories, everywhere else it asserts they are NOT registered and then SKIPs
# with the reason. The two conditions below must stay identical to the ones in
# src/plugins/score-plugin-gfx/CMakeLists.txt that set SCORE_HAS_SPOUT / the
# APPLE branch -- that is the only coupling, and the third test case in the file
# fails loudly if they ever drift apart.
score_add_test(test_gfx_shared_texture_io
  SOURCES GfxSharedTextureIO.cpp
  GUI
  PLUGINS score_plugin_gfx score_lib_device score_lib_process)
if(WIN32 AND "${CMAKE_SYSTEM_PROCESSOR}" MATCHES "(x86_64|AMD64)")
  target_compile_definitions(test_gfx_shared_texture_io PRIVATE GFX_TEST_HAS_SPOUT=1)
endif()
if(APPLE)
  target_compile_definitions(test_gfx_shared_texture_io PRIVATE GFX_TEST_HAS_SYPHON=1)
endif()

# Hidden playback-throughput benchmark for the Direct video path; see the
# file header for usage (needs VIDEO_BENCH_FILES; VIDEO_BENCH_HWDEC selects
# the hardware decoding method by its settings name). NO_CTEST: every case
# in the binary is a hidden [.tag] benchmark that needs media files handed
# in by environment, so a bare ctest entry can only ever fail with
# Catch2's 'No tests ran'.
score_add_test(test_gfx_video_direct_bench
  SOURCES VideoDirectPlaybackBench.cpp
  GUI
  PLUGINS score_plugin_gfx score_plugin_media score_plugin_scenario score_lib_process
  LIBS test_gfx_engine_glue avcodec avformat avutil
  NO_CTEST)
