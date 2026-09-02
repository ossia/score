// P1-1 (SPEC-SCENE-RENDER-TESTS.md:813-829) — the scene assembly chain renders
// a lit model, end to end (topology T-A).
//
//   Asset Loader(Box.glb) + Camera + Light + Environment
//        -> Scene Preprocessor -> Render Pipeline -> Window
//
// NO GOLDEN. Every verdict is a closed form or a difference oracle, in the
// order §3.0 asks for. Nothing in this file was blessed from an image.
//
// =============================================================================
// WHY THIS FILE LIVES IN tests/integration/ AND NOT tests/threedim/
// =============================================================================
// The work order named tests/threedim/ThreedimLitSceneTest.cpp. That directory
// is for something else: its own CMakeLists.txt header (tests/threedim/
// CMakeLists.txt:1-18) states that its targets COMPILE one engine .cpp into the
// test binary and drive it in-process, and every one of its 30 entries does
// exactly that — no target there starts the application. Every app-level driver
// in this tree (QProcess + `--no-gui --script` + OSC /script injection) lives in
// tests/integration/: ThreedimRenderTest.cpp, GfxNestedIntervalTest.cpp,
// FrameDeterminismTest.cpp, ShaderSweepScene.cpp. P1-1's own "Drive" clause
// (SPEC:820-821) says "App level, `ThreedimRenderTest.cpp` shape", and that file
// is tests/integration/ThreedimRenderTest.cpp. So: same directory, same
// registration shape, sibling of the file it is modelled on.
//
// =============================================================================
// INTENDED REGISTRATION — tests/integration/CMakeLists.txt
// =============================================================================
// Modelled on the test_integration_gfx_nested_interval block
// (tests/integration/CMakeLists.txt:554-562), which is the ctest-safe shape:
// the test SKIPs itself when the binary, the asset or a display is missing, so
// it can be a real ctest entry rather than a manual harness.
// cmake/ScoreTestRegistrationGuard.cmake FATAL_ERRORs the configure if a .cpp
// under tests/ is reached by no ctest entry, so this block is REQUIRED for the
// tree to configure at all once this file lands.
//
//   # P1-1: the scene assembly chain renders a lit model. Asset Loader(Box.glb)
//   # + Camera + Light + Environment -> Scene Preprocessor -> Render Pipeline
//   # -> Window, judged by a closed-form N.L inequality and a light-on/off
//   # difference oracle -- no golden. Drives the application binary over the OSC
//   # control port, so it must never run concurrently with the other OSC
//   # harnesses (the in-test flock mirrors /tmp/score-harness.lock and
//   # RUN_SERIAL keeps ctest honest). SKIPs itself when the binary, the
//   # out-of-repo asset corpus or a display is missing.
//   if(TARGET score AND TARGET score_plugin_gfx AND TARGET score_plugin_threedim
//      AND NOT EMSCRIPTEN)
//     score_add_test(test_integration_threedim_lit_scene
//       SOURCES ThreedimLitSceneTest.cpp
//       LIBS ${QT_PREFIX}::Gui)
//     target_compile_definitions(test_integration_threedim_lit_scene PRIVATE
//       "SCORE_APP_BINARY=\"$<TARGET_FILE:score>\"")
//     set_tests_properties(test_integration_threedim_lit_scene PROPERTIES
//       TIMEOUT 900 RUN_SERIAL TRUE LABELS "gui")
//   endif()
//
// ctest target name: test_integration_threedim_lit_scene
//
// =============================================================================
// HARDWARE / BACKENDS
// =============================================================================
// Any RHI. The backend is chosen by QSG_RHI_BACKEND, defaulted per platform the
// way GfxNestedIntervalTest.cpp:236-243 does (opengl on Linux, d3d11 on Windows,
// metal on macOS) and overridable with SCORE_TEST_API.
//
// VULKAN IS OUT OF SCOPE for this case, as SPEC:827-829 requires this file to
// say. The spec pins that on "the `UsedWithGenerateMips` abort
// (ThreedimRenderTest.cpp:32-50)". Having read those exact lines: they say the
// abort ("utexD->m_flags.testFlag(QRhiTexture::UsedWithGenerateMips)", a
// generateMips issued on an input texture created without the flag) WAS the
// Vulkan blocker and is now FIXED — "ModelDisplayNode now guards the
// generateMips call on the texture's flag, so a debug Qt Vulkan build renders
// the model pipeline instead of aborting" (ThreedimRenderTest.cpp:45-50). That
// fix is in ModelDisplayNode, a different node from the RenderedRawRasterPipeline
// node this chain ends in, and no one has run this chain on Vulkan. So the spec's
// clause is honoured conservatively: this test does not select Vulkan, and
// SCORE_TEST_API=vulkan is not a supported configuration for it until someone
// measures it. Nothing here is backend-specific in principle — every expected
// pixel value is 0 or 255 (see "GAMMA INDEPENDENCE" below), so there is no ref
// class to gate on and no silent-fallback hazard of the kind
// ThreedimRenderTest.cpp:15-19 has to guard against for its goldens.
//
// The house rule from §3.0 applies: this case's verdict IS a pixel, so it never
// falls back to QT_QPA_PLATFORM=offscreen / the Null backend. It SKIPs instead.
//
// =============================================================================
// THE SCENE, AND WHY IT IS BUILT THE WAY IT IS
// =============================================================================
// Processes go on Score.rootInterval(). A FLOATING BOX NEVER EXECUTES: it is
// `ossia::scenario::get_roots()` (3rdparty/libossia/src/ossia/editor/scenario/
// detail/scenario_execution.cpp:25-39) collecting only syncs with is_start(),
// and only the scenario's own initial sync is ever marked one
// (Scenario/Process/ScenarioModel.cpp:64) — so a Score.createBox box is
// unreachable and is never ticked. This is measured and written up at
// GfxNestedIntervalTest.cpp:12-34.
// The alternative that does execute is Score.startState(scen) +
// Score.createIntervalAfter(...); this case needs no time structure at all, so
// it takes the simpler root-interval route that ThreedimRenderTest.cpp:437-447
// already uses.
//
// Other recipe constraints, all inherited from measured notes at
// GfxNestedIntervalTest.cpp:141-144: device addresses must be "Window:/", and
// only `var` may be used in the injected script (QML scopes const/let inside
// eval).
//
// Process UUIDs are from the halp_meta(uuid) / PROCESS_METADATA declarations:
//   Asset Loader        AssetLoader.hpp:84
//   Camera              Camera.hpp:46
//   Light               Light.hpp:53
//   Environment         EnvironmentLoader.hpp:47
//   Scene Preprocessor  ScenePreprocessor/Metadata.hpp:10
//   Render Pipeline     RenderPipeline/Metadata.hpp:10
//   Window device       (as used by ThreedimRenderTest.cpp:434)
// They are cross-checked against tests/integration/live-edit/scene-storm.js:57-63,
// which drives the same six process types.
//
// All four producers are wired into the preprocessor's SINGLE "Scene In" inlet
// (ScenePreprocessor/Process.cpp:33) and merged there — that is what P1-4
// pinned. The Environment producer publishes no roots; NodeRenderer's merge
// keeps a rootless env-only contribution rather than dropping it
// (NodeRenderer.cpp:488-493, pinned green by P1-4 / commit 4b4b0567b3). It is
// in the chain because SPEC:814-815 names it in the topology; it cannot move a
// pixel here because the shader below never reads the `env` auxiliary.
//
// =============================================================================
// THE SHADER — WRITTEN BY THIS TEST, NOT COMMITTED
// =============================================================================
// §3.4 item 5 sanctions fixtures the test synthesises (the VoxelAssets.cpp /
// GeometryLoaderFormats.cpp pattern). Both stages are emitted into the
// QTemporaryDir; Gfx::RenderPipeline::Model reads the .fs it is handed and picks
// up the sibling .vs by base name (RenderPipeline/Process.cpp:28-42), so the stem
// must contain no dot (QFileInfo::baseName cuts at the FIRST one).
//
// It is NOT the classic_pbr family. Two reasons, both checked:
//   * classic_pbr_*.frag is not in this repository (`grep -rn classic_pbr` finds
//     only comments referring to it) — it ships in the out-of-repo shader
//     library, and a case whose closed form depends on an asset nobody in the
//     tree can read is not a closed form.
//   * The one light-consuming tester that IS reachable, tests-scene/
//     scene-aux-lights.fs in the csf-testers corpus, declares the OLD LightGPU
//     layout {position_type, color_intensity, direction_range, spot_params} and
//     reads `scene_lights.entries[0].position_type.xyz` as a direction. The
//     binding is now the RawLight ARENA laid out as
//     score::gfx::RawLightData (SceneGPUState.hpp:308-343, 64 B), addressed
//     through scene_light_indices — ScenePreprocessorNode.cpp:2695-2707 and
//     :2755-2767. That corpus shader is stale; using it would measure the wrong
//     bytes. (Recorded, not fixed here — it is out of this repository.)
//
// So the shader is authored here, against the layouts this tree declares, and
// its arithmetic is the source of every number asserted below.
//
// FRAGMENT OUTPUT CONTRACT (the whole oracle, in five exact colours):
//
//   background (no fragment)         (  0,   0,   0)  the pass clear colour
//   covered, scene has no light      (255, 255, 255)  white sentinel
//   covered, world light matrix bad  (255, 255,   0)  yellow diagnostic
//   covered, N.L = +1 (faces light)  (255,   0, 255)
//   covered, N.L =  0 (edge-on)      (  0,   0, 255)
//   covered, N.L = -1 (faces away)   (  0, 255, 255)
//
// B = 1.0 on every covered fragment, so "covered" is decidable without knowing
// the lighting answer, and no lit result can be confused with the background.
//
// GAMMA INDEPENDENCE. Every authored channel value is exactly 0.0 or 1.0. 0 and
// 1 are fixed points of the sRGB transfer function and of any pure power-law
// gamma, so these expectations survive whether or not the target is sRGB, and on
// any backend. That is why the tolerances below are 5 codes and not 30, and why
// no ref class needs asserting.
//
// =============================================================================
// THE CLOSED FORMS, AND WHERE EVERY NUMBER COMES FROM
// =============================================================================
// GEOMETRY. Box.glb is the Khronos sample Box: a unit cube spanning
// [-0.5,-0.5,-0.5]..[0.5,0.5,0.5], flat-shaded, ONE mesh, 24 vertices / 36
// indices, POSITION + NORMAL. Not assumed — tests/unit/GltfLoaderTest.cpp:358-400
// asserts all of it against the file's own JSON chunk, and the file is pinned by
// sha256 ed52f719... in threedim-render/fetch-real-assets.sh:25-30. Flat shading
// is what makes this case closed-form: each of the 6 faces carries its own 4
// corners with a single constant normal, so N is CONSTANT across a face and the
// interpolated v_normal cannot produce an intermediate shade anywhere inside it.
//
// CAMERA. eye = (0, 1, 3), target = (0, 0, 0), FOV 60 deg vertical. The FOV is
// a default (Camera.hpp:65-66, halp::range init 60); the eye and the target are
// SET BY THIS TEST, and have to be.
//
// THE C++ DEFAULT IS NOT THE SCORE DEFAULT. Measured, and this is the second
// thing that made the first run of this file red. Camera.hpp:58-62 declares
//     struct Eye : halp::xyz_spinboxes_f32<"Eye", halp::range{-10000,10000,0.}>
//     { Eye() { value = {0.f, 1.f, 3.f}; } } eye;
// so the node's own member initialiser says (0,1,3) while the halp::range init
// -- the only thing score's process model reads when it mints the ControlInlet
// -- says 0, broadcast across x/y/z. The inlet's value is pushed into the node
// on every tick, so the ctor's (0,1,3) is overwritten before the first frame.
// MEASURED by printing the inlet straight out of the injected script:
//     EYEVAL "[0, 0, 0]"    TGTVAL "[0, 0, 0]"    FOVVAL "60"
// With eye == target the Camera's rebuild() takes its degenerate branch
// (Camera.hpp:143-158: forward.lengthSquared() <= 1e-8 -> identity rotation)
// and emits a scene_transform of translation (0,0,0) + identity rotation. The
// flattener stamps that as the camera's worldTransform (SceneGPUState.cpp:
// 543-551) and packCameraUBO inverts it into the view matrix (CameraMath.cpp:
// 13-15), so the shader sees view == identity and cameraPosition == (0,0,0):
// the eye sits at the ORIGIN, i.e. INSIDE the unit cube, whose inner faces then
// cover 100% of the frame. Measured directly with a probe shader that encoded
// the camera UBO into colour: cameraPosition (0,0,0), view[3] (0,0,0), and
// viewProjection's diagonal equal to projection's own, i.e. no view rotation
// either -- while renderSize read (1280,720) and params read (0, 0.1, 1000),
// which is how the same probe proves the block was bound and correctly laid
// out. Note this is WORSE than having no Camera at all: with fs.cameras empty
// the preprocessor synthesises lookAt((0,1,3),(0,0,0)) itself
// (ScenePreprocessorNode.cpp:3668-3679), which is exactly the framing below.
//
// THE CAMERA CAN BE SCRIPTED. An earlier draft of this file claimed it could
// not, on the grounds that EditJsContext::setValue's vec3 overload
// (EditContext.port.cpp:296) needs a QVector3D and Qt.vector3d(x,y,z) returns a
// zeroed vector in the console engine (ThreedimRenderTest.cpp:646-656). That
// conclusion was wrong: there is a SECOND overload taking a plain JS array,
// EditJsContext::setValue(QObject*, QList<qreal>) at EditContext.port.cpp:
// 377-389, whose own doc comment is literally
//     Score.setValue(Score.inlet(Score.find("Javascript"), 0), [ 0, 0.1, 2.0 ])
// MEASURED: `Score.setValue(Score.inlet(cam, "Eye"), [0.0, 1.0, 3.0])` in the
// setup script moves the camera -- the same probe then reads cameraPosition
// (0, 1.0, 3.0) to its 0.063 quantisation, and world_transforms slot 1 carries
// the same translation. The script below therefore sets Eye and Target
// explicitly, and every number stated here is a number this test WRITES rather
// than a default it hopes for. (The Light's Rotation is reachable the same way;
// it is left alone because the derivation wants identity, not because it cannot
// be written.)
//
// From eye (0,1,3) toward the origin with up (0,1,0):
//   forward f = normalize((0,-1,-3))            = (0, -0.316228, -0.948683)
//   right   r = normalize(cross(f, up))         = (1, 0, 0)
//   up      u = cross(r, f)                     = (0, 0.948683, -0.316228)
// The camera sits on the plane x = 0, so the two faces at x = +-0.5 are exactly
// edge-on and project to zero area. The BACK face (-Z) is behind the cube. The
// visible faces are therefore exactly two: +Z (front) and +Y (top).
//
// LIGHT. Threedim::Light defaults: mode Directional (Light.hpp:78-88, init 0),
// colour white, intensity 1 (Light.hpp:93-94), rotation (0,0,0)
// (Light.hpp:137-138). Light::update writes the local direction the shader
// reads: raw.local_direction = (0, 0, -1) with w = the type enum
// (Light.cpp:211-214). Its own comment (Light.cpp:207-210) and Light.hpp:132-134
// state the convention: local -Z is the direction the light POINTS, mapped
// through the node's world matrix. With rotation (0,0,0) the quaternion is
// identity (Light.cpp:243-249), so the world matrix is the identity and
//   world light direction  d = (0, 0, -1)
//   surface-to-light       L = -d = (0, 0, +1)
//
// SHADING, per visible face:
//   +Z face  N = (0, 0, 1)   N.L = +1  ->  (255,   0, 255)
//   +Y face  N = (0, 1, 0)   N.L =  0  ->  (  0,   0, 255)
// So the face the light points at is brighter than the other visible face by
// 255 codes out of 255 in the R channel. The stated margin asserted below is
// >= 245 (96% of full scale) — slack only for 8-bit rounding.
//
// COVERAGE, for the record and for the sanity band below. The offscreen window
// is 1280x720 (OffscreenDevice.hpp:46-49). A face-on 1x1 quad at view depth d
// covers 1/(d*tan(30 deg)) of the frame height. Projecting both faces:
//   +Z face: centre (0,0,0.5), depth 2.68794, |f.N| = 0.948683
//            232 x 232 px face-on, x 0.9487  ->  ~51,000 px  = 5.5% of frame
//   +Y face: centre (0,0.5,0), depth 3.00416, |f.N| = 0.316228
//            208 x 208 px face-on, x 0.3162  ->  ~13,600 px  = 1.5% of frame
//   total coverage ~ 7.0% of 921,600 px; bright:dark ~ 3.75 : 1
// Asserted only as a loose band (1%..40% covered, each class >= 5% of covered)
// because the exact projected area of a tilted quad is not area x cos.
// MEASURED on OpenGL / 1280x720: background 863,630 px, lit (255,0,255) 50,512
// px, unlit (0,0,255) 7,458 px, other 0 -- coverage 57,970 px = 6.29% of the
// frame, bright:dark 6.8:1. The light-OFF run's white sentinel covers 57,970 px
// too, to the pixel, which is the difference oracle's premise made exact.
// The +Z estimate lands within 0.06 points (5.54% predicted, 5.48% measured);
// the +Y one is the ~1.8x optimistic figure the "not area x cos" caveat above
// exists for -- at |f.N| = 0.316 the quad's far edge sits at depth 3.48 and its
// near edge at 2.53, and 1/d^2 over that spread is not the centre-depth value.
// Both classes are far above the 5%-of-covered floor the test asserts, and the
// class populations partition the coverage exactly: `other` is empty, so there
// is no antialiased edge residue to account for even though the GL context
// reports samples=4.
//
// FACE IDENTITY (spatial half). NDC y of each face centre:
//   +Z face  y_v = u.(p-e) = -0.158114, depth 2.68794 -> ndc_y = -0.1019
//   +Y face  y_v = u.(p-e) = +0.474342, depth 3.00416 -> ndc_y = +0.2735
// so with row 0 at the top of a 720-row image the bright face's centroid sits
// near row 397 and the dark face's near row 262 — the LIT face is LOWER in the
// image. This is asserted as an ordering only, and as a CHECK not a REQUIRE.
//
// The row direction of the offscreen readback (RGBA8888 out of
// WindowDevice::grabTo, WindowDevice.cpp:145-151) is now MEASURED and is the
// one assumed above. A probe shader that painted eight horizontal bands keyed
// on int(gl_FragCoord.y / 90.0), with a distinguishable constant in the last
// band, put that band at the TOP of the saved PNG: saved row 0 corresponds to
// gl_FragCoord.y ~ 719, i.e. gl_FragCoord.y counts up from the bottom of the
// stored image, so a larger NDC y is a smaller row index. The +Y face
// (ndc_y = +0.2735) is therefore the upper one and the lit +Z face
// (ndc_y = -0.1019) the lower one, which is exactly the ordering asserted.
// MEASURED centroids: lit 387.9, unlit 264.9, against the 397 / 262 predicted.
// It stays a CHECK: if it ever fails while the class populations hold, the
// finding is about the image Y convention, not about the light.
//
// =============================================================================
// NEGATIVE CONTROL — VERIFIED TO BE REAL, AND POINTING THE RIGHT WAY
// =============================================================================
// SPEC:824-825 proposes: "Flip the light direction sign in `Light.cpp`'s raw
// write -> the bright/dark inequality inverts." That hook is CORRECT. Checked:
//
//   * The exact edit is src/plugins/score-plugin-threedim/Threedim/Light.cpp:213
//         raw.local_direction[2] = -1.f;
//     ->  raw.local_direction[2] = 1.f;
//
//   * It is the only write to that field in the whole product (`grep -rn
//     local_direction src/` returns Light.cpp:211-214, plus the struct's own
//     default member initialiser at SceneGPUState.hpp:312 and two comments).
//
//   * It really does reach this shader. `scene_lights` is bound to the RawLight
//     ARENA directly, not to a repacked copy: ScenePreprocessorNode.cpp:2695-2707
//     pushes renderer.registry().buffer(Arena::RawLight) and :2763-2767 publishes
//     it as the auxiliary named "scene_lights", with :2698-2699 stating the
//     consumer contract this shader implements verbatim — read
//     scene_lights.entries[scene_light_indices.data[i]] and compose the world
//     direction from world_transforms[transform_slot]. The bytes Light.cpp:235
//     uploads ARE the bytes the fragment shader reads.
//
//   * The CPU-side scene_state the Light also publishes (Light.cpp:89-113)
//     carries a rotation quaternion, NOT a direction vector, so it cannot mask
//     the flip.
//
// EFFECT of the flip, closed form: world d becomes (0,0,+1), L becomes (0,0,-1).
//   +Z face  N.L = -1  ->  (0, 255, 255)   the `lit` class empties into `backlit`
//   +Y face  N.L =  0  ->  (0,   0, 255)   unchanged
// The first assertion the flip meets is `REQUIRE(lit.lit > 0)` in section (a),
// and it goes RED on the whole of the +Z face's 50,512 measured pixels moving
// out of `lit`. Being a REQUIRE it ABORTS the case there, so the assertions
// after it -- CHECK(backlit ~ 0), the >= 245 R margin, the spatial ordering,
// and the whole of the light-on/off difference oracle in section (b) -- are not
// reached rather than "still green"; stated plainly because a control run's
// summary will show fewer assertions, not a mix of red and green ones.
// Everything BEFORE it is unaffected, and that is what makes this a control and
// not a smoke test: `REQUIRE(lit.white == 0)`, the coverage band, the
// background partition and the residue bound all stay GREEN, because the flip
// moves a normal's sign and touches neither the light's existence
// (scene_counts.light_count stays 1) nor one pixel of coverage.
//
// It also cannot be passed by accident. The `lit` and `backlit` classes are
// disjoint 3-channel exact matches (255,0,255) vs (0,255,255), and the measured
// run has 50,512 in the first and 0 in the second; there is no tolerance band
// that could absorb the swap.
//
// ONE CAVEAT ON THE CONTROL, recorded so a green run is not misread: Light::init
// seeds the same slot with a default-constructed RawLightData
// (Light.cpp:178-180), whose local_direction default is ALSO (0,0,-1)
// (SceneGPUState.hpp:312). Flipping only line 213 is sufficient because
// Light::update (Light.cpp:195-235) runs every frame and overwrites the seed. If
// the control does NOT go red, that is evidence Light::update is not running for
// this chain — itself a finding, not a reason to weaken the control.
//
// =============================================================================
// ASSETS
// =============================================================================
// Box.glb comes from tests/integration/threedim-render/fetch-real-assets.sh
// (URL + sha256 pinned there, licence CC0 1.0). It is NEVER committed. The skip
// convention is copied verbatim from the P1-5 loader test — assets_dir() and
// fetch_hint at tests/unit/GltfLoaderTest.cpp:143-153, used at :367 and :487.

