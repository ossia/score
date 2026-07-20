# L3 regression guards for the split/threedim hardening review (REVIEW-THREEDIM).
#
# The score_plugin_threedim symbols are hidden (default visibility), so the
# execution tests COMPILE the specific engine .cpp under test directly into the
# test target (mirroring tests/unit's test_unit_threedim_loaders), pulling the
# plugin's own include set via its INCLUDE_DIRECTORIES property. External
# symbols they reference (score::gfx::GpuResourceRegistry / RenderList) are
# exported by score_plugin_gfx, so those targets link it.
#
# Shader-string / extension / leak-path findings are constant-text or failure-
# path fixes; their honest GPU-free guard reads the SHIPPED engine source and
# asserts the corrected text (THREEDIM_SRC_DIR).
#
# Every target is its own executable so a crash-on-failure on the pre-fix engine
# (the RED-proof, e.g. the OBJ OOB segfault) cannot perturb its neighbours —
# mirroring the isolated gfx finding targets.

set(THREEDIM_DIR "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-threedim/Threedim")
set(THREEDIM_MIKKT "${SCORE_ROOT_SOURCE_DIR}/3rdparty/mikktspace/mikktspace.c")

# Add the engine plugins' full compile include set so a plugin .cpp compiled
# into a test target resolves its headers exactly as the plugin build does.
function(threedim_test_includes tgt)
  target_include_directories(${tgt} SYSTEM PRIVATE
    # The plugins' own source/binary dirs (so <Threedim/...> and <Gfx/...>
    # resolve) — CMAKE_INCLUDE_CURRENT_DIR adds these implicitly to the plugin
    # targets, so they are not in the INCLUDE_DIRECTORIES property below.
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-threedim"
    "${SCORE_ROOT_BINARY_DIR}/src/plugins/score-plugin-threedim"
    "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-gfx"
    "${SCORE_ROOT_BINARY_DIR}/src/plugins/score-plugin-gfx"
    # Deep 3rdparty include set the plugin sources need (mikktspace, avendish,
    # fastgltf, ufbx, vcglib, …).
    $<TARGET_PROPERTY:score_plugin_threedim,INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:score_plugin_gfx,INCLUDE_DIRECTORIES>)
endfunction()

# -----------------------------------------------------------------------------
# DOCUMENTED GAP — finding #11 (equirect->cube per-face UBO): each of the 6 cube
# faces must get its own UBO slot (bound via a distinct dynamic offset) so every
# face samples its own direction. The entire fix lives inside CubemapLoader's
# setupEquirectPipeline() / renderEquirectangular(), both of which require a
# FULLY-INITIALISED score::gfx::RenderList (outputUBO() allocated, shaders
# compiled, and the 6 face QRhiTextureRenderTargets bound to the cube texture)
# driving CubemapLoader through its CpuFilterNode wrapper. The headless fixture
# only constructs RenderLists for score::gfx graph nodes (ISF/CSF/BackgroundNode)
# via Graph::createAllRenderLists — there is no standalone path to build one for
# an avnd 3D CPU-filter node, and a bare RenderList(output,state) is unusable
# until that Graph init runs. Proving the actual defect (all 6 faces come out
# identical pre-fix) further needs a 6-face cubemap render + per-face readback.
# That is real render infrastructure, not a unit seam, so #11 is left as an
# honest gap rather than a faked pass. The sibling clamp fix (#3) IS covered
# because createCubemapTexture() needs only a QRhi.
#
# -----------------------------------------------------------------------------
# Source-text guards (findings #5, #6, #12, #14). Pure Catch2 — no engine link.
foreach(pair
    "shader_strings:ShaderStrings.cpp"       # #5 triplanar + #6 equirect
    "hdr_extensions:HdrExtensions.cpp"       # #12 HDR extension honesty
    "extract_leak:ExtractLeak.cpp")          # #14 partial-init release (source)
  string(REPLACE ":" ";" _p "${pair}")
  list(GET _p 0 _name)
  list(GET _p 1 _src)
  score_add_test(test_threedim_${_name} SOURCES ${_src})
  target_compile_definitions(test_threedim_${_name}
    PRIVATE THREEDIM_SRC_DIR="${THREEDIM_DIR}")
endforeach()

# -----------------------------------------------------------------------------
# #1 CRASH — OBJ loader OOB. Drives ObjFromString directly with malformed .obj.
# Compiles TinyObj.cpp; mikktspace.c (its only out-of-line dependency) is built
# as a separate C static lib so the test target's C++ PCH stays consistent.
add_library(threedim_test_mikktspace STATIC "${THREEDIM_MIKKT}")
target_include_directories(threedim_test_mikktspace SYSTEM PRIVATE
  "${SCORE_ROOT_SOURCE_DIR}/3rdparty/mikktspace")
