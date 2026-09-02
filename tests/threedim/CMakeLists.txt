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
    "extract_leak:ExtractLeak.cpp"           # #14 partial-init release (source)
    "fisheye:FisheyeProjections.cpp")
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

# -----------------------------------------------------------------------------
# The pure scene_state -> scene_state algebra, plus TagAs. Header-inline nodes
# need no engine .cpp; the rest compile theirs in. score_plugin_gfx supplies
# the GpuResourceRegistry / RenderList symbols
# their init/update/release reference — none of which these tests call, so the
# arena slots stay invalid and no GPU is touched.
score_add_test(test_threedim_scene_graph_ops
  SOURCES SceneGraphOps.cpp
    "${THREEDIM_DIR}/SceneGroup.cpp"
    "${THREEDIM_DIR}/SceneSelector.cpp"
    "${THREEDIM_DIR}/SceneDuplicator.cpp"
    "${THREEDIM_DIR}/TagAs.cpp"
    "${THREEDIM_DIR}/PrimitiveCloud/FormatOverride.cpp"
  PLUGINS score_plugin_gfx
  LIBS ${QT_PREFIX}::Gui)
threedim_test_includes(test_threedim_scene_graph_ops)

# CameraArray's six cubemap faces — the producer side of issue #163. Entirely
# header-inline; only Qt Gui is needed for QQuaternion.
score_add_test(test_threedim_camera_array_faces
  SOURCES CameraArrayFaces.cpp
  LIBS ${QT_PREFIX}::Gui)
threedim_test_includes(test_threedim_camera_array_faces)

# Transform3D TRS composition + the TransformHelper free functions.
score_add_test(test_threedim_transform3d
  SOURCES Transform3DCompose.cpp "${THREEDIM_DIR}/Transform3D.cpp"
  PLUGINS score_plugin_gfx)
threedim_test_includes(test_threedim_transform3d)

# ShadowCascadeSetup's cascade math: split scheme, findActiveCamera id
# filtering, the reverse-Z NDC flip and the zPad direction.
score_add_test(test_threedim_shadow_cascades
  SOURCES ShadowCascades.cpp "${THREEDIM_DIR}/ShadowCascadeSetup.cpp"
  LIBS ${QT_PREFIX}::Gui)
threedim_test_includes(test_threedim_shadow_cascades)

# EXPECTED RED. Contracts the engine does not satisfy yet (issue #163's
# one-entry `camera` aux; CameraSwitch's Blend accumulating onto default-
# constructed components). Isolated so an open defect does not mask the
# green suites, and so the entry flips to PASS on the day it is fixed.
score_add_test(test_threedim_known_defects
  SOURCES KnownDefects.cpp
  PLUGINS score_plugin_gfx
  LIBS ${QT_PREFIX}::Gui)
threedim_test_includes(test_threedim_known_defects)
target_compile_definitions(test_threedim_known_defects PRIVATE
  GFX_SRC_DIR="${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-gfx/Gfx")

# #3 CRASH — CubemapLoader must clamp the cube face size to the device
# TextureSizeMax (and check create()). Drives createCubemapTexture() directly on
# a REAL offscreen QRhi with an over-limit size and asserts the clamp. GPU —
# needs DISPLAY; SKIPs without a real device. Compiles CubemapLoader.cpp for the
# createCubemapTexture symbol.
score_plugin_hidden_sources(_cubemap_hidden
  "${THREEDIM_DIR}/CubemapLoader.cpp")
score_add_test(test_threedim_cubemap_clamp
  SOURCES CubemapClamp.cpp ${_cubemap_hidden}
  GUI
  PLUGINS score_plugin_gfx score_plugin_scenario score_lib_process)
threedim_test_includes(test_threedim_cubemap_clamp)

# -----------------------------------------------------------------------------
# ASSET INGESTION + MESH GENERATION. All CPU: parsers, primitive generators and
# the descriptor-building code between them. None of these touch a QRhi, so they
# run headless with no display.

# The attribute-description layer every extraction strategy goes through
# (attributeFormat*/isFloatFormat/findAttribute/attribute_lookup). Header-only;
# score_plugin_gfx supplies the RenderState/RenderList declarations the header
# pulls in.
score_add_test(test_threedim_geometry_attributes
  SOURCES GeometryAttributeLookup.cpp
  PLUGINS score_plugin_gfx)
threedim_test_includes(test_threedim_geometry_attributes)

# Primitive generators (Plane/Cube/Sphere/Icosahedron/Cone/Cylinder/Torus) and
# ArrayToMesh. Both build on vcglib (header-only) and on the shared
# createMesh/loadTriMesh expansion in Primitive.cpp; Qt Gui is needed for the
# QMatrix4x4 in TinyObj.hpp.
score_add_test(test_threedim_primitive_meshes
  SOURCES PrimitiveMeshes.cpp
    "${THREEDIM_DIR}/Primitive.cpp"
    "${THREEDIM_DIR}/ArrayToGeometry.cpp"
  LIBS ${QT_PREFIX}::Gui)
