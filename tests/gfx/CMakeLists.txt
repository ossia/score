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

# Regression guard for the unbound top-level AUXILIARY placeholder: created and
# never written, so on a RenderList rebuild it read back recycled device memory.
# Shaders read those placeholders as sentinels (classic_pbr_openpbr gates its
# clustered-lighting and volumetric paths on cluster_config.cluster_x == 0u and
# then indexes 16-byte placeholder SSBOs with an id derived from that grid), so
# a nonzero placeholder is a multi-gigabyte out-of-bounds fragment-stage read.
score_add_isf_test(aux_placeholder_zeroed GfxAuxPlaceholderZeroed.cpp)
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
# The 3D storage image resized at runtime (64^3 -> 32^3 -> 96^3 in one run)
# keeps writing the closed form at each size with no stale slice.
# Green on both backends.
score_add_csf_test(image3d_resize CsfImage3dResize.cpp)
# CSF cube + array storage-image compute write, sampled downstream. ISOLATED like
# image3d: the GL layered-storage workaround must cover cube/array, else
# faces/layers past 0 read black on OpenGL. Green on both backends.
score_add_csf_test(cube_array CsfCubeArray.cpp)
# CSF geometry / storage-buffer producers: parse + port-surface coverage; the
# readback itself SKIPs (no Geometry/Buffer-consuming sink in this fixture yet).
score_add_csf_test(geometry CsfGeometry.cpp)
# CSF geometry / storage-buffer VALUE readback, through the BufferSinkNode
# fixture (score_test/GfxBufferSink.hpp): closes the SKIP CsfGeometry.cpp
# documents. Also carries the instancing counter half (instanceHits SSBO).
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

# Which way up each video output hands its pixels over: the InvertYRenderer
# readback that Libav/GStreamer/Shmdata/Sh4lt/NDI/PipeWire share, against the
# GPU encoders that Libav and GStreamer use for yuv420p, nv12 and friends.
# The encoders include GPUVideoDecoder.hpp, which needs score_plugin_media's
# Video/VideoInterface.hpp; the end-to-end cell drives the real
# Gfx::LibavEncoderNode, whose symbols the shared plug-in hides.
score_plugin_hidden_sources(_orient_libav_hidden
    "${_gfx_src}/Gfx/Libav/LibavEncoder.cpp"
    "${_gfx_src}/Gfx/Libav/LibavEncoderNode.cpp")
score_add_test(test_gfx_video_output_orientation
  SOURCES GfxVideoOutputOrientation.cpp ${_orient_libav_hidden}
  GUI
  PLUGINS score_plugin_gfx score_plugin_media score_plugin_scenario score_lib_process
  LIBS test_gfx_engine_glue avutil avformat avcodec swscale)