set_target_properties(threedim_test_mikktspace PROPERTIES
  DISABLE_PRECOMPILE_HEADERS ON
  POSITION_INDEPENDENT_CODE ON
  FOLDER "Tests")

score_add_test(test_threedim_obj_crash
  SOURCES ObjCrash.cpp "${THREEDIM_DIR}/TinyObj.cpp"
  LIBS threedim_test_mikktspace)
threedim_test_includes(test_threedim_obj_crash)

# -----------------------------------------------------------------------------
# #10 correctness — AnimationPlayer Speed advances. Compiles AnimationPlayer.cpp
# (needs Qt Gui for QMatrix4x4/QQuaternion). No GPU.
score_add_test(test_threedim_animation_speed
  SOURCES AnimationSpeed.cpp "${THREEDIM_DIR}/AnimationPlayer.cpp"
  LIBS ${QT_PREFIX}::Gui)
threedim_test_includes(test_threedim_animation_speed)

# #9 correctness — AnimationPlayer must write sampled TRS into skeleton
# joints[] (the data the renderer's FK consumes) so skinned meshes deform.
# Pure state — no GPU.
score_add_test(test_threedim_animation_skinning
  SOURCES AnimationSkinning.cpp "${THREEDIM_DIR}/AnimationPlayer.cpp"
  LIBS ${QT_PREFIX}::Gui)
threedim_test_includes(test_threedim_animation_skinning)

# -----------------------------------------------------------------------------
# #2 CRASH — Instancer instance_count clamp.  #7 — ExtractBuffer2 index sizing.
# #8 — Camera::release resets m_state.  Each compiles its engine .cpp and links
# score_plugin_gfx for the exported GpuResourceRegistry/RenderList symbols (and
# its PUBLIC Qt GuiPrivate include for qrhi_p.h). Pure logic — no GPU / display.
score_add_test(test_threedim_instancer_clamp
  SOURCES InstancerClamp.cpp "${THREEDIM_DIR}/Instancer.cpp"
  PLUGINS score_plugin_gfx)
threedim_test_includes(test_threedim_instancer_clamp)

# #4 CRASH — Instancer must rebuild when ANY Points buffer handle changes (a
# reallocated secondary attribute buffer left a dangling handle -> GPU UAF).
# Pure logic — no GPU.
score_add_test(test_threedim_instancer_stale
  SOURCES InstancerStaleBuffer.cpp "${THREEDIM_DIR}/Instancer.cpp"
  PLUGINS score_plugin_gfx)
threedim_test_includes(test_threedim_instancer_stale)

score_add_test(test_threedim_extract_index
  SOURCES ExtractIndex.cpp "${THREEDIM_DIR}/ExtractBuffer2.cpp"
  PLUGINS score_plugin_gfx)
threedim_test_includes(test_threedim_extract_index)

# #13 correctness — compute-extraction SRB rebuild must stay layout-compatible
# with the pipeline (uniform@0/src@1/out@2). Header-only strategy driven on a
# REAL offscreen QRhi: init()+update()+dispatch, then read the extracted buffer
# back and assert it equals the source normals. GPU — needs DISPLAY; SKIPs where
# compute / non-uniform readback is unavailable.
score_add_test(test_threedim_extract_compute_srb
  SOURCES ExtractComputeSrb.cpp
  GUI
  PLUGINS score_plugin_gfx score_plugin_scenario score_lib_process)
threedim_test_includes(test_threedim_extract_compute_srb)

score_add_test(test_threedim_camera_release
  SOURCES CameraRelease.cpp "${THREEDIM_DIR}/Camera.cpp"
  PLUGINS score_plugin_gfx)
threedim_test_includes(test_threedim_camera_release)

# #3 CRASH — CubemapLoader must clamp the cube face size to the device
# TextureSizeMax (and check create()). Drives createCubemapTexture() directly on
# a REAL offscreen QRhi with an over-limit size and asserts the clamp. GPU —
# needs DISPLAY; SKIPs without a real device. Compiles CubemapLoader.cpp for the
# createCubemapTexture symbol.
score_add_test(test_threedim_cubemap_clamp
  SOURCES CubemapClamp.cpp "${THREEDIM_DIR}/CubemapLoader.cpp"
  GUI
  PLUGINS score_plugin_gfx score_plugin_scenario score_lib_process)
threedim_test_includes(test_threedim_cubemap_clamp)