#include <QElapsedTimer>
#include <QFile>
#include <QImage>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QUdpSocket>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#if defined(Q_OS_UNIX)
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace
{
QString appBinary()
{
#if defined(SCORE_APP_BINARY)
  return QStringLiteral(SCORE_APP_BINARY);
#else
  return {};
#endif
}

//! Default destination of tests/integration/threedim-render/fetch-real-assets.sh
//! ("${1:-$HOME/ossia/threedim-assets}", fetch-real-assets.sh:11).
//! Same helper as tests/unit/GltfLoaderTest.cpp:143-148.
QString assetsDir()
{
  const char* home = ::getenv("HOME");
  return QString::fromUtf8(home ? home : "")
         + QStringLiteral("/ossia/threedim-assets");
}

//! Byte-for-byte the hint tests/unit/GltfLoaderTest.cpp:151-153 prints.
constexpr const char* kFetchHint
    = "run tests/integration/threedim-render/fetch-real-assets.sh to fetch "
      "the corpus into ~/ossia/threedim-assets";

// halp_meta(uuid) / PROCESS_METADATA declarations, cross-checked against
// tests/integration/live-edit/scene-storm.js:57-63.
constexpr auto kUuidWindow = "5a181207-7d40-4ad8-814e-879fcdf8cc31";
constexpr auto kUuidAsset = "2f6a8c41-7d93-4e5b-b1c8-4e3f9a7d2c5b";
constexpr auto kUuidCamera = "4c91b5e2-8d76-4ab3-9f14-6e0d8b3a2c57";
constexpr auto kUuidLight = "9f3c1a5e-4b7d-4e2a-8c5f-1d6e0b9a3c7f";
constexpr auto kUuidEnv = "d3f5a8c1-8b47-4e91-9c2d-6f1a9b5e3c82";
constexpr auto kUuidPreproc = "a8f2c6d0-1b4e-4c7a-9d3f-5e8b2c1a7f0d";
constexpr auto kUuidRenderPipeline = "dbfc2101-40d7-4807-8804-571e88992e7e";

// ---------------------------------------------------------------------------
// The synthesised shader pair. Layouts are transcribed from this tree:
//
//   RawLight  <- score::gfx::RawLightData      SceneGPUState.hpp:308-343 (64 B)
//                color[4] / local_direction[4] / range_cone[4] /
//                {shadow_enabled, decay_mode, transform_slot, normal_bias}
//                -> the last quad is read as uvec4, .z = transform_slot
//                   (the field Light.cpp:231-233 stamps).
//   PerDraw   <- PerDrawGPU                    ScenePreprocessorNode.cpp:40-49
//                model mat4 / normal mat4 / {material_index, tag_hash,
//                transform_slot, skeleton_offset} = 144 B (its static_assert).
//   camera    <- score::gfx::CameraUBOData     CameraMath.hpp:23-32 (240 B)
//   counts    <- SceneCountsUBO                ScenePreprocessorNode.cpp:328-335
//
// KINDS ARE NOT FREE. `camera` is declared TYPE "uniform" and everything else
// TYPE "storage" because that is how the PRODUCER allocated each buffer, and a
// mismatch resolves to zeros rather than to an error:
//   * m_camerasBuffer is Dynamic + UniformBuffer (ScenePreprocessorNode.cpp:
//     3715-3721), so `camera` MUST be a uniform block.
//   * m_sceneCountsBuffer is Static + StorageBuffer ONLY, with no UBO half --
//     "QRhi forbids Dynamic + StorageBuffer, and Static + UniformBuffer fails
//     create() on D3D11 and GLES ... All bundled shaders therefore declare
//     scene_counts as a storage buffer" (ScenePreprocessorNode.cpp:4152-4168).
//     MEASURED, and this is what made the first run of this file red: declaring
//     scene_counts as "uniform" still NAME-MATCHES and still adopts the
//     preprocessor's handle (IsfBindingsBuilder.cpp:995-1010, the store.ubos
//     loop), but the QRhi GL backend emulates uniform blocks out of the
//     QRhiBuffer's CPU-side mirror, which a StorageBuffer-only buffer does not
//     have -- so every member read back as 0, light_count included, and the
//     shader took its "no light" branch on a scene that had a light. The
//     product's own note at :4165-4168 says a shader author "may still declare
//     TYPE: uniform, but then owns the backend-support question"; this file
//     does not, and declares storage/read_only like every bundled shader and
//     like the green sibling GfxLightLiveTest.cpp:369-376.
//
// Auxiliary NAMEs must match the ones the preprocessor publishes; the member
// names inside each block are ours to choose (the same freedom the corpus's
// syn-scene-xform.fs uses). Published names: ScenePreprocessorNode.hpp:16-31 and
// the push_back list at ScenePreprocessorNode.cpp:2760-2827.
//
// per_draws is indexed at [0] rather than [gl_DrawID]: Box.glb yields exactly
// ONE mesh (asserted at tests/unit/GltfLoaderTest.cpp:358-362), so there is
// exactly one draw and gl_DrawID is 0 for every fragment. Indexing 0 keeps this
// case off the gl_DrawID / shader-draw-parameters plumbing, which it does not
// set out to test.
//
// PIPELINE_STATE: depth test + write on, culling OFF. Culling is off on purpose
// — the winding the flattened glTF arrives in is not something this test has
// measured, and with the depth buffer on, the near face wins regardless. The
// project-wide depth convention is reverse-Z + GREATER compare + clear 0.0
// (CameraMath.hpp:47-60, and depthClearForCompare at PipelineStateHelpers.cpp:
// 49-63 returning 0.0 for anything but Less/LessOrEqual), which is exactly what
// RenderedRawRasterPipelineNode.cpp:3175-3181 uses when the descriptor declares
// no DEPTH_COMPARE. So DEPTH_COMPARE is deliberately NOT declared here: the pass
// keeps the engine's own convention, matched to the projection the engine's own
// camera packer built. TOPOLOGY is likewise not declared, so the material-mode
// default (triangles) applies — the precedence rule pinned by P1-11 / 174a0798ad.
constexpr auto kFragmentShader = R"SHADER(/*{
  "DESCRIPTION": "P1-1: the scene chain's own light, measured. Shades the flattened scene by the signed N.L of the first live light, encoded so that BOTH signs are visible: R = max(N.L, 0), G = max(-N.L, 0), B = 1 on every covered fragment. Every authored channel is exactly 0.0 or 1.0, which are fixed points of sRGB and of any power-law gamma, so the expectations are backend- and colourspace-independent. White = the scene carries no light at all; yellow = the light's world transform is degenerate (a named diagnostic, never a silent fallback).",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST-SYNTHETIC", "TEST-SCENE"],
  "PIPELINE_STATE": { "DEPTH_TEST": true, "DEPTH_WRITE": true, "CULL_MODE": "none" },
  "VERTEX_INPUTS": [
    { "TYPE": "vec3", "NAME": "position" },
    { "TYPE": "vec3", "NAME": "normal" }
  ],
  "VERTEX_OUTPUTS": [ { "TYPE": "vec3", "NAME": "v_normal" } ],
  "FRAGMENT_INPUTS": [ { "TYPE": "vec3", "NAME": "v_normal" } ],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "TYPES": [
    { "NAME": "RawLight", "LAYOUT": [
        { "NAME": "color_intensity",     "TYPE": "vec4"  },
        { "NAME": "local_direction_type","TYPE": "vec4"  },
        { "NAME": "range_cone",          "TYPE": "vec4"  },
        { "NAME": "slots",               "TYPE": "uvec4" }
    ] },
    { "NAME": "PerDraw", "LAYOUT": [
        { "NAME": "model",         "TYPE": "mat4"  },
        { "NAME": "normal_matrix", "TYPE": "mat4"  },
        { "NAME": "slots",         "TYPE": "uvec4" }
    ] }
  ],
  "INPUTS": [
    { "NAME": "camera", "TYPE": "uniform", "VISIBILITY": "vertex",
      "LAYOUT": [
        { "NAME": "view",           "TYPE": "mat4" },
        { "NAME": "projection",     "TYPE": "mat4" },
        { "NAME": "viewProjection", "TYPE": "mat4" },
        { "NAME": "cameraPosition", "TYPE": "vec4" },
        { "NAME": "renderSize",     "TYPE": "vec4" },
        { "NAME": "params",         "TYPE": "vec4" }
      ]
    },
    { "NAME": "per_draws", "TYPE": "storage", "ACCESS": "read_only",
      "VISIBILITY": "vertex",
      "LAYOUT": [ { "NAME": "data", "TYPE": "PerDraw[]" } ]
    },
    { "NAME": "scene_counts", "TYPE": "storage", "ACCESS": "read_only",
      "VISIBILITY": "fragment",
      "LAYOUT": [
        { "NAME": "light_count",    "TYPE": "uint" },
        { "NAME": "material_count", "TYPE": "uint" },
        { "NAME": "draw_count",     "TYPE": "uint" },
        { "NAME": "pad0",           "TYPE": "uint" }
      ]
    },
    { "NAME": "scene_lights", "TYPE": "storage", "ACCESS": "read_only",
      "VISIBILITY": "fragment",
      "LAYOUT": [ { "NAME": "entries", "TYPE": "RawLight[]" } ]
    },
    { "NAME": "scene_light_indices", "TYPE": "storage", "ACCESS": "read_only",
      "VISIBILITY": "fragment",
      "LAYOUT": [ { "NAME": "data", "TYPE": "uint[]" } ]
    },
    { "NAME": "world_transforms", "TYPE": "storage", "ACCESS": "read_only",
      "VISIBILITY": "fragment",
      "LAYOUT": [ { "NAME": "data", "TYPE": "mat4[]" } ]
    }
  ]
}*/