threedim_test_includes(test_threedim_primitive_meshes)

# MagicaVoxel ingestion: Vox.cpp (opengametools is header-only, pulled in by
# Vox.cpp's own OGT_*_IMPLEMENTATION defines) plus the VoxelLoader node that
# wraps it. Malformed .vox fixtures are driven in a forked child, so a parser
# crash is reported instead of killing the executable.
score_add_test(test_threedim_voxel_assets
  SOURCES VoxelAssets.cpp
    "${THREEDIM_DIR}/Vox.cpp"
    "${THREEDIM_DIR}/VoxelLoader.cpp"
  LIBS ${QT_PREFIX}::Gui)
threedim_test_includes(test_threedim_voxel_assets)

# GeometryLoader's extension dispatch and the four parsers behind it: TinyObj
# (needs the mikktspace tangent generator), Ply (miniply) and VcgImporters
# (STL/OFF, vcglib header-only). Also covers GaussianSplatsFromPly.
score_add_test(test_threedim_geometry_loader_formats
  SOURCES GeometryLoaderFormats.cpp
    "${THREEDIM_DIR}/GeometryLoader.cpp"
    "${THREEDIM_DIR}/TinyObj.cpp"
    "${THREEDIM_DIR}/Ply.cpp"
    "${THREEDIM_DIR}/VcgImporters.cpp"
  LIBS ${QT_PREFIX}::Gui miniply mikktspace)
threedim_test_includes(test_threedim_geometry_loader_formats)

# PCLToMesh2 (Pointcloud to mesh): GPU-buffer -> dynamic_gpu_geometry
# descriptor building. Pure CPU state — the buffer handle is opaque — so no
# QRhi / display. Compiles PCLToGeometry.cpp for the out-of-line ctor and
# operator(); Qt Gui supplies QMatrix4x4/QQuaternion for TinyObj.hpp's
# rebuild_transform.
score_add_test(test_threedim_pcl_to_geometry
  SOURCES PCLToGeometryTest.cpp "${THREEDIM_DIR}/PCLToGeometry.cpp"
  LIBS ${QT_PREFIX}::Gui)
threedim_test_includes(test_threedim_pcl_to_geometry)

# InverseKinematics (2-bone analytic solver). Entirely header-inline — pure
# scene_state / QQuaternion math, no engine .cpp and no GPU; Qt Gui supplies
# QQuaternion/QVector3D/QMatrix4x4 (same shape as test_threedim_camera_array_faces).
score_add_test(test_threedim_inverse_kinematics
  SOURCES InverseKinematicsTest.cpp
  LIBS ${QT_PREFIX}::Gui)
threedim_test_includes(test_threedim_inverse_kinematics)

# MaterialOverride's material-table rewrite: clone targeting (All/ByIndex),
# factor/texture override + revert, clone-pointer stability, dirty/version
# algebra. Pure scene_state logic — no GPU, no Qt Gui, no exported gfx
# symbols (rebuild()/operator()() reference no GpuResourceRegistry/RenderList).
score_add_test(test_threedim_material_override
  SOURCES MaterialOverrideTest.cpp "${THREEDIM_DIR}/MaterialOverride.cpp")
threedim_test_includes(test_threedim_material_override)

# SceneGraphFilter: the scene_state -> scene_state culling predicate walker.
# Pure CPU — the whole unit is rebuild()/operator()(); no GPU, no Qt.
score_add_test(test_threedim_scene_graph_filter
  SOURCES SceneGraphFilterTest.cpp "${THREEDIM_DIR}/SceneGraphFilter.cpp")
threedim_test_includes(test_threedim_scene_graph_filter)

# GeometryPacker's CPU-side packing contract: interleave order, output stride,
# per-attribute offsets, vec4 padding, missing-attribute skip, source-location
# arithmetic and index passthrough — all computed in PackedExtractionStrategy::
# init() before any QRhi work (the vertices==0 "Zero output size" guard returns
# first). Compiles GeometryPacker.cpp for buildAttributeSpecs()/specsChanged();
# score_plugin_gfx supplies the exported makeCompute / RenderList symbols the
# header-inline strategy references (never reached at runtime). No GPU/display.
score_add_test(test_threedim_geometry_packer
  SOURCES GeometryPackerTest.cpp "${THREEDIM_DIR}/GeometryPacker.cpp"
  PLUGINS score_plugin_gfx)
threedim_test_includes(test_threedim_geometry_packer)

# Humanoid pose/retarget family: presets, source maps, keypoint/tracker
# adapters and the HumanoidRetarget offset math. Entirely header-inline
# (HumanoidRetarget.hpp and friends), so no engine .cpp TU is compiled in;
# score_plugin_gfx supplies the ossia scene_state/geometry_port declarations,
# same as test_threedim_geometry_attributes. Pure state — no GPU / display.
score_add_test(test_threedim_humanoid_retarget
  SOURCES HumanoidRetargetTest.cpp
  PLUGINS score_plugin_gfx)
threedim_test_includes(test_threedim_humanoid_retarget)