target_compile_definitions(test_gfx_video_output_orientation PRIVATE
  GFX_TEST_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/corpus")

# Syphon shares a texture instead of reading pixels back, so none of the flips
# the readback outputs apply are in play: whether a client sees the picture the
# right way up can only be answered by being one. macOS only, and it SKIPs
# without a window-server session because a Syphon server needs one.
if(APPLE)
  score_plugin_hidden_sources(_syphon_orient_hidden
      "${_gfx_src}/Gfx/Syphon/SyphonOutput.mm"
      "${_gfx_src}/Gfx/Syphon/SyphonInput.mm")
  score_add_test(test_gfx_syphon_orientation
    SOURCES GfxSyphonOrientation.mm
    GUI
    PLUGINS score_plugin_gfx score_plugin_scenario score_lib_process
    LIBS test_gfx_engine_glue Syphon)
endif()

# The multi-window soft-edge blend ramp: the pixels past its own edge stay
# dark. Calling pow() on the negative base an extrapolated edge coordinate
# produces is undefined, and the drivers that answer NaN lit them fully.
score_add_gfx_test(soft_edge_blend SoftEdgeBlend.cpp)

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
# The topology of 2026/crash-renderpipeline-mrt.score -- a raw-raster
# feeding a second, MRT raw-raster (raster#1 image out -> raster#2 sampler),
# both attachments closed-form, then a sink resize the graph must survive.
score_add_gfx_test(raster_chain_mrt GfxRasterChainMrt.cpp)
# A $USER-driven INSTANCE_COUNT changed mid-render (4 -> 9 -> 2 in one run,
# no rebuild) redraws exactly the right number of instance strips,
# each carrying its closed-form buffer identity (G) and draw identity (B) --
# no stale strip from the previous count.
score_add_gfx_test(instance_count_live GfxInstanceCountLive.cpp)
# The indirect draw takes its count from the GPU-written buffer. CPU
# instances pinned at 8; only the compute-written indirect command can make
# the frame show 3 then 6 strips. SKIPs where neither DrawIndirect nor
# ReadBackNonUniformBuffer exists (the engine only degrades there).
score_add_gfx_test(indirect_draw_count GfxIndirectDrawCount.cpp)
score_add_gfx_test(indirect_fallback_ladder GfxIndirectFallbackLadder.cpp)
score_add_gfx_test(zero_count_draw GfxZeroCountDraw.cpp)
score_add_gfx_test(gpu_driven_lights GfxGpuDrivenLights.cpp)
score_add_gfx_test(y_orientation GfxYOrientation.cpp)
score_add_gfx_test(indirect_draw_bench GfxIndirectDrawBench.cpp)

# Depth-clear polarity: the only coverage of it in the suite.
score_add_gfx_test(review_render_targets_depth GfxReviewRenderTargetsDepth.cpp)

# Render-target cases 01-07. Each case states a contract of its own, and the
# red ones are declared in cmake/ScoreExpectedRedGuard.cmake with a mechanism
# rather than a citation.
score_add_gfx_test(review_render_targets GfxReviewRenderTargets.cpp)

# A one-element vertex fallback bound as a PerInstance
# stream, drawn with two instances. Uses the production VertexFallbackPool and a
# real QRhi readback, so it measures the binding the engine actually emits.
score_add_gfx_test(review_scene_fallback GfxReviewSceneFallback.cpp)
score_add_gfx_test(interior_orphan_binding GfxInteriorOrphanBinding.cpp)
score_add_gfx_test(d3d11_vertex_binding_cap GfxD3D11VertexBindingCap.cpp)
# One resolver behind both entry points: it clears the caller's plan on entry,
# and the strict and fallback-aware calls agree on layout and plan.
score_add_gfx_test(unified_binding_resolver GfxUnifiedBindingResolver.cpp)

# Indirect-draw firstInstance ABI and layered
# indirect dispatch, each driven through the GPU rung AND the CPU fallback rung
# so the two are forced to agree.
score_add_gfx_test(review_draw_dispatch_edges GfxReviewDrawDispatchEdges.cpp)
score_add_gfx_test(review_indirect_standalone GfxReviewIndirectStandalone.cpp)

# Large-allocation limits: what the GPU and QRhi actually permit, and what the
# engine does at the int64 -> quint32 boundary where a size silently narrows.
score_add_gfx_test(buffer_limits GfxBufferLimits.cpp)

# Graph reconciliation, incremental removal, rebuild before the first frame,
# and incremental target mip flags.
score_add_gfx_test(review_graph_lifecycle GfxReviewGraphLifecycle.cpp)
# The cubemap render target's six faces, against the self-declared
# oracle of 2026/test-cubemap-output.score (+X red, -X cyan, +Y green,
# -Y magenta, +Z blue, -Z yellow), through the MULTIVIEW-6 + CUBEMAP
# array-then-copy shim; samplerCube viewer probes each face exactly once.
# GL follows GfxMultiview's headless-layered-raster precedent (structural
# half + SUCCEED with the reason); Null asserts the shim selection only.
score_add_gfx_test(cubemap_six_faces GfxCubemapSixFaces.cpp)
score_add_gfx_test(cube_depth_attachment_size GfxCubeDepthAttachmentSize.cpp)
score_add_gfx_test(provoking_vertex GfxProvokingVertex.cpp)
score_add_gfx_test(msl_reserved_names GfxMslReservedNames.cpp)
score_add_gfx_test(points_default_size GfxPointsDefaultSize.cpp)
# Pin, [!shouldfail]: packCameraUBO ignores camera_projection and
# always builds reverse-Z perspective, so an orthographic or fulldome camera
# renders as a perspective one. Unit-math level: byte-identical UBOs provably
# imply byte-identical frames (no other reader of camera projection exists).
score_add_gfx_test(camera_projection_pin GfxCameraProjectionPin.cpp)
# The Null backend refuses rather than pretends. Null accepts every
# command and rasterizes nothing, so a pixel case run against it would either go
# red naming the FIXTURE ("readback ... empty/short") or, for a case expecting a
# black pixel, go green against a buffer nothing drew. score_test/Gfx.hpp reports
# SKIP for Null, with an opt-in (allowNullBackend) for the structural halves that
# want it. This case pins the selection logic, the SKIP verdict shape, and that
# opting in still yields no pixels. GPU-independent: it runs, and asserts the
# same thing, on any backend.
score_add_gfx_test(null_backend_refuses GfxNullBackendRefuses.cpp)

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
# correct (see the FINDING block in the file). Own target so the attributable
# RED cannot take the green orientation group with it.
score_add_gfx_test(orientation_findings GfxOrientationFindings.cpp)

# The storage-image macros that make a compute generator portable: they correct
# the read and the store together, on every backend except Vulkan -- a rule
# that does not follow from where each backend puts the framebuffer origin.
score_add_gfx_test(csf_orient_macros GfxCsfOrientMacros.cpp)

# The shader-include search path -- registration mechanism tested
# directly (temp library root, header resolution + per-call path recompute),
# the missing-include failure is DIAGNOSABLE by name (no pin needed: the
# product already errors "Shader include not found" with the searched paths),
# failures are not cached, and the academy openpbr.h positive half SKIPs with
# enablement instructions when the addon's package is absent. APP-booted,
# GPU-less (Vulkan bake path).
score_add_test(test_gfx_shader_include_path
  SOURCES GfxShaderIncludePath.cpp
  APP
  PLUGINS score_plugin_gfx score_plugin_library
  LIBS test_gfx_engine_glue)
target_include_directories(test_gfx_shader_include_path PRIVATE
  "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-library")

# Collapsing a run of material-texture auxiliaries into one array binding plus
# a shared sampler. Parser-level, so no device and no display: the assertions
# are on the descriptor, on the emitted GLSL, and on whether the separated
# texture/sampler form still bakes for all five backends.
score_add_test(test_gfx_aux_texture_ladder
  SOURCES GfxAuxTextureLadder.cpp
  PLUGINS score_plugin_gfx score_lib_process
  LIBS test_gfx_engine_glue)
# Static builds interlink every plugin through the global registry, so linking
# score_plugin_gfx alone leaves the other plugins' registration symbols
# unresolved. score_add_test only links the whole list for GUI/APP tests, and
# this one is neither. No-op on dynamic-plugin builds.
score_link_plugins_for_static(test_gfx_aux_texture_ladder)
target_compile_definitions(test_gfx_aux_texture_ladder PRIVATE
  GFX_TEST_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/corpus")

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

# Writable cubemaps on Direct3D. HLSL has no RWTextureCube in any shader model
# and SPIRV-Cross aborts the translation rather than approximating one, so a
# cube storage image cannot bake for D3D11 and D3D12 -- and only for them. The
# emitter declares it instead as the 2D-array VIEW the D3D UAV already is, gated
# on QSHADER_HLSL, with no change to the shader the author writes; this pins both
# halves (it bakes everywhere; the API is unchanged). Same no-device shape as
# csf_orient_gate: QShaderBaker cross-compiles to HLSL on any host. The picture
# itself is gfx_csf_cube_array's job.
score_add_test(test_gfx_cube_image_hlsl
  SOURCES GfxCubeImageHlsl.cpp
  PLUGINS score_plugin_gfx score_lib_process
  LIBS test_gfx_engine_glue)
target_compile_definitions(test_gfx_cube_image_hlsl PRIVATE
  GFX_TEST_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/corpus")

# The same idea, widened from one shader to the whole corpus: EVERY shader in
# corpus/, of every kind, generated through the path the application uses and
# then baked for GLSL, SPIR-V, HLSL and MSL. QShaderBaker cross-compiles on any
# host, so the two targets a Linux ctest run never otherwise asks for -- HLSL
# and MSL -- are asked for here, on this machine, with no device and no display.
#
# It exists because of this failure shape: a duplicated `#define isf_FragCoord`
# with mismatched QSHADER_* guards agrees on SPIR-V and GLSL and disagrees on
# HLSL and MSL, so under setPerTargetCompilation(true) glslang rejects every
# raw-raster fragment stage ("Macro redefined; different substitutions") and no
# raw-raster ISF shader compiles on Direct3D or Metal. Catching that otherwise
# takes a four-backend Windows sweep; this finds it in an ordinary ctest run.
#
# APP rather than plain, because ProgramCache::get reads Gfx::Settings::Model
# out of score::AppContext() (ShaderProgram.cpp:534); GPU-less all the same, so
# not GUI.
score_add_test(test_gfx_shader_corpus_targets
  SOURCES ShaderCorpusTargets.cpp
  APP
  PLUGINS score_plugin_gfx score_lib_process
  LIBS test_gfx_engine_glue)
target_compile_definitions(test_gfx_shader_corpus_targets PRIVATE
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
# The add-new-output render-pass leak, made COUNTABLE:
# per-cycle QRhi::statistics().allocCount (Vulkan/D3D12), QT_RHI_LEAK_CHECK=1
# teardown accounting (all backends, unix), and the Vulkan validation layer's
# object tracking when present. The incremental_findings pin only asserts the
# added output's pixels appear; green there is not evidence about the leak.
score_add_gfx_test(render_pass_leak GfxRenderPassLeak.cpp)
# A raw-Vulkan vkCmdCopyBuffer recorded through beginExternal() is invisible to
# QRhi's own barrier tracking, so score's batch barrier is the only thing that
# can order it against the staging copies of the QRhiResourceUpdateBatch
# submitted earlier in the same command buffer. A barrier naming only the
# compute stage leaves growBuf's full-extent zero-clear of
# ScenePreprocessor::inst.attribs and issuePendingGpuCopies' per-instance writes
# into the same buffer as a transfer WRITE_AFTER_WRITE.
# Runs the engine path under synchronization validation, with a deliberately
# unsynchronised copy as the positive control so a silent layer SKIPs instead
# of going green.
score_add_gfx_test(buffer_copy_barrier_sync GfxBufferCopyBarrierSync.cpp)
# The resize-after-incremental-add Vulkan SIGSEGV, re-run in
# a forked child so a future regression is a recorded verdict ("child killed by
# signal 11") instead of a dead ctest run. The parent stays graphics-free
# before forking; the child boots its own GUI app.
score_add_gfx_test(incremental_resize_fork GfxIncrementalResizeFork.cpp)

# An incremental edge add that lands between a fast-path sink resize and the
# rebuild that resize defers must not build its pass against the render pass
# descriptor BackgroundNode::resize() has already freed (SIGSEGV in
# GenericNodeRenderer::addOutputPass on the corpus document test-js-ui.score).
# Its own target: the pin compares the handles the sink renderer hands out
# against the sink's live ones, which must not be perturbed by a neighbour's
# QRhi teardown.
score_add_gfx_test(sink_resize_stale_pass GfxSinkResizeStalePass.cpp)

# -----------------------------------------------------------------------------
# ISOLATED regression guards for the split/gfx-scene engine fixes.
# Each is its OWN executable so a crash-on-failure on the pre-fix engine (the
# RED-proof) cannot perturb its neighbours — mirroring test_gfx_isf_mrt_persistent
# / test_gfx_csf_image3d / test_gfx_incremental_findings.
#
#   rt_changed     surgical rt_changed must rebuild an intermediate node's
#                  own output passes.
#   node_removal   node removal must release transitively-unreachable
#                  upstream renderers.
#   storage_vis    storage-visibility filter drift.
#   depth_sampler  sampler-index miscount across SamplableDepth.
#   multiview      multiview UBO binding collision.
#   csf_cube_array GL layered rebind covers cube/array storage images.
score_add_gfx_test(rt_changed      GfxRtChanged.cpp)
# The FORMAT half of the surgical rt_changed path (rt_changed covers
# size): RGBA8 -> RGBA16F mid-render, oracle = a 2.0 write surviving the
# round trip only in float (halver shader: red 128 clipped vs 255 preserved).
score_add_gfx_test(rt_format_changed GfxRtFormatChanged.cpp)
score_add_gfx_test(node_removal    GfxNodeRemoval.cpp)
# Removing a CSF geometry producer (and then the geometry filter) while
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
# The geometry-buffer sibling of batch_lifetime -- a CSF geometry
# buffer reallocated in the same frame the raster rebinds it renders one of
# the two closed forms (old size / new size), never garbage; plus the
# teardown-with-pending-initial-batch scenario.
score_add_gfx_test(geometry_buffer_rebind GfxGeometryBufferRebind.cpp)
# A read_write geometry attribute read across indices must observe a consistent
# previous state, by snapshot (non-feedback) or by ping-pong swap (feedback).
score_add_gfx_test(csf_readwrite_state GfxCsfReadWriteState.cpp)
# State must travel around a Delayed* geometry edge and advance.
score_add_gfx_test(csf_feedback_loop GfxCsfFeedbackLoop.cpp)
score_add_gfx_test(buffer_adoption_lifetime GfxBufferAdoptionLifetime.cpp)
score_add_gfx_test(raw_raster_buffer_input GfxRawRasterBufferInput.cpp)
score_add_gfx_test(raw_raster_aux_count GfxRawRasterAuxCount.cpp)
score_add_gfx_test(depth_sampler   GfxDepthSampler.cpp)
score_add_gfx_test(storage_visibility GfxStorageVisibility.cpp)
score_add_gfx_test(multiview       GfxMultiview.cpp)
score_add_gfx_test(dynamic_slot    GfxDynamicSlot.cpp)
# The three scene-filter nodes, which had no reference anywhere under tests/
# while being user-facing processes.
score_add_gfx_test(scene_filter_nodes SceneFilterNodes.cpp)
# N producers into ScenePreprocessor's single inlet merge additively and
# memoize on {state.get(), version}: idle frames re-run merge_scenes ZERO
# times (pointer identity), a bump re-merges exactly once, one contributor
# short-circuits to a pointer copy, an env-only rootless producer is kept.
score_add_gfx_test(scene_merge_memo SceneMergeMemo.cpp)
# The unified-MDI geometry published nine vertex bindings, one past
# QD3D11CommandBuffer::MAX_VERTEX_BUFFER_BINDING_COUNT, so a shader reading
# every stream lost its ninth on D3D11 without an error. translation and
# instance_color0 now share one interleaved 32-byte per-instance binding; this
# pins the count (backend-independent, so it is checkable without Direct3D)
# and reads both halves of the packed slot back as pixels.
score_add_gfx_test(scene_instance_binding_pack GfxSceneInstanceBindingPack.cpp)

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

if(TARGET score_plugin_threedim AND TARGET score_plugin_avnd)
  set(_3d_md "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-threedim/Threedim")
  score_plugin_hidden_sources(_md_hidden
      "${_3d_md}/Primitive.cpp"
      "${_3d_md}/ModelDisplay/ModelDisplayNode.cpp")
  score_add_test(test_gfx_model_display_texture
    SOURCES
      GfxModelDisplayTexture.cpp
      ${_md_hidden}
    GUI
    PLUGINS score_plugin_gfx score_plugin_avnd score_plugin_scenario score_lib_process
    LIBS test_gfx_engine_glue)
  target_compile_definitions(test_gfx_model_display_texture PRIVATE
    GFX_TEST_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/corpus")
  target_include_directories(test_gfx_model_display_texture SYSTEM PRIVATE
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

if(TARGET score_plugin_avnd)
  score_add_test(test_gfx_gpu_texture_output_blit
    SOURCES GfxGpuTextureOutputBlit.cpp
    GUI
    PLUGINS score_plugin_gfx score_plugin_avnd score_plugin_scenario score_lib_process
    LIBS test_gfx_engine_glue)
  target_compile_definitions(test_gfx_gpu_texture_output_blit PRIVATE
    GFX_TEST_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/corpus")
  target_include_directories(test_gfx_gpu_texture_output_blit SYSTEM PRIVATE
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-gfx"
    "${SCORE_ROOT_BINARY_DIR}/src/plugins/score-plugin-gfx"
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-avnd"
    "${SCORE_ROOT_BINARY_DIR}/src/plugins/score-plugin-avnd"
    $<TARGET_PROPERTY:score_plugin_avnd,INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:score_plugin_gfx,INCLUDE_DIRECTORIES>)
endif()

# The depth clear of the pass RenderList opens on a SINK's render target.
# Needs the same threedim scaffolding as the Crousti target above.
if(TARGET score_plugin_threedim AND TARGET score_plugin_avnd)
  set(_3d_sd "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-threedim/Threedim")
  score_plugin_hidden_sources(_sinkdepth_hidden "${_3d_sd}/Primitive.cpp")
  score_add_test(test_gfx_sink_depth_clear_polarity
    SOURCES
      GfxSinkDepthClearPolarity.cpp
      ${_sinkdepth_hidden}
    GUI
    PLUGINS score_plugin_gfx score_plugin_avnd score_plugin_scenario score_lib_process
    LIBS test_gfx_engine_glue)
  target_compile_definitions(test_gfx_sink_depth_clear_polarity PRIVATE
    GFX_TEST_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/corpus")
  target_include_directories(test_gfx_sink_depth_clear_polarity SYSTEM PRIVATE
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

# An EisenScript rasterises. StructureSynthTest.cpp drives
# StrucSynth::worker::work() as a pure function and pins the vertex buffer;
# nothing anywhere puts that mesh on a GPU. Same hidden-sources shape as the
# Crousti target above -- StructureSynth.cpp is hidden-visibility inside the
# plug-in, and it drags in TinyObj.cpp (ObjFromString) and the vendored
# libssynth, which the plug-in target already carries.
if(TARGET score_plugin_threedim AND TARGET score_plugin_avnd)
  set(_3d_ss "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-threedim/Threedim")
  score_plugin_hidden_sources(_ssynth_hidden
      "${_3d_ss}/StructureSynth.cpp"
      "${_3d_ss}/TinyObj.cpp")
  score_add_test(test_gfx_structure_synth_render
    SOURCES
      GfxStructureSynthRender.cpp
      ${_ssynth_hidden}
    GUI
    PLUGINS score_plugin_gfx score_plugin_avnd score_plugin_threedim
            score_plugin_scenario score_lib_process
    LIBS test_gfx_engine_glue)
  target_compile_definitions(test_gfx_structure_synth_render PRIVATE
    GFX_TEST_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/corpus")
  target_include_directories(test_gfx_structure_synth_render SYSTEM PRIVATE
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

# EnvironmentLoader's render_target_size override reaches the camera
# pass (packCameraUBO's renderSize/aspect), scoped to exactly that -- the
# physical RT extent must NOT change. CPU producer/merge + camera-math halves
# run GPU-less; the render half drives real Cube+Camera+EnvironmentLoader
# through ScenePreprocessorNode with a camera-UBO-probing raster shader.
if(TARGET score_plugin_threedim AND TARGET score_plugin_avnd)
  score_plugin_hidden_sources(_env_rts_hidden
      "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-threedim/Threedim/Primitive.cpp"
      "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-threedim/Threedim/Camera.cpp"
      "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-threedim/Threedim/EnvironmentLoader.cpp")
  score_add_test(test_gfx_env_render_target_size
    SOURCES GfxEnvRenderTargetSize.cpp ${_env_rts_hidden}
    GUI
    PLUGINS score_plugin_gfx score_plugin_avnd score_plugin_scenario score_lib_process
    LIBS test_gfx_engine_glue)
  target_include_directories(test_gfx_env_render_target_size SYSTEM PRIVATE
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

# Switching the active camera mid-render moves the image (the drawn
# colour identifies WHICH camera is active), and switching back reproduces
# the first frame byte-for-byte with zero texture-channel reallocation. CPU
# halves pin the id mint, merge-order first-non-zero-wins, CameraSwitch
# Select forwarding and flattenScene id->index resolution; the render half
# drives real Cube + real Camera-authored scenes through ScenePreprocessorNode
# with a camera-position-encoding probe raster. OffsetAllocator compiled in
# because the TU reads GpuResourceRegistry through RenderList::registry().
if(TARGET score_plugin_threedim AND TARGET score_plugin_avnd)
  set(_3d_cs "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-threedim/Threedim")
  score_plugin_hidden_sources(_cam_switch_hidden
      "${_3d_cs}/Primitive.cpp"
      "${_3d_cs}/Camera.cpp")
  score_add_test(test_gfx_camera_switch_live
    SOURCES GfxCameraSwitchLive.cpp ${_cam_switch_hidden}
    GUI
    PLUGINS score_plugin_gfx score_plugin_avnd score_plugin_scenario score_lib_process
    LIBS test_gfx_engine_glue)
  target_sources(test_gfx_camera_switch_live PRIVATE
    "${SCORE_ROOT_SOURCE_DIR}/3rdparty/OffsetAllocator/offsetAllocator.cpp")
  target_include_directories(test_gfx_camera_switch_live SYSTEM PRIVATE
    "${SCORE_ROOT_SOURCE_DIR}/3rdparty/OffsetAllocator"
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

# Render half: the Instances spinbox on Buffers-to-geometry v2 drives
# the drawn count, live, with exactly one CustomMesh::reload per flush. Also
# PINS (green, flip-when-fixed) the engine gap that an Instances-ONLY edit is
# absent from the node's change fingerprint and freezes until another control
# nudges it.
if(TARGET score_plugin_threedim)
  score_plugin_hidden_sources(_b2g_instances_hidden
      "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-threedim/Threedim/BufferToGeometry2.cpp")
  score_add_test(test_gfx_buffertogeometry_instances
    SOURCES GfxBufferToGeometryInstances.cpp ${_b2g_instances_hidden}
    GUI
    PLUGINS score_plugin_gfx score_plugin_scenario score_lib_process
    LIBS test_gfx_engine_glue)
  target_compile_definitions(test_gfx_buffertogeometry_instances PRIVATE
    GFX_TEST_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/corpus")
  target_include_directories(test_gfx_buffertogeometry_instances SYSTEM PRIVATE
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-threedim"
    "${SCORE_ROOT_BINARY_DIR}/src/plugins/score-plugin-threedim"
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-gfx"
    "${SCORE_ROOT_BINARY_DIR}/src/plugins/score-plugin-gfx"
    $<TARGET_PROPERTY:score_plugin_threedim,INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:score_plugin_gfx,INCLUDE_DIRECTORIES>)
endif()

# Render half: a point cloud renders as exactly N points -- real
# PCLToMesh2 descriptor with a NON-ZERO byte_offset (regression guard against a
# full-byte_size over-count), real avnd bridge, real point draw; each point owns
# its own pixel.
if(TARGET score_plugin_threedim)
  score_plugin_hidden_sources(_pcl_count_hidden
      "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-threedim/Threedim/PCLToGeometry.cpp")
  score_add_test(test_gfx_pointcloud_count
    SOURCES GfxPointCloudCount.cpp ${_pcl_count_hidden}
    GUI
    PLUGINS score_plugin_gfx score_plugin_scenario score_lib_process
    LIBS test_gfx_engine_glue)
  target_compile_definitions(test_gfx_pointcloud_count PRIVATE
    GFX_TEST_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/corpus")
  target_include_directories(test_gfx_pointcloud_count SYSTEM PRIVATE
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-threedim"
    "${SCORE_ROOT_BINARY_DIR}/src/plugins/score-plugin-threedim"
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-gfx"
    "${SCORE_ROOT_BINARY_DIR}/src/plugins/score-plugin-gfx"
    $<TARGET_PROPERTY:score_plugin_threedim,INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:score_plugin_gfx,INCLUDE_DIRECTORIES>)
endif()

# Render half: the Instancer clamps a shrinking transforms buffer
# (byte_size shrinks AND handle changes) without over-reading -- real
# Instancer tick + real ScenePreprocessor instance-group copy + real
# instanced draw + pixels. Instancer.cpp is hidden-visibility, compiled in.
if(TARGET score_plugin_threedim)
  score_plugin_hidden_sources(_instancer_shrink_hidden
      "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-threedim/Threedim/Instancer.cpp")
  score_add_test(test_gfx_instancer_shrink
    SOURCES GfxInstancerShrink.cpp ${_instancer_shrink_hidden}
    GUI
    PLUGINS score_plugin_gfx score_plugin_scenario score_lib_process
    LIBS test_gfx_engine_glue)
  target_compile_definitions(test_gfx_instancer_shrink PRIVATE
    GFX_TEST_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/corpus")
  target_include_directories(test_gfx_instancer_shrink SYSTEM PRIVATE
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-threedim"
    "${SCORE_ROOT_BINARY_DIR}/src/plugins/score-plugin-threedim"
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-gfx"
    "${SCORE_ROOT_BINARY_DIR}/src/plugins/score-plugin-gfx"
    $<TARGET_PROPERTY:score_plugin_threedim,INCLUDE_DIRECTORIES>
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

# A material texture swapped live changes the frame WITHOUT a graph
# rebuild, and a layer-growth change legitimately reallocates the
# QRhiTextureArray -- the test asserts WHICH path each change kind took via
# registry snapshots. Case 2 ([!mayfail]) pins the statically-located gap
# that a plain handle swap may never reach the consumer (no dynamic-slot
# term in meshesUnchanged).
score_add_gfx_test(material_texture_swap GfxMaterialTextureSwap.cpp)
score_add_gfx_test(material_presets_compile GfxMaterialPresetsCompile.cpp)
score_add_gfx_test(material_preset_wrap GfxMaterialPresetWrap.cpp)
target_sources(test_gfx_material_preset_wrap PRIVATE
  "${SCORE_ROOT_SOURCE_DIR}/3rdparty/OffsetAllocator/offsetAllocator.cpp")
target_include_directories(test_gfx_material_preset_wrap PRIVATE
  "${SCORE_ROOT_SOURCE_DIR}/3rdparty/OffsetAllocator")
score_add_gfx_test(scene_gpu_attribute_swap GfxScenePreprocessorGpuAttributeSwap.cpp)
target_sources(test_gfx_scene_gpu_attribute_swap PRIVATE
  "${SCORE_ROOT_SOURCE_DIR}/3rdparty/OffsetAllocator/offsetAllocator.cpp")
target_include_directories(test_gfx_scene_gpu_attribute_swap PRIVATE
  "${SCORE_ROOT_SOURCE_DIR}/3rdparty/OffsetAllocator")
target_sources(test_gfx_material_texture_swap PRIVATE
  "${SCORE_ROOT_SOURCE_DIR}/3rdparty/OffsetAllocator/offsetAllocator.cpp")
target_include_directories(test_gfx_material_texture_swap PRIVATE
  "${SCORE_ROOT_SOURCE_DIR}/3rdparty/OffsetAllocator")
target_sources(test_gfx_mesh_slabs PRIVATE
  "${SCORE_ROOT_SOURCE_DIR}/3rdparty/OffsetAllocator/offsetAllocator.cpp")
target_include_directories(test_gfx_mesh_slabs PRIVATE
  "${SCORE_ROOT_SOURCE_DIR}/3rdparty/OffsetAllocator")

# The GfxContext incremental edge-to-not-yet-present-node case is a DOCUMENTED
# GAP: the two-channel GfxContext baseline-diff bug is not reproducible through
# the Graph-level L3 fixture. This target holds the honest SKIP anchor + rationale.
score_add_gfx_test(incremental_edge_defer GfxIncrementalEdgeDefer.cpp)

# -----------------------------------------------------------------------------
# UNIT regression guards. These are non-GPU pure logic/unit tests — no display,
# no QRhi — so they build as plain score_add_test targets (Catch2WithMain), each
# its own executable.
#
#   assettable_eviction   AssetTable: a staged-but-never-acquired entry must be
#                         trimmable. Unit test constructing AssetTable directly
#                         (needs the QImage member in DecodedAsset -> link
#                         Qt Gui).
#   scatter_dispatch      GPUBufferScatter dispatch-dims clamp + shader
#                         index linearization. Pure-math UNIT test of the
#                         algorithm (replicates the private helper + GLSL
#                         formula; see the file header for scope/honesty).
#   gstreamer_bus_classify GStreamer bus WARNING-vs-ERROR classification
#                         + feed/finalize decoupling. DOCUMENTED GAP
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
# event loop for several seconds, which is what delivers
# PM::PluginSettingsModel::firstTimeLibraryDownload()'s modal QMessageBox and
# wedges the run under the offscreen QPA. Leaving the escape hatch off keeps
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
            score_plugin_media score_lib_process score_lib_device ${ARGN}
    LIBS test_gfx_engine_glue)
  target_compile_definitions(test_gfx_process_${name} PRIVATE
    GFX_TEST_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/corpus")
endfunction()

score_add_gfx_process_test(shader_commands GfxShaderCommands.cpp)
score_add_gfx_process_test(images          GfxImagesProcess.cpp)
score_add_gfx_process_test(video_text      GfxVideoTextProcess.cpp)

# The raw-raster family lives in score_plugin_threedim while the three other
# shader families live in score_plugin_gfx, and the whole point of the drop
# layer is that they share ".fs" without stepping on each other -- so the drop
# test needs both plug-ins loaded. The cases that need it SKIP when it is not
# built.
set(_gfx_library_extra_plugins "")
if(TARGET score_plugin_threedim AND TARGET score_plugin_avnd)
  set(_gfx_library_extra_plugins score_plugin_threedim score_plugin_avnd)
endif()
score_add_gfx_process_test(library         GfxProcessLibrary.cpp
                           ${_gfx_library_extra_plugins})

# Pin, its own single-test executable: an empty nodelist mime payload
# SIGABRTs in rapidjson today. The case asserts the CORRECT behaviour (child
# process survives, zero drops) and is [!shouldfail]; the drop runs in a
# forked child because CTest's WILL_FAIL does not invert abnormal termination.
score_add_gfx_process_test(drop_empty_nodelist GfxDropEmptyNodelistAbort.cpp)

# The run_commands full-rebuild flag must not latch across frames and
# downgrade the next incremental edge diff. Drives the real document
# GfxContext through GfxExecutionAction, observed via SCORE_GFX_TRACE.
score_add_gfx_process_test(edge_consume_latch GfxEdgeConsumeLatch.cpp)

# Within one incremental frame, every renderer (initState) precedes
# every pass creation. Observed via the GFX-RECONCILE / GFX-PASS trace lines.
score_add_gfx_process_test(renderers_before_passes GfxRenderersBeforePasses.cpp)

# Opening a texture-outlet preview used to force a full rebuild of every render
# list, so whatever was already on screen blinked. Drives the real preview
# widget against the real document GfxContext.
score_add_gfx_process_test(preview_output_rebuild GfxPreviewOutputRebuild.cpp)

# What a frame costs as the target grows, with no readback and no output device
# in the way. Answers "is the renderer the wall at 8K" -- it is not.
score_add_test(test_gfx_render_scaling
  SOURCES GfxRenderScaling.cpp
  GUI
  PLUGINS score_plugin_gfx score_lib_process)
target_include_directories(test_gfx_render_scaling
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(test_gfx_render_scaling PRIVATE
  GFX_TEST_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/corpus")
set_tests_properties(test_gfx_render_scaling PROPERTIES TIMEOUT 600 RUN_SERIAL TRUE)

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

# A geometry filter displaces the mesh it is given, by a closed-form
# delta.
score_add_gfx_test(geometry_filter_shift GfxGeometryFilterShift.cpp)

# Closing a document mid-render tears down cleanly, fork-isolated so a
# crash is a red assertion in the parent rather than a dead run.
score_add_gfx_test(document_close_teardown GfxDocumentCloseTeardown.cpp)

# A camera array renders six faces, each carrying its own camera's
# encoded forward direction. Needs Threedim::CameraArray, which is
# hidden-visibility inside the plug-in, so it is compiled in directly.
if(TARGET score_plugin_threedim AND TARGET score_plugin_avnd)
  set(_ca_c "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-threedim/Threedim")
  score_plugin_hidden_sources(_camarray_hidden
      "${_ca_c}/Primitive.cpp"
      "${_ca_c}/Camera.cpp"
      "${_ca_c}/CameraArray.cpp")
  score_add_test(test_gfx_camera_array_faces
    SOURCES
      GfxCameraArrayFaces.cpp
      ${_camarray_hidden}
    GUI
    PLUGINS score_plugin_gfx score_plugin_avnd score_plugin_scenario score_lib_process
    LIBS test_gfx_engine_glue)
  target_compile_definitions(test_gfx_camera_array_faces PRIVATE
    GFX_TEST_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/corpus")
  target_include_directories(test_gfx_camera_array_faces SYSTEM PRIVATE
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

# MODEL_MATRIX is declared one binding higher when MULTIVIEW >= 2, and the
# raw-raster renderer numbers its bindings independently. Threedim::Cube is
# hidden-visibility, so Primitive.cpp is compiled in as the other scene tests do.
if(TARGET score_plugin_threedim AND TARGET score_plugin_avnd)
  set(_mmb_c "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-threedim/Threedim")
  score_plugin_hidden_sources(_mmb_hidden
      "${_mmb_c}/Primitive.cpp"
      "${_mmb_c}/Camera.cpp")
  score_add_test(test_gfx_rawraster_model_matrix_binding
    SOURCES
      GfxRawRasterModelMatrixBinding.cpp
      ${_mmb_hidden}
    GUI
    PLUGINS score_plugin_gfx score_plugin_avnd score_plugin_scenario score_lib_process
    LIBS test_gfx_engine_glue)
  target_compile_definitions(test_gfx_rawraster_model_matrix_binding PRIVATE
    GFX_TEST_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/corpus")
  target_include_directories(test_gfx_rawraster_model_matrix_binding SYSTEM PRIVATE
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

# Every example in the shader guide, pinned by the picture it renders. The bake
# half is free: the files live in corpus/ so ShaderCorpusTargets sweeps them.
score_add_gfx_test(shader_guide_examples GfxShaderGuideExamples.cpp)

# Per-layer depth renders through the scratch-D32F + copyTexture shim,
# each layer carrying its own closed-form depth.
score_add_gfx_test(per_layer_depth GfxPerLayerDepth.cpp)

# A gaussian splat renders through the path real scores take
# (asset_loader(.ply) -> Scene Preprocessor -> scene_group -> Flattened Scene
# Filter -> CSF chain -> Render Pipeline), on a .ply the test synthesises.
# score_add_gfx_test() is not enough: the loader entry points are
# hidden-visibility inside the plug-in, so the AssetLoader / PrimitiveCloud /
# Ply translation units are compiled straight in. test_unit_threedim_3rdparty
# (miniply / mikktspace / ufbx) is reusable because tests/CMakeLists.txt adds
# `unit` before `gfx`; the TARGET guard keeps that honest.
if(TARGET score_plugin_threedim AND TARGET test_unit_threedim_3rdparty)
  set(_splat_3d "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-threedim/Threedim")
  score_plugin_hidden_sources(_splat_hidden
      "${_splat_3d}/AssetLoader.cpp" "${_splat_3d}/GltfParser.cpp"
      "${_splat_3d}/FbxParser.cpp"   "${_splat_3d}/TinyObj.cpp"
      "${_splat_3d}/Ply.cpp"         "${_splat_3d}/VcgImporters.cpp"
      "${_splat_3d}/SceneFromMeshes.cpp"
      "${_splat_3d}/PrimitiveCloud/PlyParser.cpp"
      "${_splat_3d}/PrimitiveCloud/SplatBinary.cpp"
      "${_splat_3d}/PrimitiveCloud/SpzCodec.cpp"
      "${_splat_3d}/PrimitiveCloud/SceneFromCloud.cpp"
      "${_splat_3d}/PrimitiveCloud/FormatOverride.cpp")
  score_add_test(test_gfx_splat_render
    SOURCES GfxSplatRender.cpp ${_splat_hidden}
    GUI
    PLUGINS score_plugin_gfx score_plugin_threedim score_plugin_avnd
            score_plugin_scenario score_lib_process
    LIBS test_gfx_engine_glue test_unit_threedim_3rdparty fastgltf spz
         "${QT_PREFIX}::Gui")
  target_compile_definitions(test_gfx_splat_render PRIVATE
    GFX_TEST_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/corpus")
  target_include_directories(test_gfx_splat_render SYSTEM PRIVATE
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-threedim"
    "${SCORE_ROOT_BINARY_DIR}/src/plugins/score-plugin-threedim"
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-gfx"
    "${SCORE_ROOT_BINARY_DIR}/src/plugins/score-plugin-gfx"
    "${SCORE_ROOT_SOURCE_DIR}/3rdparty/vcglib"
    "${SCORE_ROOT_SOURCE_DIR}/3rdparty/eigen"
    $<TARGET_PROPERTY:score_plugin_threedim,INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:score_plugin_gfx,INCLUDE_DIRECTORIES>)
endif()

# The panel's edge-blend overlay is a QPainter gradient, and stopped one pixel
# short of the quad's right and bottom edges.
score_add_test(test_gfx_blend_preview_fringe
  SOURCES BlendPreviewFringe.cpp
  GUI)

# Images node output orientation vs ISF (N94): into a sink, through an ISF
# image input and into ModelDisplay.
if(TARGET score_plugin_threedim AND TARGET score_plugin_avnd)
  set(_3d_md_fixl "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-threedim/Threedim")
  score_plugin_hidden_sources(_md_hidden_fixl
      "${_3d_md_fixl}/Primitive.cpp"
      "${_3d_md_fixl}/ModelDisplay/ModelDisplayNode.cpp")
  score_add_test(test_gfx_images_orientation_fixl
    SOURCES
      GfxImagesOrientationFixL.cpp
      ${_md_hidden_fixl}
    GUI
    PLUGINS score_plugin_gfx score_plugin_avnd score_plugin_scenario score_lib_process
    LIBS test_gfx_engine_glue)
  target_compile_definitions(test_gfx_images_orientation_fixl PRIVATE
    GFX_TEST_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/corpus")
  target_include_directories(test_gfx_images_orientation_fixl SYSTEM PRIVATE
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

# CSF sampled inputs (N67 audio, N68 TYPE image without ACCESS): declared as
# samplers by parse_csf, baked, and the sampled image rendered on every backend.
score_add_csf_test(sampled_inputs_fixd GfxCsfSampledInputsFixD.cpp)

# Raw raster, agent K: single colour OUTPUT FORMAT (N70), missing required
# vertex input reported at warning level (N58).
score_add_gfx_test(raw_raster_fixk GfxRawRasterFixK.cpp)

# Merge Geometries bakes each input's transform into its meshes (fixF, N79a).
score_add_gfx_test(merge_geometries_transform_fixF MergeGeometriesTransformFixF.cpp)

# Academy Texture Loader cubemaps (N43 equirect faces + orientation, N44
# cross/strip slicing and the over-read guard), driven directly on a QRhi.
if(TARGET score_addon_academy AND TARGET score_plugin_avnd)
  set(_academy_src_fixa "${SCORE_ROOT_SOURCE_DIR}/src/addons/score-addon-academy")
  score_plugin_hidden_sources(_academy_hidden_fixa
      "${_academy_src_fixa}/Academy/Asset/TextureLoader.cpp"
      "${_academy_src_fixa}/Academy/Resources/EquirectToCubemap.cpp"
      "${_academy_src_fixa}/Academy/Resources/TextureResource.cpp")
  score_add_test(test_gfx_academy_texture_loader_cube_fixa
    SOURCES
      GfxAcademyTextureLoaderCubeFixA.cpp
      ${_academy_hidden_fixa}
    GUI
    PLUGINS score_plugin_gfx score_plugin_avnd score_addon_academy
    LIBS test_gfx_engine_glue)
  target_include_directories(test_gfx_academy_texture_loader_cube_fixa SYSTEM PRIVATE
    "${_academy_src_fixa}"
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-gfx"
    "${SCORE_ROOT_BINARY_DIR}/src/plugins/score-plugin-gfx"
    $<TARGET_PROPERTY:score_addon_academy,INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:score_plugin_avnd,INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:score_plugin_gfx,INCLUDE_DIRECTORIES>)
endif()

# Image feedback loops, agent M: an all-Immediate cycle is broken at one edge
# and warned about (N63); a Delayed self-cable carries state (N64).
score_add_gfx_test(feedback_cycle_fixm GfxFeedbackCycleFixM.cpp)

# Library preset fixes (blinn_phong lights, classic_pbr_textured fallback,
# classic_pbr_shadowed, openpbr light scale, cubemap_orbit orientation, compute
# preset workgroup sizes and Game of Life).
score_add_gfx_test(library_preset_fixes_o GfxLibraryPresetFixesO.cpp)
target_sources(test_gfx_library_preset_fixes_o PRIVATE
  "${SCORE_ROOT_SOURCE_DIR}/3rdparty/OffsetAllocator/offsetAllocator.cpp")
target_include_directories(test_gfx_library_preset_fixes_o PRIVATE
  "${SCORE_ROOT_SOURCE_DIR}/3rdparty/OffsetAllocator")
if(EXISTS "${SCORE_ROOT_SOURCE_DIR}/src/addons/score-addon-academy/3rdparty/OpenPBR-BSDF/openpbr_data_constants.h")
  target_include_directories(test_gfx_library_preset_fixes_o PRIVATE
    "${SCORE_ROOT_SOURCE_DIR}/src/addons/score-addon-academy/3rdparty/OpenPBR-BSDF")
  target_compile_definitions(test_gfx_library_preset_fixes_o PRIVATE FIXO_OPENPBR_BSDF=1)
endif()

# The avendish "Average color" example averages its input (agent G, N89).
if(TARGET score_plugin_avnd)
  score_add_test(test_gfx_average_color_fixg
    SOURCES GfxAverageColorFixG.cpp
    GUI
    PLUGINS score_plugin_gfx score_plugin_avnd score_plugin_scenario score_lib_process
    LIBS test_gfx_engine_glue)
  target_compile_definitions(test_gfx_average_color_fixg PRIVATE
    GFX_TEST_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/corpus")
  target_include_directories(test_gfx_average_color_fixg SYSTEM PRIVATE
    "${SCORE_ROOT_SOURCE_DIR}/3rdparty/avendish/examples"
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-gfx"
    "${SCORE_ROOT_BINARY_DIR}/src/plugins/score-plugin-gfx"
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-avnd"
    "${SCORE_ROOT_BINARY_DIR}/src/plugins/score-plugin-avnd"
    $<TARGET_PROPERTY:score_plugin_avnd,INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:score_plugin_gfx,INCLUDE_DIRECTORIES>)
endif()

# CSF attribute naming, COPY_FROM next to AUXILIARY, unblended storage-image
# outlet, read_write on a static CPU mesh, FRAMEINDEX origin (agent C: N55,
# N56, N57, N59, N60, N62).
score_add_gfx_test(csf_isf_fixc GfxCsfIsfFixC.cpp)

# avnd bridge inlet resolution (fixE): unwired / upstream-owned texture inlets,
# per-port scene inlets, CPU geometry into a GPU geometry inlet.
if(TARGET score_plugin_threedim AND TARGET score_plugin_avnd)
  set(_3d_fixe "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-threedim/Threedim")
  score_plugin_hidden_sources(_avnd_inlet_fixe_hidden
      "${_3d_fixe}/Primitive.cpp"
      "${_3d_fixe}/PBRMesh.cpp"
      "${_3d_fixe}/SceneResourceRoute.cpp"
      "${_3d_fixe}/Noise.cpp")
  score_add_test(test_gfx_avnd_inlet_resolve_fixe
    SOURCES GfxAvndInletResolveFixE.cpp ${_avnd_inlet_fixe_hidden}
    GUI
    PLUGINS score_plugin_gfx score_plugin_avnd score_plugin_scenario score_lib_process
    LIBS test_gfx_engine_glue)
  target_compile_definitions(test_gfx_avnd_inlet_resolve_fixe PRIVATE
    GFX_TEST_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/corpus")
  target_include_directories(test_gfx_avnd_inlet_resolve_fixe SYSTEM PRIVATE
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

# A loaded Structure Synth program is drawn (agent G, N82).
if(TARGET score_plugin_threedim AND TARGET score_plugin_avnd)
  set(_3d_ssfixg "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-threedim/Threedim")
  score_plugin_hidden_sources(_ssfixg_hidden
      "${_3d_ssfixg}/StructureSynth.cpp"
      "${_3d_ssfixg}/TinyObj.cpp"
      "${_3d_ssfixg}/ModelDisplay/ModelDisplayNode.cpp")
  score_add_test(test_gfx_structure_synth_fixg
    SOURCES
      GfxStructureSynthFixG.cpp
      ${_ssfixg_hidden}
    GUI
    PLUGINS score_plugin_gfx score_plugin_avnd score_plugin_scenario score_lib_process
    LIBS test_gfx_engine_glue)
  if(_ssfixg_hidden)
    target_link_libraries(test_gfx_structure_synth_fixg PRIVATE ssynth mikktspace)
  endif()
  target_include_directories(test_gfx_structure_synth_fixg SYSTEM PRIVATE
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

# ScenePreprocessorNode: default material after a pre-frame RenderList rebuild,
# per-preprocessor environment, hidden scene nodes, exact cloud bucket sizes.
score_add_gfx_test(scene_preprocessor_fixn GfxScenePreprocessorFixN.cpp)
target_sources(test_gfx_scene_preprocessor_fixn PRIVATE
  "${SCORE_ROOT_SOURCE_DIR}/3rdparty/OffsetAllocator/offsetAllocator.cpp")
target_include_directories(test_gfx_scene_preprocessor_fixn PRIVATE
  "${SCORE_ROOT_SOURCE_DIR}/3rdparty/OffsetAllocator")

# N84: the Splat renderer (GaussianSplatNode) joining a render list through the
# incremental reconcile path (initState + addOutputPass, no init) must not
# crash and must draw its splat.
if(TARGET score_plugin_threedim)
  set(_3d_gsi "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-threedim/Threedim")
  score_plugin_hidden_sources(_gsi_hidden
      "${_3d_gsi}/Splat/GaussianSplatNode.cpp")
  score_add_test(test_gfx_gaussian_splat_incremental
    SOURCES
      GfxGaussianSplatIncremental.cpp
      ${_gsi_hidden}
    GUI
    PLUGINS score_plugin_gfx score_plugin_scenario score_lib_process
    LIBS test_gfx_engine_glue)
  target_compile_definitions(test_gfx_gaussian_splat_incremental PRIVATE
    GFX_TEST_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/corpus")
  target_include_directories(test_gfx_gaussian_splat_incremental SYSTEM PRIVATE
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-threedim"
    "${SCORE_ROOT_BINARY_DIR}/src/plugins/score-plugin-threedim"
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-gfx"
    "${SCORE_ROOT_BINARY_DIR}/src/plugins/score-plugin-gfx"
    $<TARGET_PROPERTY:score_plugin_threedim,INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:score_plugin_gfx,INCLUDE_DIRECTORIES>)
endif()

# An edge to a node that never registers is retried a bounded number of ticks
# and then dropped with one warning, instead of re-entering the edge diff on
# every tick. Drives the real document GfxContext (fixJ, N6).
score_add_gfx_process_test(edge_retry_bound_fixj GfxEdgeRetryBoundFixJ.cpp)

# Extract buffer picks up a CSF geometry's buffers created after its init
# (agent G, N80).
if(TARGET score_plugin_threedim AND TARGET score_plugin_avnd)
  score_plugin_hidden_sources(_extract_late_fixg_hidden
      "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-threedim/Threedim/GeometryToBuffer.cpp")
  score_add_test(test_gfx_extract_buffer_late_fixg
    SOURCES GfxExtractBufferLateFixG.cpp ${_extract_late_fixg_hidden}
    GUI
    PLUGINS score_plugin_gfx score_plugin_avnd score_plugin_scenario score_lib_process
    LIBS test_gfx_engine_glue)
  target_compile_definitions(test_gfx_extract_buffer_late_fixg PRIVATE
    GFX_TEST_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/corpus")
  target_include_directories(test_gfx_extract_buffer_late_fixg SYSTEM PRIVATE
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

# Academy TextureResource::upload refuses pixel data shorter than its slices,
# and the EquirectToCubemap vertex stage mirrors Y on HLSL/MSL only, keeping
# the panorama's top on +Y (agent E2).
if(TARGET score_addon_academy)
  set(_academy_src_e2 "${SCORE_ROOT_SOURCE_DIR}/src/addons/score-addon-academy")
  score_plugin_hidden_sources(_academy_hidden_e2
      "${_academy_src_e2}/Academy/Resources/EquirectToCubemap.cpp"
      "${_academy_src_e2}/Academy/Resources/TextureResource.cpp")
  score_add_test(test_gfx_academy_upload_bounds_e2
    SOURCES
      GfxAcademyUploadBoundsE2.cpp
      ${_academy_hidden_e2}
    GUI
    PLUGINS score_plugin_gfx score_addon_academy
    LIBS test_gfx_engine_glue)
  target_compile_definitions(test_gfx_academy_upload_bounds_e2 PRIVATE
    ACADEMY_EQUIRECT_CPP="${_academy_src_e2}/Academy/Resources/EquirectToCubemap.cpp")
  target_include_directories(test_gfx_academy_upload_bounds_e2 SYSTEM PRIVATE
    "${_academy_src_e2}"
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-gfx"
    "${SCORE_ROOT_BINARY_DIR}/src/plugins/score-plugin-gfx"
    $<TARGET_PROPERTY:score_addon_academy,INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:score_plugin_gfx,INCLUDE_DIRECTORIES>)
endif()

# Merge Geometries bakes transforms into GPU-resident geometry (agent B2).
score_add_gfx_test(merge_geometries_gpu_transform_b2 MergeGeometriesGpuTransformB2.cpp)

# N95: flattenScene drops transform / light slot refs freed before a re-init (agent F2).
score_add_gfx_test(stale_slot_flatten_f2 GfxStaleSlotFlattenF2.cpp)
target_sources(test_gfx_stale_slot_flatten_f2 PRIVATE
  "${SCORE_ROOT_SOURCE_DIR}/3rdparty/OffsetAllocator/offsetAllocator.cpp")
target_include_directories(test_gfx_stale_slot_flatten_f2 PRIVATE
  "${SCORE_ROOT_SOURCE_DIR}/3rdparty/OffsetAllocator")

# Self-feedback target limits (depth, mips, GrabsFromSource ports).
score_add_gfx_test(selffb_limits_d2 GfxSelfFeedbackLimitsD2.cpp)

# A VSA shader's gl_Position is WebGL clip space: z in [-w, w] is visible on
# every backend (agent G2, N39).
score_add_gfx_test(vsa_clip_depth_g2 GfxVsaClipDepthG2.cpp)

# CSF storage bindings at the published range; read_write on a static GPU upstream (agent A2).
score_add_gfx_test(csf_buffer_range_fixa2 GfxCsfBufferRangeFixA2.cpp)

# Raw raster storage images take the lowest bindings, under the 8 OpenGL image
# units NVIDIA exposes (N93).
score_add_gfx_test(storage_image_low_binding_n93 GfxStorageImageLowBindingN93.cpp)

# An offscreen output logs a lost GPU device once and reports it through
# OutputNode::deviceLost() (agent I4, N73 option A).
score_add_gfx_test(offscreen_device_lost_i4 GfxOffscreenDeviceLostI4.cpp)

# Texture inlet settings on Texture to buffer and Texture Info (agent I6).
if(TARGET score_plugin_threedim AND TARGET score_plugin_avnd)
  score_add_test(test_gfx_inlet_settings_i6
    SOURCES GfxInletSettingsI6.cpp
    GUI
    PLUGINS score_plugin_gfx score_plugin_avnd score_plugin_scenario score_lib_process
    LIBS test_gfx_engine_glue)
  target_compile_definitions(test_gfx_inlet_settings_i6 PRIVATE
    GFX_TEST_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/corpus")
  target_include_directories(test_gfx_inlet_settings_i6 SYSTEM PRIVATE
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

# Instancer TRS scales the instance spread, the prototype's root scale does not
# (agent I2, N61 option B).
if(TARGET score_plugin_threedim)
  score_plugin_hidden_sources(_instancer_placement_i2_hidden
      "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-threedim/Threedim/Instancer.cpp")
  score_add_test(test_gfx_instancer_placement_i2
    SOURCES GfxInstancerPlacementI2.cpp ${_instancer_placement_i2_hidden}
    GUI
    PLUGINS score_plugin_gfx score_plugin_scenario score_lib_process
    LIBS test_gfx_engine_glue)
  target_compile_definitions(test_gfx_instancer_placement_i2 PRIVATE
    GFX_TEST_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/corpus")
  target_include_directories(test_gfx_instancer_placement_i2 SYSTEM PRIVATE
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-threedim"
    "${SCORE_ROOT_BINARY_DIR}/src/plugins/score-plugin-threedim"
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-gfx"
    "${SCORE_ROOT_BINARY_DIR}/src/plugins/score-plugin-gfx"
    $<TARGET_PROPERTY:score_plugin_threedim,INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:score_plugin_gfx,INCLUDE_DIRECTORIES>)
endif()

# ISF graphics and CSF compute storage images take the lowest bindings, under
# the 8 OpenGL image units NVIDIA exposes (N93, agent I1).
score_add_gfx_test(isf_csf_image_binding_i1 GfxIsfCsfImageBindingI1.cpp)

# A texture inlet's filter change reaches CSF, raw raster and VSA samplers at
# runtime (agent I1).
score_add_gfx_test(input_sampler_filter_i1 GfxInputSamplerFilterI1.cpp)

# Premultiplied render targets, the ALPHA key, over-compositing of several
# cables into one input, and the MRT / CSF / multi-pass copies (N57, agent A3).
score_add_gfx_test(premultiplied_alpha_a3 GfxPremultipliedAlphaA3.cpp)

# What sampling macros return from premultiplied targets (straight for
# render-target inputs, raw otherwise, *_PREMULTIPLIED variants), COMPOSITE,
# and the premultiplied draws of outputs (agent A4).
score_add_gfx_test(alpha_a4 GfxAlphaA4.cpp)

# A video frame (opaque and translucent) read by a straight ISF (agent A4).
if(TARGET score_plugin_media)
  score_add_test(test_gfx_alpha_video_a4
    SOURCES GfxAlphaVideoA4.cpp
    GUI
    PLUGINS score_plugin_gfx score_plugin_media score_plugin_scenario
            score_lib_process
    LIBS test_gfx_engine_glue avutil)
  target_compile_definitions(test_gfx_alpha_video_a4 PRIVATE
    GFX_TEST_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/corpus")
endif()

# Merge Geometries names each attribute a transform leaves untransformed,
# once per renderer (CPU formats, per-instance, GPU vec2 positions) (L1).
score_add_gfx_test(merge_geometries_warn_l1 MergeGeometriesWarnL1.cpp)

# Texture inlet settings reach avnd GPU texture inputs and gpp shader nodes;
# an RGBA8 format set on an inlet is a setting (agent L2).
if(TARGET score_plugin_threedim AND TARGET score_plugin_avnd)
  score_add_test(test_gfx_texture_inlet_spec_l2
    SOURCES GfxTextureInletSpecL2.cpp
    GUI
    PLUGINS score_plugin_gfx score_plugin_avnd score_plugin_scenario score_lib_process
    LIBS test_gfx_engine_glue)
  target_compile_definitions(test_gfx_texture_inlet_spec_l2 PRIVATE
    GFX_TEST_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/corpus")
  target_include_directories(test_gfx_texture_inlet_spec_l2 SYSTEM PRIVATE
    "${SCORE_ROOT_SOURCE_DIR}/3rdparty/avendish/examples"
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

# Raw raster read_only storage image INPUTS take no sampler binding; storage
# image bindings past the OpenGL image units warn once per shader (agent A5).
score_add_gfx_test(n93_remainders_a5 GfxN93RemaindersA5.cpp)

# A node grabbing its own depth output (GrabsFromSource self-feedback) reads an
# empty texture instead of the live attachment, with one warning (N64, L1).
score_add_gfx_test(selffb_depth_grab_l1 GfxSelfFeedbackDepthGrabL1.cpp)

# Academy Texture Loader queues no generateMips for a 1x1 texture (Metal
# validation abort), through TextureResource and EquirectToCubemap (L1).
if(TARGET score_addon_academy AND TARGET score_plugin_avnd)
  set(_academy_src_l1 "${SCORE_ROOT_SOURCE_DIR}/src/addons/score-addon-academy")
  score_plugin_hidden_sources(_academy_hidden_l1
      "${_academy_src_l1}/Academy/Asset/TextureLoader.cpp"
      "${_academy_src_l1}/Academy/Resources/EquirectToCubemap.cpp"
      "${_academy_src_l1}/Academy/Resources/TextureResource.cpp")
  score_add_test(test_gfx_academy_mips_guard_l1
    SOURCES
      GfxAcademyMipsGuardL1.cpp
      ${_academy_hidden_l1}
    GUI
    PLUGINS score_plugin_gfx score_plugin_avnd score_addon_academy
    LIBS test_gfx_engine_glue)
  target_include_directories(test_gfx_academy_mips_guard_l1 SYSTEM PRIVATE
    "${_academy_src_l1}"
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-gfx"
    "${SCORE_ROOT_BINARY_DIR}/src/plugins/score-plugin-gfx"
    $<TARGET_PROPERTY:score_addon_academy,INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:score_plugin_avnd,INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:score_plugin_gfx,INCLUDE_DIRECTORIES>)
endif()

# Raw raster and CSF consumers follow an upstream auxiliary resized at run
# time (agent A5).
score_add_gfx_test(aux_size_a5 GfxAuxSizeA5.cpp)

# A texture inlet's mip mode switched while running reallocates the inlet's
# render target, for ISF, avnd GPU and gpp inputs (agent L4).
if(TARGET score_plugin_threedim AND TARGET score_plugin_avnd)
  score_add_test(test_gfx_mip_toggle_l4
    SOURCES GfxMipToggleL4.cpp
    GUI
    PLUGINS score_plugin_gfx score_plugin_avnd score_plugin_scenario score_lib_process
    LIBS test_gfx_engine_glue)
  target_compile_definitions(test_gfx_mip_toggle_l4 PRIVATE
    GFX_TEST_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/corpus")
  target_include_directories(test_gfx_mip_toggle_l4 SYSTEM PRIVATE
    "${SCORE_ROOT_SOURCE_DIR}/3rdparty/avendish/examples"
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

# A read_write / write_only CSF on an upstream it does not declare PERSISTENT
# works on a copy: init-once upstreams do not accumulate (agent A5, N59).
score_add_gfx_test(csf_read_write_copy_a5 GfxCsfReadWriteCopyA5.cpp)

# Raw raster "PRIMITIVE_DATA": PRIMITIVE_ID and BARYCENTRIC in the fragment
# stage, procedural and on an expanded indexed mesh (agent A5).
score_add_gfx_test(raw_raster_primitive_data_a5 GfxRawRasterPrimitiveDataA5.cpp)

# A gpp shader node fed by an ISF node with an unsized image inlet: the
# render list asks for its input target before initState (agent L4).
if(TARGET score_plugin_avnd)
  score_add_test(test_gfx_gpp_rt_before_init_l4
    SOURCES GfxGppRtBeforeInitL4.cpp
    GUI
    PLUGINS score_plugin_gfx score_plugin_avnd score_plugin_scenario score_lib_process
    LIBS test_gfx_engine_glue)
  target_compile_definitions(test_gfx_gpp_rt_before_init_l4 PRIVATE
    GFX_TEST_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/corpus")
  target_include_directories(test_gfx_gpp_rt_before_init_l4 SYSTEM PRIVATE
    "${SCORE_ROOT_SOURCE_DIR}/3rdparty/avendish/examples"
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-gfx"
    "${SCORE_ROOT_BINARY_DIR}/src/plugins/score-plugin-gfx"
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-avnd"
    "${SCORE_ROOT_BINARY_DIR}/src/plugins/score-plugin-avnd"
    $<TARGET_PROPERTY:score_plugin_avnd,INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:score_plugin_gfx,INCLUDE_DIRECTORIES>)
endif()

# An unbound raw raster image or texture gets a placeholder of the declared
# scalar class (float / uint / sint) and shape (agent A5).
score_add_gfx_test(raw_raster_placeholder_format_a5 GfxRawRasterPlaceholderFormatA5.cpp)

# An unsized texture inlet takes the size of the inlets it renders into, down
# the chain, with fan-out and after a runtime size change (agent L4).
score_add_gfx_test(inlet_size_inherit_l4 GfxInletSizeInheritL4.cpp)

# A gpp compute node as the output node with two image inputs: every input
# after the first had no resource update batch (agent L4).
if(TARGET score_plugin_avnd)
  score_add_test(test_gfx_compute_output_two_images_l4
    SOURCES GfxComputeOutputTwoImagesL4.cpp
    GUI
    PLUGINS score_plugin_gfx score_plugin_avnd score_plugin_scenario score_lib_process
    LIBS test_gfx_engine_glue)
  target_compile_definitions(test_gfx_compute_output_two_images_l4 PRIVATE
    GFX_TEST_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/corpus")
  target_include_directories(test_gfx_compute_output_two_images_l4 SYSTEM PRIVATE
    "${SCORE_ROOT_SOURCE_DIR}/3rdparty/avendish/examples"
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-gfx"
    "${SCORE_ROOT_BINARY_DIR}/src/plugins/score-plugin-gfx"
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-avnd"
    "${SCORE_ROOT_BINARY_DIR}/src/plugins/score-plugin-avnd"
    $<TARGET_PROPERTY:score_plugin_avnd,INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:score_plugin_gfx,INCLUDE_DIRECTORIES>)
endif()

# LAYER / RESOLVE / QUEUE of raw raster shaders: parser, private layer targets,
# shader-authored resolve with depth write or depth input, weighted blended OIT
# built in shader code, transparent-after-opaque ordering (agents S1, LR).
score_add_gfx_test(transparency_s1 GfxTransparencyS1.cpp)

# Booting the GUI application and rendering on it, repeated, keeps the number
# of open file descriptors stable (agent M1).
score_add_gfx_test(fd_stable_m1 GfxFdStableM1.cpp)

# A buffer retired outside a frame (a CSF regrowing during init) outlives the
# initial batch that still names it (agent M1).
score_add_gfx_test(outside_frame_release_m1 GfxOutsideFrameReleaseM1.cpp)

# Every texture a live pipeline samples has the view type its shader declares,
# including after a cable into an array input is cut (agent M1).
score_add_gfx_test(bound_view_type_m1 GfxBoundViewTypeM1.cpp)

# Placeholders swapped for an upstream buffer and orphaned adopted buffers
# dropped during init outlive the initial batch that names them (agent F1).
score_add_gfx_test(outside_frame_release_f1 GfxOutsideFrameReleaseF1.cpp)

# Extract buffer (by name) in Buffer mode reads an auxiliary of a 0-vertex
# geometry, like Scene Preprocessor's splat-only carrier (agent F2).
if(TARGET score_plugin_threedim AND TARGET score_plugin_avnd)
  score_plugin_hidden_sources(_extract_aux_carrier_f2_hidden
      "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-threedim/Threedim/ExtractBuffer2.cpp")
  score_add_test(test_gfx_extract_buffer_aux_carrier_f2
    SOURCES GfxExtractBufferAuxCarrierF2.cpp ${_extract_aux_carrier_f2_hidden}
    GUI
    PLUGINS score_plugin_gfx score_plugin_avnd score_plugin_scenario score_lib_process
    LIBS test_gfx_engine_glue)
  target_compile_definitions(test_gfx_extract_buffer_aux_carrier_f2 PRIVATE
    GFX_TEST_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/corpus")
  target_include_directories(test_gfx_extract_buffer_aux_carrier_f2 SYSTEM PRIVATE
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

# A CPU filter unpremultiplies the readback of its input target; a
# gpu_texture_output declaring premultiplied is drawn with the premultiplied
# over (agent F2).
if(TARGET score_plugin_avnd)
  score_add_test(test_gfx_cpu_filter_premultiplied_f2
    SOURCES GfxCpuFilterPremultipliedF2.cpp
    GUI
    PLUGINS score_plugin_gfx score_plugin_avnd score_plugin_scenario score_lib_process
    LIBS test_gfx_engine_glue)
  target_compile_definitions(test_gfx_cpu_filter_premultiplied_f2 PRIVATE
    GFX_TEST_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/corpus")
  target_include_directories(test_gfx_cpu_filter_premultiplied_f2 SYSTEM PRIVATE
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-gfx"
    "${SCORE_ROOT_BINARY_DIR}/src/plugins/score-plugin-gfx"
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-avnd"
    "${SCORE_ROOT_BINARY_DIR}/src/plugins/score-plugin-avnd"
    $<TARGET_PROPERTY:score_plugin_avnd,INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:score_plugin_gfx,INCLUDE_DIRECTORIES>)
endif()

# Merge Geometries negates tangent.w under a mirroring transform, CPU and GPU
# bakes (agent F2).
score_add_gfx_test(merge_geometries_tangent_mirror_f2 MergeGeometriesTangentMirrorF2.cpp)

# WindowDevice::grabTo stops cleanly when its device is disconnected or deleted
# from inside the grab's event pump (agent F2).
score_add_gfx_process_test(grab_teardown_f2 GfxGrabTeardownF2.cpp)

# $COUNT_ / $BYTESIZE_ of a raw raster's INPUTS storage_input / uniform_input
# resolve from the buffer bound through its Buffer edge (agent F1).
score_add_gfx_test(raw_raster_input_count_f1 GfxRawRasterInputCountF1.cpp)

# A raw raster's Enable-blend factors are not applied on top of the colour its
# ALPHA straight + COMPOSITE multiply/screen wrapper premultiplied (agent F1).
score_add_gfx_test(raw_raster_user_blend_f1 GfxRawRasterUserBlendF1.cpp)

# C6: a shader's own #define PI compiles on every stage kind, and a render
# that throws neither escapes RenderList::render nor leaves the QRhi frame open
# (92170740b1) (agent F3).
score_add_gfx_test(define_pi_frame_leak_f3 GfxDefinePiFrameLeakF3.cpp)

# C6: a geometry filter that #defines PI still bakes through a fulldome
# ModelDisplay, whose projection code once declared PI (agent F3).
if(TARGET score_plugin_threedim AND TARGET score_plugin_avnd)
  set(_f3_md "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-threedim/Threedim")
  score_plugin_hidden_sources(_f3_md_hidden
      "${_f3_md}/Primitive.cpp"
      "${_f3_md}/ModelDisplay/ModelDisplayNode.cpp")
  score_add_test(test_gfx_define_pi_model_display_f3
    SOURCES
      GfxDefinePiModelDisplayF3.cpp
      ${_f3_md_hidden}
    GUI
    PLUGINS score_plugin_gfx score_plugin_avnd score_plugin_scenario score_lib_process
    LIBS test_gfx_engine_glue)
  target_compile_definitions(test_gfx_define_pi_model_display_f3 PRIVATE
    GFX_TEST_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/corpus"
    F3_THREEDIM_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../threedim")
  target_include_directories(test_gfx_define_pi_model_display_f3 SYSTEM PRIVATE
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

# A render entry point reached inside a frame (renderFrames, a pumped render
# clock, a second RenderList::render) does nothing (agent G9).
score_add_gfx_test(render_reentry_g9 GfxRenderReentryG9.cpp)

# Single-cable texture inlets: Texture Info reads the upstream texture, no
# inlet render target; multi-cable inlets still mix (agent SC).
if(TARGET score_plugin_threedim AND TARGET score_plugin_avnd)
  score_add_test(test_gfx_single_cable_inlet_sc
    SOURCES GfxSingleCableInletSC.cpp
    GUI
    PLUGINS score_plugin_gfx score_plugin_avnd score_plugin_scenario score_lib_process
    LIBS test_gfx_engine_glue)
  target_compile_definitions(test_gfx_single_cable_inlet_sc PRIVATE
    GFX_TEST_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/corpus")
  target_include_directories(test_gfx_single_cable_inlet_sc SYSTEM PRIVATE
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

# audioFFT / audioHistogram textures for non-power-of-two audio buffers:
# finite texels, DC..nyquist span, sine peak bin, stable histogram rows
# next to a waveform input (agent FF).
score_add_gfx_test(audio_spectrum_ff GfxAudioSpectrum.cpp)

# ossia::fft zero-pads a non-power-of-two input up to its plan size (agent E2).
score_add_test(test_gfx_ossia_fft_tail_e2
  SOURCES OssiaFftTailE2.cpp)

# ISF audio upload: an odd-length waveform fills its whole texture, and the
# upload runs once per frame with several output edges (agent E2).
score_add_gfx_test(audio_upload_e2 GfxAudioUploadE2.cpp)

# A raw raster's geometry INPUT adds no node port (volumetric_composite shape:
# DEPTH:true image, uniform block, controls, AUXILIARY) (agent E2).
score_add_gfx_test(raw_raster_depth_input_e2 GfxRawRasterDepthInputE2.cpp)

# textureLod on an ISF cubemap input reads the requested mip of a mipmapped
# cube published by a halp object (agent E2).
if(TARGET score_plugin_avnd)
  score_add_test(test_gfx_cubemap_mip_e2
    SOURCES GfxCubemapMipE2.cpp
    GUI
    PLUGINS score_plugin_gfx score_plugin_avnd score_plugin_scenario score_lib_process
    LIBS test_gfx_engine_glue)
  target_compile_definitions(test_gfx_cubemap_mip_e2 PRIVATE
    GFX_TEST_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/corpus")
  target_include_directories(test_gfx_cubemap_mip_e2 SYSTEM PRIVATE
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-gfx"
    "${SCORE_ROOT_BINARY_DIR}/src/plugins/score-plugin-gfx"
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-avnd"
    "${SCORE_ROOT_BINARY_DIR}/src/plugins/score-plugin-avnd"
    $<TARGET_PROPERTY:score_plugin_avnd,INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:score_plugin_gfx,INCLUDE_DIRECTORIES>)
endif()

# A CSF SIZE reading $COUNT_ of a read_only upstream auxiliary logs no ERR232
# before the upstream binds it; undeclared names still log (agent E2).
score_add_gfx_test(csf_pending_aux_count_e2 GfxCsfPendingAuxCountE2.cpp)

# isf::descriptor's VSA background colour defaults to transparent (agent E2).
score_add_test(test_gfx_isf_descriptor_defaults_e2
  SOURCES IsfDescriptorDefaultsE2.cpp
  PLUGINS score_plugin_gfx)

# Skinning through the Scene Preprocessor (joints_0 / weights_0 attributes,
# joint_matrices, skeleton_offset) and the integer vertex-input fallback
# (agent E2).
score_add_gfx_test(scene_skinning_e2 GfxSceneSkinningE2.cpp)
target_sources(test_gfx_scene_skinning_e2 PRIVATE
  "${SCORE_ROOT_SOURCE_DIR}/3rdparty/OffsetAllocator/offsetAllocator.cpp")
target_include_directories(test_gfx_scene_skinning_e2 PRIVATE
  "${SCORE_ROOT_SOURCE_DIR}/3rdparty/OffsetAllocator")

# Posed skeletons survive a Scene Preprocessor input shared with a Camera or a
# Light (merge_scenes carries scene_state.skeletons) (agent MS).
score_add_gfx_test(scene_merge_skeletons_ms GfxSceneMergeSkeletonsMS.cpp)
target_sources(test_gfx_scene_merge_skeletons_ms PRIVATE
  "${SCORE_ROOT_SOURCE_DIR}/3rdparty/OffsetAllocator/offsetAllocator.cpp")
target_include_directories(test_gfx_scene_merge_skeletons_ms PRIVATE
  "${SCORE_ROOT_SOURCE_DIR}/3rdparty/OffsetAllocator")

# Cubemap Loader layouts (equirectangular, horizontal and vertical cross) read
# back through a cubemap_view-convention unwrap. CubemapLoader is
# hidden-visibility inside the plug-in, so it is compiled in directly.
if(TARGET score_plugin_threedim AND TARGET score_plugin_avnd)
  score_plugin_hidden_sources(_cml_e3_hidden
      "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-threedim/Threedim/CubemapLoader.cpp")
  score_add_test(test_gfx_cubemap_loader_orientation_e3
    SOURCES
      GfxCubemapLoaderOrientationE3.cpp
      ${_cml_e3_hidden}
    GUI
    PLUGINS score_plugin_gfx score_plugin_avnd score_plugin_scenario score_lib_process
    LIBS test_gfx_engine_glue)
  target_compile_definitions(test_gfx_cubemap_loader_orientation_e3 PRIVATE
    GFX_TEST_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/corpus")
  target_include_directories(test_gfx_cubemap_loader_orientation_e3 SYSTEM PRIVATE
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

# A raw raster with two outgoing edges patches the geometry-matched uniform and
# storage buffers into the SRB of every pass. Threedim::Cube is
# hidden-visibility, so Primitive.cpp is compiled in directly.
if(TARGET score_plugin_threedim AND TARGET score_plugin_avnd)
  score_plugin_hidden_sources(_rr2e_e3_hidden
      "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-threedim/Threedim/Primitive.cpp")
  score_add_test(test_gfx_rawraster_two_edge_geometry_buffers_e3
    SOURCES
      GfxRawRasterTwoEdgeGeometryBuffersE3.cpp
      ${_rr2e_e3_hidden}
    GUI
    PLUGINS score_plugin_gfx score_plugin_avnd score_plugin_scenario score_lib_process
    LIBS test_gfx_engine_glue)
  target_compile_definitions(test_gfx_rawraster_two_edge_geometry_buffers_e3 PRIVATE
    GFX_TEST_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/corpus")
  target_include_directories(test_gfx_rawraster_two_edge_geometry_buffers_e3 SYSTEM PRIVATE
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

# A raw raster reads a PER_LAYER depth array through an IS_ARRAY image input
# with the sampler its INPUTS entry declares.
score_add_gfx_test(rawraster_depth_array_input_e3 GfxRawRasterDepthArrayInputE3.cpp)