void main()
{
    // No light reached the merged scene at all: white. This is the light-OFF
    // reference frame, and it is also the coverage mask for the lit run.
    if(scene_counts.light_count == 0u)
    {
        isf_FragColor = vec4(1.0, 1.0, 1.0, 1.0);
        return;
    }

    // ScenePreprocessorNode.cpp:2755-2760: iterate 0..light_count and read
    // scene_lights.entries[scene_light_indices.data[i]]. One light here.
    uint li = scene_light_indices.data[0];
    vec3 dl = scene_lights.entries[li].local_direction_type.xyz;
    uint xf = scene_lights.entries[li].slots.z;

    // ScenePreprocessorNode.cpp:2698-2699: compose the world-space direction
    // from world_transforms[transform_slot].
    vec3 dw = mat3(world_transforms.data[xf]) * dl;
    float len = length(dw);
    if(len < 1e-5)
    {
        // Degenerate world matrix. Reported as its own colour rather than
        // normalised away, so a world-transform failure is named instead of
        // silently poisoning the light verdict.
        isf_FragColor = vec4(1.0, 1.0, 0.0, 1.0);
        return;
    }

    vec3 L = -(dw / len);                 // surface -> light
    vec3 N = normalize(v_normal);
    float ndl = dot(N, L);
    isf_FragColor = vec4(max(ndl, 0.0), max(-ndl, 0.0), 1.0, 1.0);
}
)SHADER";