# StrucSynth (Structure Synth): EisenScript -> mesh through the vendored
# libssynth and ObjFromString. The unit's whole surface is the synchronous
# static worker::work() seam, so no threads/GPU/QApplication (libssynth's
# ProgressDialog is a stub). Compiles StructureSynth.cpp (the unit) and
# TinyObj.cpp (ObjFromString); links the vendored `ssynth` static target
# and threedim_test_mikktspace for TinyObj.cpp's genTangSpaceDefault reference
# (never reached: ssynth OBJ output carries no texcoords); Qt Gui for
# QMatrix4x4/QQuaternion in TinyObj.hpp.
score_add_test(test_threedim_structure_synth
  SOURCES StructureSynthTest.cpp
          "${THREEDIM_DIR}/StructureSynth.cpp"
          "${THREEDIM_DIR}/TinyObj.cpp"
  LIBS ssynth threedim_test_mikktspace ${QT_PREFIX}::Gui)
threedim_test_includes(test_threedim_structure_synth)

# BuffersToGeometry / BuffersToGeometry2 (Buffers to geometry): raw GPU
# buffers + layout controls -> dynamic_gpu_geometry descriptor building.
# Pure CPU state — handles are opaque — so no QRhi / display. Compiles both
# engine .cpps for the out-of-line ctors and operator(); score_plugin_gfx
# supplies the include set GeometryToBufferStrategies.hpp pulls in; Qt Gui
# supplies QMatrix4x4/QQuaternion for TransformHelper.hpp / TinyObj.hpp.
score_add_test(test_threedim_buffer_to_geometry
  SOURCES BufferToGeometryTest.cpp
    "${THREEDIM_DIR}/BufferToGeometry.cpp"
    "${THREEDIM_DIR}/BufferToGeometry2.cpp"
  PLUGINS score_plugin_gfx
  LIBS ${QT_PREFIX}::Gui)
threedim_test_includes(test_threedim_buffer_to_geometry)

# TextToMesh: the QRawFont -> QPainterPath -> ear-clip tessellation pipeline
# and its scene_state / mesh-reuse / dirty contract. CPU-only (init/update/
# release never called), but QFont needs a QGuiApplication + font database:
# the test makes one on first use and defaults QT_QPA_PLATFORM=offscreen.
# score_plugin_gfx supplies the exported GpuResourceRegistry/RenderList
# symbols the compiled-in TU references (never reached at runtime).
score_add_test(test_threedim_text_to_mesh
  SOURCES TextToMeshTest.cpp "${THREEDIM_DIR}/TextToMesh.cpp"
  PLUGINS score_plugin_gfx
  LIBS ${QT_PREFIX}::Gui)
threedim_test_includes(test_threedim_text_to_mesh)

# Text to Texture — CPU rasterization of a string into an RGBA8 texture.
# Header-only node (QPainter on QImage — no GPU / QRhi / engine .cpp), so
# only the include set is needed. Qt Gui for QImage/QPainter/QFont; the test
# makes a QGuiApplication on demand. Assertions are font-independent.
score_add_test(test_threedim_text_to_texture
  SOURCES TextToTextureTest.cpp
  LIBS ${QT_PREFIX}::Gui)
threedim_test_includes(test_threedim_text_to_texture)

# SplatLoader ("Splat loader" node, BufferLoader.cpp): the file-port process()
# closure that moves GaussianSplatsFromPly results into node state, and the
# m_changed dirty-flag lifecycle. CPU seam only — init/update/release need a
# live RenderList/QRhi. Compiles BufferLoader.cpp for the node's out-of-line
# members and Ply.cpp for GaussianSplatsFromPly (miniply); score_plugin_gfx
# exports the RenderList symbols BufferLoader.cpp's TU references; Qt Gui
# supplies QMatrix4x4 (TinyObj.hpp) and the QRhi symbols.
score_add_test(test_threedim_buffer_loader
  SOURCES BufferLoaderTest.cpp
    "${THREEDIM_DIR}/BufferLoader.cpp"
    "${THREEDIM_DIR}/Ply.cpp"
  LIBS ${QT_PREFIX}::Gui miniply
  PLUGINS score_plugin_gfx)
threedim_test_includes(test_threedim_buffer_loader)

# InjectBuffer + ExtractSceneBuffer: the aux-buffer injection / scene-slot
# extraction pair. InjectBuffer is pure scene_state algebra (no GPU symbol);
# ExtractSceneBuffer.cpp references GpuResourceRegistry::buffer() and pulls in
# RenderList.hpp, so score_plugin_gfx supplies those exported symbols (never
# reached at runtime — the tests stop at the null-scene guard). GPU-side slot
# resolution is out of scope here.
score_add_test(test_threedim_buffer_inject_extract
  SOURCES BufferInjectExtractTest.cpp
    "${THREEDIM_DIR}/InjectBuffer.cpp"
    "${THREEDIM_DIR}/ExtractSceneBuffer.cpp"
  PLUGINS score_plugin_gfx)
threedim_test_includes(test_threedim_buffer_inject_extract)