constexpr auto kVertexShader = R"SHADER(void main()
{
    // Box.glb is a single mesh, hence a single draw: gl_DrawID == 0.
    mat4 M = per_draws.data[0].model;
    v_normal = mat3(per_draws.data[0].normal_matrix) * normal;
    gl_Position = clipSpaceCorrMatrix * camera.viewProjection * M * vec4(position, 1.0);
}
)SHADER";

// ---------------------------------------------------------------------------
// Driver. Copied from the known-good tests/integration/GfxNestedIntervalTest.cpp
// (:224-316): start the app on a setup script that defines phase functions and
// leaves the event loop FREE, then inject those calls over OSC. The busy-wait
// alternative is measured-bad — a JS `while` loop blocks the main thread and the
// asynchronous work (there, a mid-play interval start; here, the AssetLoader's
// file worker) never completes (GfxNestedIntervalTest.cpp:90-108).

struct Run
{
  bool started{false};
  bool sawReady{false};
  bool crashed{true};
  int exitCode{-1};
  QString log;
  QString rendererLine;
};

//! One `/script s <code>` datagram to the app's LocalTree device on udp/6666 --
//! byte-identical to what live-edit-sweep.sh's oscsend produces
//! (GfxNestedIntervalTest.cpp:208-218).
void sendScript(QUdpSocket& sock, const QByteArray& code)
{
  auto pad4 = [](QByteArray b) {
    b.append('\0');
    while(b.size() % 4)
      b.append('\0');
    return b;
  };
  const QByteArray dgram = pad4("/script") + pad4(",s") + pad4(code);
  sock.writeDatagram(dgram, QHostAddress::LocalHost, 6666);
}

Run runPhased(
    const QString& jsPath, const std::vector<std::pair<int, QByteArray>>& phases)
{
  auto env = QProcessEnvironment::systemEnvironment();
  // Offscreen render target: with a mapped window the grab reads the SCREEN at
  // its geometry, i.e. the desktop, which is never blank.
  env.insert("SCORE_FORCE_OFFSCREEN_WINDOW", "Window");
  env.insert("SCORE_AUDIO_BACKEND", "dummy");
  env.insert("SCORE_DISABLE_AUDIOPLUGINS", "1");
  // Make QRhi print which renderer it actually got, so a run can be diagnosed.
  // Not a gate: every expectation here is 0-or-255 and backend-independent.
  env.insert("QT_LOGGING_RULES", "qt.rhi.general=true");
  env.insert("QT_FORCE_STDERR_LOGGING", "1");
  env.insert("QT_ASSUME_STDERR_HAS_CONSOLE", "1");
  // The platform's own backend, as GfxNestedIntervalTest.cpp:236-243. Vulkan is
  // out of scope for this case; see the header.
#if defined(_WIN32)
  constexpr auto defaultApi = "d3d11";
#elif defined(__APPLE__)
  constexpr auto defaultApi = "metal";
#else
  constexpr auto defaultApi = "opengl";
#endif
  env.insert("QSG_RHI_BACKEND", qEnvironmentVariable("SCORE_TEST_API", defaultApi));
  env.remove("QT_QPA_PLATFORM");

  Run r;

#if defined(Q_OS_UNIX)
  // OSC port 6666 is machine-global: serialize against live-edit-sweep.sh and
  // the other harnesses by taking the very same advisory lock.
  const int lockFd = ::open("/tmp/score-harness.lock", O_CREAT | O_RDWR, 0666);
  if(lockFd >= 0 && ::flock(lockFd, LOCK_EX) != 0)
  {
    // Not fatal; the run just risks stray 6666 traffic.
  }
#endif

  QProcess p;
  p.setProcessEnvironment(env);
  p.setProcessChannelMode(QProcess::MergedChannels);
  p.start(appBinary(), {"--no-gui", "--no-restore", "--script", jsPath});

  auto pump = [&](int ms) {
    QElapsedTimer t;
    t.start();
    do
    {
      p.waitForReadyRead(50);
      r.log += QString::fromUtf8(p.readAll());
    } while(t.elapsed() < ms && p.state() == QProcess::Running);
  };

  if(p.waitForStarted(30000))
  {
    r.started = true;
    QElapsedTimer boot;
    boot.start();
    while(boot.elapsed() < 60000 && p.state() == QProcess::Running
          && !r.log.contains("LIT-READY"))
      pump(100);
    r.sawReady = r.log.contains("LIT-READY");

    if(r.sawReady)
    {
      QUdpSocket sock;
      QElapsedTimer t0;
      t0.start();
      for(const auto& [at_ms, code] : phases)
      {
        while(t0.elapsed() < at_ms && p.state() == QProcess::Running)
          pump(50);
        sendScript(sock, code);
      }
    }

    if(!p.waitForFinished(120000))
    {
      p.kill();
      p.waitForFinished(5000);
    }
  }
  r.log += QString::fromUtf8(p.readAll());
  r.crashed
      = p.exitStatus() != QProcess::NormalExit || p.state() != QProcess::NotRunning;
  r.exitCode = p.exitCode();
  for(const auto& line : r.log.split('\n'))
    if(line.contains("qt.rhi.general") && line.contains("RENDERER"))
      r.rendererLine = line.trimmed();

#if defined(Q_OS_UNIX)
  if(lockFd >= 0)
    ::close(lockFd); // releases the flock
#endif
  return r;
}

QString writeFile(const QTemporaryDir& dir, const QString& name, const QByteArray& src)
{
  const QString path = dir.filePath(name);
  QFile f{path};
  REQUIRE(f.open(QIODevice::WriteOnly));
  f.write(src);
  f.close();
  return path;
}

//! The whole scene, as one setup script. `withLight` is the ONLY difference
//! between the two runs of the difference oracle.
QString sceneScript(
    const QString& glb, const QString& fs, const QString& png, bool withLight)
{
  QString s;
  s += "var FL = 705600000;\n"; // flicks per second (TimeVal impl units)
  s += QStringLiteral("Score.createDevice(\"Window\", \"%1\", {});\n").arg(kUuidWindow);
  // The document's default Scenario would only add an empty timeline between
  // the transport and the processes; ThreedimRenderTest.cpp:436 removes it for
  // the same reason before putting everything on the root interval.
  s += "var sc = Score.find(\"Scenario.1\"); if (sc) Score.remove(sc);\n";
  s += "var root = Score.rootInterval();\n";
  s += "Score.setIntervalDuration(root, 120 * FL);\n";

  auto mk = [&](const char* var, const char* uuid, const QString& data, int code) {
    s += QStringLiteral("var %1 = Score.createProcess(root, \"%2\", \"%3\");\n")
             .arg(QString::fromUtf8(var), QString::fromUtf8(uuid), data);
    s += QStringLiteral(
             "if (!%1) { console.log(\"LIT-ERROR: no %1\"); Qt.exit(%2); }\n")
             .arg(QString::fromUtf8(var))
             .arg(code);
  };

  mk("asset", kUuidAsset, glb, 10);
  mk("cam", kUuidCamera, {}, 11);
  // The camera MUST be placed: score mints the Eye / Target inlets from the
  // halp::range init (0), not from Camera::ins::Eye's member initialiser, so
  // the score-side default is eye == target == origin -- a degenerate look-at
  // that leaves the eye inside the cube. See "CAMERA" in the header for the
  // measurement. The JS-array overload of setValue (EditContext.port.cpp:
  // 377-389) is what makes a vec3 control writable from the console engine.
  s += "var eyePort = Score.inlet(cam, \"Eye\");\n";
  s += "var tgtPort = Score.inlet(cam, \"Target\");\n";
  s += "if (!eyePort || !tgtPort) { console.log(\"LIT-ERROR: no camera "
       "Eye/Target inlet\"); Qt.exit(23); }\n";
  s += "Score.setValue(eyePort, [0.0, 1.0, 3.0]);\n";
  s += "Score.setValue(tgtPort, [0.0, 0.0, 0.0]);\n";
  if(withLight)
    mk("light", kUuidLight, {}, 12);
  mk("envp", kUuidEnv, {}, 13);
  mk("pre", kUuidPreproc, {}, 14);
  mk("rp", kUuidRenderPipeline, fs, 15);

  // Every producer into the preprocessor's single "Scene In"
  // (ScenePreprocessor/Process.cpp:33); the merge is P1-4's subject.
  s += "var sin = Score.inlet(pre, \"Scene In\");\n";
  s += "if (!sin) { console.log(\"LIT-ERROR: no Scene In\"); Qt.exit(16); }\n";
  s += "function feed(p, tag) {\n"
       "  var o = Score.outlet(p, 0);\n"
       "  if (!o) { console.log(\"LIT-ERROR: no scene outlet on \" + tag); Qt.exit(17); }\n"
       "  if (!Score.createCable(o, sin)) { console.log(\"LIT-ERROR: no cable from \" + tag); Qt.exit(18); }\n"
       "}\n";
  s += "feed(asset, \"asset\");\n";
  s += "feed(cam, \"camera\");\n";
  if(withLight)
    s += "feed(light, \"light\");\n";
  s += "feed(envp, \"environment\");\n";

  s += "var go = Score.outlet(pre, \"Geometry Out\");\n";
  s += "var gi = Score.inlet(rp, \"Geometry In\");\n";
  s += "if (!go || !gi) { console.log(\"LIT-ERROR: preproc/pipeline port lookup\"); Qt.exit(19); }\n";
  s += "if (!Score.createCable(go, gi)) { console.log(\"LIT-ERROR: no geometry cable\"); Qt.exit(20); }\n";
  s += "var tex = Score.outlet(rp, \"Texture Out\");\n";
  s += "if (!tex) { console.log(\"LIT-ERROR: no Texture Out\"); Qt.exit(21); }\n";
  // Device addresses must be "Window:/" (GfxNestedIntervalTest.cpp:141-142).
  s += "Score.setAddress(tex, \"Window:/\");\n";

  s += "var dev = Score.device(\"Window\");\n";
  s += "if (!dev) { console.log(\"LIT-ERROR: no window device\"); Qt.exit(22); }\n";
  // grabFrame renders N frames synchronously and then reads back
  // (WindowDevice.cpp:234-238), so the grab is not racing vsync.
  s += QStringLiteral(
           "function grab() { dev.grabFrame(4, \"%1\"); console.log(\"MARK-GRAB\"); }\n")
           .arg(png);
  s += "function finish() { Score.stop(); console.log(\"LIT-OK\"); Qt.exit(0); }\n";
  s += "Score.play();\n";
  s += "console.log(\"LIT-READY\");\n";
  return s;
}

// ---------------------------------------------------------------------------
// Verdicts.

//! Slack for 8-bit rounding only: every authored channel is exactly 0.0 or 1.0
//! (see GAMMA INDEPENDENCE in the header), so nothing legitimate lands between.
constexpr int kNear = 5;

struct Classes
{
  bool loaded{false};
  int w{0}, h{0}, total{0};
  int background{0}; //!< ( 0, 0, 0 ) -- the raw-raster pass clear colour
  int white{0};      //!< (255,255,255) -- scene carries no light
  int yellow{0};     //!< (255,255, 0 ) -- degenerate light world matrix
  int lit{0};        //!< (255, 0 ,255) -- N.L = +1, facing the light
  int unlit{0};      //!< ( 0 , 0 ,255) -- N.L =  0, edge-on
  int backlit{0};    //!< ( 0 ,255,255) -- N.L = -1, facing away
  int other{0};      //!< anything else: an intermediate shade or a resample
  double litRowSum{0}, unlitRowSum{0};
  double litRSum{0}, unlitRSum{0}; //!< raw R codes, for the measured margin

  int covered() const noexcept { return white + yellow + lit + unlit + backlit; }
  double coverFrac() const noexcept
  {
    return total > 0 ? double(covered()) / total : 0.;
  }
};

Classes classify(const QString& path)
{
  Classes c;
  QImage img{path};
  if(img.isNull())
    return c;
  img = img.convertToFormat(QImage::Format_RGB888);
  c.loaded = true;
  c.w = img.width();
  c.h = img.height();
  c.total = c.w * c.h;
  const auto lo = [](int v) { return v <= kNear; };
  const auto hi = [](int v) { return v >= 255 - kNear; };
  for(int y = 0; y < c.h; y++)
  {
    const uchar* row = img.constScanLine(y);
    for(int x = 0; x < c.w; x++)
    {
      const int r = row[x * 3 + 0], g = row[x * 3 + 1], b = row[x * 3 + 2];
      if(lo(r) && lo(g) && lo(b))
        c.background++;
      else if(hi(r) && hi(g) && hi(b))
        c.white++;
      else if(hi(r) && hi(g) && lo(b))
        c.yellow++;
      else if(hi(r) && lo(g) && hi(b))
      {
        c.lit++;
        c.litRowSum += y;
        c.litRSum += r;
      }
      else if(lo(r) && lo(g) && hi(b))
      {
        c.unlit++;
        c.unlitRowSum += y;
        c.unlitRSum += r;
      }
      else if(lo(r) && hi(g) && hi(b))
        c.backlit++;
      else
        c.other++;
    }
  }
  return c;
}

//! Pixels differing by more than 24 codes in some channel -- the same
//! "materially different" threshold ThreedimRenderTest.cpp:502 uses.
long long farPixels(const QImage& a, const QImage& b)
{
  if(a.isNull() || b.isNull() || a.size() != b.size())
    return -1;
  long long far = 0;
  for(int y = 0; y < a.height(); y++)
  {
    const uchar* ra = a.constScanLine(y);
    const uchar* rb = b.constScanLine(y);
    for(int x = 0; x < a.width(); x++)
    {
      int worst = 0;
      for(int ch = 0; ch < 3; ch++)
        worst = std::max(worst, std::abs(int(ra[x * 3 + ch]) - int(rb[x * 3 + ch])));
      if(worst > 24)
        far++;
    }
  }
  return far;
}

QImage loadRgb(const QString& path)
{
  QImage im{path};
  return im.isNull() ? im : im.convertToFormat(QImage::Format_RGB888);
}

//! Environment gate. Never a verdict -- rule 1: SKIP for the environment, never
//! fail for it. Follows FrameDeterminismTest.cpp / GfxNestedIntervalTest.cpp:384-400.
QString notReady()
{
  if(appBinary().isEmpty() || !QFile::exists(appBinary()))
    return QStringLiteral("needs the ossia-score binary");
  if(!QFile::exists(assetsDir() + QStringLiteral("/Box.glb")))
    return QStringLiteral("Box.glb not present in ") + assetsDir()
           + QStringLiteral(" - ") + QString::fromUtf8(kFetchHint);
  // The offscreen QPA has no GL: the readback comes back flat, proving nothing.
  // §3.0's house rule -- never let a pixel verdict fall back to Null.
  if(qEnvironmentVariable("QT_QPA_PLATFORM") == QStringLiteral("offscreen"))
    return QStringLiteral("QT_QPA_PLATFORM=offscreen makes a pixel verdict vacuous");
  if(qEnvironmentVariable("SCORE_TEST_API").compare(
         QStringLiteral("vulkan"), Qt::CaseInsensitive)
     == 0)
    return QStringLiteral(
        "Vulkan is out of scope for P1-1 (SPEC-SCENE-RENDER-TESTS.md:827-829); "
        "see the header of this file");
#if defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__)
  if(!qEnvironmentVariableIsSet("DISPLAY")
     && !qEnvironmentVariableIsSet("WAYLAND_DISPLAY"))
    return QStringLiteral("needs a real display");
#endif
  return {};
}
} // namespace

TEST_CASE(
    "the scene assembly chain renders a lit model",
    "[integration][threedim][gfx][scene][render][gui]")
{
  if(const auto why = notReady(); !why.isEmpty())
    SKIP(why.toStdString());

  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  if(qEnvironmentVariableIsSet("SCORE_TEST_KEEP_ARTIFACTS"))
  {
    dir.setAutoRemove(false);
    WARN("artifacts kept in " << dir.path().toStdString());
  }

  // The shader stem must hold no dot: RenderPipeline/Process.cpp:32 finds the
  // vertex stage via QFileInfo::baseName(), which cuts at the FIRST dot.
  const QString fs = writeFile(dir, "p1-1-lit.fs", QByteArray{kFragmentShader});
  writeFile(dir, "p1-1-lit.vs", QByteArray{kVertexShader});
  const QString glb = assetsDir() + QStringLiteral("/Box.glb");

  const QString litPng = dir.filePath("lit.png");
  const QString darkPng = dir.filePath("nolight.png");

  // The AssetLoader parses on a worker thread and lands its result through a
  // closure, so the frame must be grabbed on wall-clock time, not on the first
  // render: ThreedimRenderTest.cpp:8-9 measured that a synchronous render pass
  // finishes before the loader's closure does, and settles for 9 s. Same 9 s
  // here, then `finish()` a second later.
  const auto phases = std::vector<std::pair<int, QByteArray>>{
      {9000, "grab()"}, {10500, "finish()"}};

  auto run = [&](bool withLight, const QString& png, const char* name) {
    const QString js = writeFile(
        dir, QStringLiteral("%1.js").arg(QString::fromUtf8(name)),
        sceneScript(glb, fs, png, withLight).toUtf8());
    const Run r = runPhased(js, phases);
    INFO(name << " renderer: " << r.rendererLine.toStdString());
    INFO(name << " log:\n" << r.log.toStdString());
    // A missing binary / display was already handled by notReady(); anything
    // from here on is a verdict, not an environment excuse.
    REQUIRE(r.started);
    REQUIRE(r.sawReady);
    // Every LIT-ERROR path in the script exits non-zero with a named reason;
    // the log is in scope above, so a construction failure reads as itself.
    CHECK_FALSE(r.log.contains("LIT-ERROR"));
    REQUIRE(r.log.contains("MARK-GRAB"));
    // The offscreen forcing worked; nothing grabbed the desktop.
    REQUIRE_FALSE(r.log.contains("capturing the SCREEN"));
    // WindowDevice::grabTo refuses to write a file and says exactly why when the
    // sink has nothing in it (WindowDevice.cpp:130-139).
    if(!QFile::exists(png))
      FAIL(
          "no frame was grabbed for " << name
                                      << "; the chain rendered nothing into the "
                                         "window (see the log above)");
    return r;
  };

  const Run litRun = run(true, litPng, "lit");
  const Run darkRun = run(false, darkPng, "nolight");
  CHECK_FALSE(litRun.crashed);
  CHECK_FALSE(darkRun.crashed);

  const Classes lit = classify(litPng);
  const Classes dark = classify(darkPng);
  REQUIRE(lit.loaded);
  REQUIRE(dark.loaded);
  REQUIRE(lit.w == dark.w);
  REQUIRE(lit.h == dark.h);
  INFO(
      "lit: " << lit.w << "x" << lit.h << " bg=" << lit.background
              << " white=" << lit.white << " yellow=" << lit.yellow
              << " lit=" << lit.lit << " unlit=" << lit.unlit
              << " backlit=" << lit.backlit << " other=" << lit.other);
  INFO(
      "nolight: bg=" << dark.background << " white=" << dark.white
                     << " yellow=" << dark.yellow << " lit=" << dark.lit
                     << " unlit=" << dark.unlit << " backlit=" << dark.backlit
                     << " other=" << dark.other);

  // -- Diagnostic sentinels first, so a structural failure is named rather than
  // showing up as a mysterious inequality failure.
  // The lit run's scene HAS a light, so the "no light" sentinel must be absent.
  REQUIRE(lit.white == 0);
  // A degenerate light world matrix would make the direction meaningless. The
  // shader flags it rather than normalising a zero vector.
  if(lit.yellow > 0)
    FAIL(
        "the light's world transform is degenerate: "
        << lit.yellow
        << " pixels hit the shader's yellow diagnostic, i.e. "
           "mat3(world_transforms.data[transform_slot]) * local_direction is "
           "the zero vector. That is a world-transform defect "
           "(ScenePreprocessorNode.cpp:2698-2699 / Light.cpp:231-233), not a "
           "lighting one");

  // =========================================================================
  // (d) THE FRAME IS NOT UNIFORM.
  // Stated first because it is the cheapest, and because every assertion below
  // presupposes it. Derived: the box covers ~7% of a 1280x720 frame (see the
  // header's coverage arithmetic), so neither class can be the whole frame.
  // =========================================================================
  {
    INFO("coverage fraction = " << lit.coverFrac());
    CHECK(lit.covered() > 0);
    CHECK(lit.background > 0);
    // Loose band around the derived ~7.0%: generous on both sides because the
    // projected area of a tilted quad is not exactly area x cos(theta).
    CHECK(lit.coverFrac() > 0.01);
    CHECK(lit.coverFrac() < 0.40);
  }

  // =========================================================================
  // (c) THE BACKGROUND IS EXACTLY THE CLEAR COLOUR.
  // RenderedRawRasterPipelineNode.cpp:3179-3181 begins the pass with
  // Qt::transparent, i.e. (0, 0, 0, 0); read back as RGB that is (0, 0, 0).
  // Asserted as a PARTITION, which is the strong form: with Samples=1 and no
  // MSAA every pixel is either a fragment the shader wrote (B = 1.0) or
  // untouched clear, and nothing in between. `other` is the residue -- a
  // resampling blit between the render target and the readback would put edge
  // pixels there, so it is bounded rather than required to be zero.
  // =========================================================================
  {
    const double bg = double(lit.background) / lit.total;
    const double residue = double(lit.other) / lit.total;
    INFO("background fraction = " << bg << ", unclassified = " << residue);
    // Complement of the coverage band above.
    CHECK(bg > 0.60);
    CHECK(residue < 0.005);
    // Same statement for the light-off frame.
    CHECK(double(dark.other) / dark.total < 0.005);
  }

  // =========================================================================
  // (a) THE CLOSED-FORM INEQUALITY -- the primary gate.
  //
  // Light points along world -Z (Light.cpp:207-214, identity rotation), so
  // L = (0, 0, +1). The two faces the default camera can see are +Z and +Y
  // (camera on the plane x = 0 at (0,1,3); Camera.hpp:58-64):
  //
  //     +Z face   N = (0,0,1)   N.L = +1   ->  R = 255
  //     +Y face   N = (0,1,0)   N.L =  0   ->  R =   0
  //
  // MARGIN: 255 codes of 255. Asserted at >= 245 (96% of full scale), the slack
  // being 8-bit rounding only. Because Box.glb is flat-shaded (one normal per
  // face, 24 vertices for 6 faces -- GltfLoaderTest.cpp:373-377), N is CONSTANT
  // inside each face, so no interpolated intermediate shade can exist: the
  // covered pixels must partition into exactly these two classes.
  //
  // DEVIATION FROM THE SPEC'S WORDING, stated plainly. SPEC:815-816 asks for
  // "the face the light points at ... brighter than the opposite face". The
  // opposite face (-Z) is on the far side of the cube, so no single camera
  // placement can hold both it and the +Z face: an opaque convex solid never
  // shows two opposite faces at once. (The camera IS scriptable -- this test
  // places it -- so the limit here is the geometry, not the tooling.) A second
  // frame from behind would compare two RUNS rather than two faces, which is a
  // weaker statement than the one below. What is measured instead is the strongest
  // inequality available in one frame -- lit face vs the other visible face,
  // 255 vs 0 -- plus the `backlit` class, which is the shader's channel for a
  // face turned AWAY from the light and which must be empty here precisely
  // because the -Z face is hidden. Between them they pin both signs of N.L.
  // =========================================================================
  {
    const int covered = lit.covered();
    REQUIRE(covered > 0);
    const double litFrac = double(lit.lit) / covered;
    const double unlitFrac = double(lit.unlit) / covered;
    INFO(
        "of " << covered << " covered pixels: lit=" << litFrac
              << " unlit=" << unlitFrac << " backlit="
              << double(lit.backlit) / covered);

    // Both visible faces are present. MEASURED proportions of the covered set
    // are 0.8713 lit (+Z, 50,512 px) and 0.1287 unlit (+Y, 7,458 px) -- see the
    // header's coverage note for why the +Y closed form over-estimates.
    // Asserted at >= 5% each, which is far below either and far above nothing.
    REQUIRE(lit.lit > 0);
    REQUIRE(lit.unlit > 0);
    CHECK(litFrac > 0.05);
    CHECK(unlitFrac > 0.05);

    // THE MARGIN ITSELF, measured rather than restated: the mean R code over
    // every pixel of the lit face minus the mean R code over every pixel of the
    // other visible face. Closed form 255 - 0 = 255; asserted at >= 245, the
    // 10-code slack being 8-bit rounding only.
    const double meanRLit = lit.litRSum / lit.lit;
    const double meanRUnlit = lit.unlitRSum / lit.unlit;
    INFO(
        "mean R: lit face = " << meanRLit << ", other visible face = "
                              << meanRUnlit << ", margin = "
                              << (meanRLit - meanRUnlit) << " (closed form 255)");
    CHECK(meanRLit - meanRUnlit >= 245.0);

    // The -Z face is hidden by the depth buffer, so nothing visible faces away
    // from the light. This is the assertion the negative control inverts: with
    // Light.cpp:213 flipped, the +Z face moves out of `lit` and into `backlit`.
    CHECK(double(lit.backlit) / covered < 0.01);

    // FACE IDENTITY, spatial half. Predicted centroid rows at 720 px:
    // bright ~397, dark ~262 (NDC y -0.1019 and +0.2735; derivation in the
    // header). Only the ORDERING is asserted, and only as a CHECK: the row
    // direction of the RGBA8888 offscreen readback is UNVERIFIED by this
    // author. A failure here ALONE, with the class populations above green,
    // is a finding about the image Y convention, not about the light.
    if(lit.lit > 0 && lit.unlit > 0)
    {
      const double litRow = lit.litRowSum / lit.lit;
      const double unlitRow = lit.unlitRowSum / lit.unlit;
      INFO(
          "centroid rows: lit=" << litRow << " (predicted ~" << 0.5510 * lit.h
                                << "), unlit=" << unlitRow << " (predicted ~"
                                << 0.3633 * lit.h << ")");
      CHECK(litRow > unlitRow);
    }
  }

  // =========================================================================
  // (b) THE DIFFERENCE ORACLE -- light on vs light off, quantified.
  //
  // The light-off run is the SAME scene minus the Light process, so
  // scene_counts.light_count is 0 and every covered fragment takes the shader's
  // white branch. That makes the light-off frame the exact coverage mask, and
  // proves independently of the light that the asset, the camera and the
  // preprocessor all reach the rasteriser -- the control ShaderSweepScene.cpp
  // needed at :446-451 to keep a black frame from being read as "no light".
  //
  // Closed form for the pixel count that must differ:
  //   +Z face   (255,0,255) vs (255,255,255)  -> differs by 255 in G
  //   +Y face   (  0,0,255) vs (255,255,255)  -> differs by 255 in R and G
  //   background  (0,0,0)   vs (0,0,0)        -> identical
  // So EVERY covered pixel differs and NO background pixel does: the far-pixel
  // count equals the coverage exactly. Asserted at >= 98% of coverage, the 2%
  // being slack for the two runs' rasterisation agreeing only to within an edge
  // pixel (they are separate processes).
  // =========================================================================
  {
    // The light-off frame is entirely white-on-black: no lighting classes.
    CHECK(dark.white > 0);
    CHECK(dark.lit == 0);
    CHECK(dark.unlit == 0);
    CHECK(dark.backlit == 0);
    CHECK(dark.yellow == 0);

    // Same geometry, same camera, same everything but the light: the coverage
    // must agree. 2% tolerance for edge rasterisation between two processes.
    const double covLit = lit.coverFrac(), covDark = dark.coverFrac();
    INFO("coverage lit=" << covLit << " nolight=" << covDark);
    CHECK(std::abs(covLit - covDark) < 0.02 * std::max(covLit, covDark) + 1e-9);

    const QImage a = loadRgb(litPng), b = loadRgb(darkPng);
    const long long far = farPixels(a, b);
    INFO(
        "far pixels (>24 codes in some channel) = "
        << far << " of " << lit.total << "; coverage = " << lit.covered());
    REQUIRE(far >= 0);
    // Materially different, and different in exactly the right places.
    CHECK(far >= (long long)(0.98 * lit.covered()));
    // And no more than the coverage plus an edge allowance: the background must
    // not have moved, which is (c) restated across the two runs.
    CHECK(far <= (long long)(lit.covered() + 0.005 * lit.total));
  }
}
