// P1-2 (SPEC-SCENE-RENDER-TESTS.md:830) -- a light added or removed mid-render
// changes the frame, and ONLY that.
//
// =============================================================================
// WHAT THIS ADDS OVER THE EXISTING STATIC FORM
// =============================================================================
// tests/integration/ShaderSweepScene.cpp:415-459 already has the STATIC form:
// it builds `Primitive cube [+ Camera + DirectionalLight] -> ScenePreprocessor
// -> raster tester -> sink` TWICE with `SceneOpts{.light = true}` and
// `{.light = false}` (:421, :426), renders both from scratch, and fails when
// the two readbacks are byte-identical ("the DirectionalLight never reached
// scene_lights", :453-455). Two independent graphs, two independent
// `GfxPipeline::create()` calls, no transport: it can only say that a graph
// BUILT with a light differs from a graph BUILT without one.
//
// This file does it LIVE, which is a different question and the one the 16
// corpus documents that wire a Light into a Scene Preprocessor actually pose:
// ONE graph, ONE running transport, the Light process and its cable created
// and then destroyed underneath it. It therefore also asserts the thing the
// static form structurally cannot: that the change is an SSBO/UBO update on an
// otherwise untouched graph, not a rebuild, and that removing the light puts
// the frame back EXACTLY where it was.
//
// =============================================================================
// THE CHAIN, AND WHY IT IS ON THE ROOT INTERVAL
// =============================================================================
//   Primitive Cube -> Scene Preprocessor -> Render Pipeline -> Window:/
// all on `Score.rootInterval()`, which always executes.
//
// It is NOT in a `Score.createBox` box, deliberately. `Macro::createBox`
// (Scenario/Commands/CommandAPI.cpp:53) mints a brand-new start TimeSync; only
// the scenario's own initial sync is ever marked a start point
// (Scenario/Process/ScenarioModel.cpp:64 `start_tn.setStartPoint(true)`), and
// `ossia::scenario::get_roots()`
// (3rdparty/libossia/src/ossia/editor/scenario/detail/scenario_execution.cpp:25-39)
// roots ONLY syncs with `is_start()`. A floating box never executes at all --
// measured and written up in tests/integration/GfxNestedIntervalTest.cpp:12-34.
// The document's own `Scenario.1` is removed the way
// tests/integration/live-edit/common.js:30-35 (`initBase`) and
// ThreedimRenderTest.cpp:436 do, so the only executing objects are the three
// root-interval processes and the one added mid-play.
//
// Process UUIDs, all read out of the tree:
//   Cube             cf8a328a-1ba6-47f8-929f-2168bdec90b0  Primitive.hpp:84
//   Light            9f3c1a5e-4b7d-4e2a-8c5f-1d6e0b9a3c7f  Light.hpp:53
//   ScenePreprocessor a8f2c6d0-1b4e-4c7a-9d3f-5e8b2c1a7f0d ScenePreprocessor/Metadata.hpp
//   RenderPipeline   dbfc2101-40d7-4807-8804-571e88992e7e  RenderPipeline/Metadata.hpp
//   Window device    5a181207-7d40-4ad8-814e-879fcdf8cc31  (JsGraphE2ETest recipe)
// The same five constants appear in tests/integration/live-edit/scene-storm.js:
// 57-63, which builds exactly this Group -> Scene Preprocessor -> Render
// Pipeline -> Window shape from JS while the transport runs, and is where the
// port-wiring idiom below is copied from (scene-storm.js:128-146).
//
// Port wiring (names, not indices, because control inlets interleave):
//   Cube outlet 0             "Geometry"      TinyObj.hpp:117-127
//   ScenePreprocessor inlet   "Scene In"      ScenePreprocessor/Process.cpp:31
//   ScenePreprocessor outlet  "Geometry Out"  ScenePreprocessor/Process.cpp:38
//   RenderPipeline inlet      "Geometry In"   RenderPipeline/Process.cpp:170
//   RenderPipeline outlet     "Texture Out"   RenderPipeline/Process.cpp:171
//   Light outlet 0            "Scene"         Light.hpp:143-149
// RenderPipeline::setProgram pushes "Geometry In"/"Texture Out" itself before
// the ISF-derived ports (Process.cpp:170-174), so those two names exist for
// every program.
//
// =============================================================================
// THE PROBE SHADER: A CLOSED-FORM ORACLE, NO GOLDEN
// =============================================================================
// No committed shader in this tree reads the scene light data (grep
// "scene_lights" over the repo hits only .cpp/.hpp), so the probe pair is
// authored here and written into the QTemporaryDir -- exactly the way
// tests/gfx/GfxEnvRenderTargetSize.cpp:189-231 authors kProbeVS/kProbeFS for
// the same reason. That probe is the shape this one is copied from: MODE
// RAW_RASTER_PIPELINE, `{"TYPE": "vec4", "NAME": "position"}`, empty
// VERTEX_OUTPUTS/FRAGMENT_INPUTS, driven by a Threedim::Cube through a real
// ScenePreprocessor -- i.e. a shader shape this repo has already rendered
// green on both backends (P1-19, LEDGER-SCENE-RENDER-TESTS.md).
//
// The probe declares ONE input, the preprocessor's own `scene_counts`:
//     { "NAME": "scene_counts", "TYPE": "storage", "ACCESS": "read_only",
//       "LAYOUT": [ light_count, material_count, draw_count, pad0 : uint ] }
// * The name is what ScenePreprocessorNode publishes on the emitted geometry:
//   `.name = "scene_counts"` at ScenePreprocessorNode.cpp:2797-2799.
// * The layout is SceneCountsUBO field-for-field
//   (ScenePreprocessorNode.cpp:328-334, static_assert sizeof == 16 at :335).
// * "storage" is mandated by the product's own comment at
//   ScenePreprocessorNode.cpp:4152-4165: QRhi forbids Dynamic+StorageBuffer and
//   Static+UniformBuffer fails create() on D3D11/GLES, so the buffer is
//   allocated SSBO-only and "all bundled shaders therefore declare scene_counts
//   as a storage buffer -- rasterizers with TYPE: "storage", ACCESS:
//   "read_only"".
// * Name-resolution against the upstream geometry's auxiliary_buffers is the
//   documented mechanism: RenderedRawRasterPipelineNode.cpp:1933-1937 --
//   "INPUTS storage_input / uniform_input name-match against the geometry's
//   auxiliary_buffers the same way, which is how ScenePreprocessor publishes
//   scene_lights, per_draws, scene_counts, camera and env into flattened-scene
//   shaders."
// * light_count is authoritative and is the arena-addressable light subset:
//   `SceneCountsUBO sc{(uint32_t)m_cachedLightIndices.size(), ...}` at
//   ScenePreprocessorNode.cpp:4592-4596, fed from freshLightIndices
//   (:4136-4140, :4523, :4572).
//
// Fragment output, per pixel:
//     R = fract(gl_FragCoord.x / 256.0)      -- coverage/identity plane
//     G = draw_count  > 0 ? 1.0 : 0.0        -- "scene_counts is really bound"
//     B = light_count > 0 ? 1.0 : 0.0        -- the quantity under test
//     A = 1.0
// R is a pure function of the pixel, so "the R plane is unchanged" is exactly
// "the covered pixel set is unchanged" -- an uncovered pixel would read the
// clear colour instead. G is the placeholder discriminator: an UNRESOLVED
// auxiliary gets an auto-allocated zero-filled placeholder sized from the
// shader's own LAYOUT (RenderedRawRasterPipelineNode.cpp:1955-1983,
// `aux_declared_size`), and a zero placeholder has draw_count == 0 as well as
// light_count == 0. Without G, "the light never reached the shader" and "the
// probe never bound scene_counts at all" would be the same red; with it they
// are two different reds.
//
// The vertex stage deliberately IGNORES the camera:
//     gl_Position = clipSpaceCorrMatrix * vec4(position.xy * 4.0 - 2.0, 0.5, 1.0);
// `vcg::tri::Box` builds the cube over [0,1]^3 (Primitive.cpp:172-180), so
// position.xy*4-2 spans [-2,2] in NDC: the +Z and -Z faces each cover the whole
// clip square and carry opposite windings, so the viewport is fully covered
// under any cull mode, and z = 0.5 passes both a forward depth test (clear 1.0,
// LESS) and a reverse-Z one (clear 0.0, GREATER). Ignoring
// camera.viewProjection is not laziness: the preprocessor's no-camera default
// framing would otherwise decide coverage, and JS cannot pin a Camera instead
// -- `Qt.vector3d(x,y,z)` in the console engine drops its arguments and returns
// a zeroed vector, measured and documented at ThreedimRenderTest.cpp:644-654.
//
// NO GOLDEN IS USED OR BLESSED. Every verdict below is either a closed-form
// constant (0 or 255 in a named channel), a plane-equality between two frames
// this same run produced, or a trace-line count.
//
// =============================================================================
// THE FIVE PHASES AND WHAT EACH ASSERTS
// =============================================================================
// The setup script defines phase functions and leaves the event loop FREE; the
// C++ parent injects them over OSC (`/script s "..."` on udp/6666, the
// LocalTree script node, JS/ApplicationPlugin.cpp:192-201) under the very same
// /tmp/score-harness.lock every other harness takes, since port 6666 is
// machine-global. This is GfxNestedIntervalTest.cpp:90-108's measured recipe:
// a JS busy-wait blocks the main-thread queues that a mid-play graph edit needs,
// so the edits must arrive as separate event-loop turns.
//
//   0.8 s  phaseA()      grabFrame(3, A.png); "MARK-A"      -- no light
//   2.3 s  addLight()    macro { Light + cable -> Scene In }; "MARK-ADD"
//   3.8 s  phaseB()      grabFrame(3, B.png); "MARK-B"      -- light live
//   5.3 s  dropLight()   Score.remove(light);  "MARK-DROP"
//   6.8 s  phaseC()      grabFrame(3, C.png); "MARK-C"      -- light gone
//   7.8 s  finish()      Score.stop(); "LIGHTLIVE-OK"; Qt.exit(0)
//
// grabFrame(n, path) is renderFrames(n) then grabTo(path)
// (WindowDevice.cpp:234-237), i.e. the render side is stepped synchronously by
// the grab itself (GfxContext.cpp:956-967; FrameDeterminismTest.cpp proves the
// step clock), so no grab races vsync. The probe reads no TIME and no
// frame counter, which is what makes C == A byte-exact a legitimate demand
// rather than a flake.
//
// Pixel assertions:
//   [render]     the R plane carries >= 16 distinct values across the middle
//                row of every frame -- a flat clear (nothing drew) is not
//                mistaken for a valid frame.
//   [bound]      G == 255 on every pixel of A, B and C: draw_count > 0, so the
//                probe is reading the preprocessor's real scene_counts and not
//                a zero placeholder.
//   [A]          B channel == 0 on every pixel: light_count == 0.
//   [B]          B channel == 255 on every pixel: light_count > 0.
//   [A != B]     QUANTIFIED: exactly width*height pixels differ, every one of
//                them by >= 200 in blue, and ZERO pixels differ in R, G or
//                alpha. "The light changed the frame, and only the lighting
//                term of it."
//   [C == A]     EXACT: max per-channel absolute difference over all four
//                channels is 0, i.e. 0 differing pixels. Not a tolerance.
//
// Trace assertions (SCORE_GFX_TRACE=1). GfxContext::updateGraph prints
//     GFX-EDGES consume old=<n> new=<n> full=<0|1>
// at GfxContext.cpp:917-919, inside the `if(edges_changed.exchange(false))`
// block at :906; full=1 routes through recompute_connections() (:921-925) and
// full=0 through incrementalEdgeUpdate (:926-930). The child's merged log is
// split at the MARK lines the script prints immediately after each grab; both
// streams (console.log through Qt's handler, and the raw fprintf) reach the
// same pipe in wall-clock order under QT_FORCE_STDERR_LOGGING.
//   [incr-add]   every consume between MARK-A and MARK-B says full=0
//   [incr-drop]  every consume between MARK-B and MARK-C says full=0
//   [grow]       the edge set grows across the add   (newN > oldN)
//   [shrink]     the edge set shrinks across the drop (newN < oldN)
//   [restored]   the edge count after the drop equals the count before the add
//                -- the "and only that" claim on the graph side: the light's
//                edge is the only edge that ever moved.
//
// =============================================================================
// NEGATIVE CONTROL
// =============================================================================
// PROPOSED CONTROL (one line, product side):
//     src/plugins/score-plugin-gfx/Gfx/Graph/ScenePreprocessorNode.cpp
//     insert after line 4140 (i.e. immediately after the
//     `for(uint32_t s : fs.lightArenaSlots) if(s != 0xFFFFFFFFu) ...` loop
//     that fills freshLightIndices):
//
//         freshLightIndices.clear();
//
// freshLightIndices is the only source of the light index list and of the
// count: it sizes the buffer (:4147), is diff-uploaded on the mesh-unchanged
// fast path (:4522-4523), uploaded on the structural path (:4547-4551), moved
// into m_cachedLightIndices (:4572), and `light_count` is literally
// `(uint32_t)m_cachedLightIndices.size()` (:4592-4593). Clearing it pins
// light_count at 0 on both paths.
//
// EXPECTED SPLIT -- this is what makes it a control and not a smoke test:
//   RED   : [B] (blue must be 255 with the light live) and [A != B] (the
//           quantified difference collapses from width*height differing
//           pixels to 0).
//   GREEN : [render], [bound] (draw_count is untouched), [A], [C == A] (both
//           frames are still byte-identical -- they are simply now identical to
//           B as well), and all five trace assertions [incr-add], [incr-drop],
//           [grow], [shrink], [restored] -- the cable is still created and
//           destroyed, so the edge set still moves and still moves
//           incrementally.
//
// THE SPEC'S PROPOSED CONTROL IS WRONG AND POINTS THE OTHER WAY.
// SPEC-SCENE-RENDER-TESTS.md:838-839 proposes: "Make `scene_light_indices`
// include the sentinel `0xFFFFFFFF` slot -> B stops differing from A."
// Read against the code, it cannot do that:
//   1. The sentinel is pushed ONLY for producer-less lights.
//      SceneGPUState.cpp:532-541: the arena slot is
//      `(*light)->raw_slot.size != 0 ? (*light)->raw_slot.internal_index
//                                    : 0xFFFFFFFFu`,
//      with the comment "0xFFFFFFFF sentinel for producer-less lights (e.g.
//      FBX/glTF-embedded lights that don't own a RawLight slot yet)".
//   2. A Threedim::Light ALWAYS owns a slot. Light::init
//      (Light.cpp:167-193) allocates from Arena::RawLight unconditionally on
//      first init and Light.cpp:79 stamps the resulting ref onto the emitted
//      light_component's raw_slot. So this test's light is never the sentinel,
//      and in this scene `fs.lightArenaSlots` contains no sentinel at all --
//      dropping the filter at ScenePreprocessorNode.cpp:4138-4140 is a literal
//      NO-OP here. A control that cannot change the measured value is not a
//      control.
//   3. Even in a scene that did have a producer-less light, admitting the
//      sentinel ADDS an entry and INCREMENTS light_count (:4592) -- it makes B
//      differ from A more, not less, and additionally makes shaders index
//      `scene_lights.entries[0xFFFFFFFF]` out of the arena
//      (ScenePreprocessorNode.cpp:2752-2755). It is a corruption knob, not an
//      erasure knob.
// The control proposed above erases the light from the GPU-visible state, which
// is the direction the assertion actually needs.
//
// =============================================================================
// HARDWARE / SKIP POLICY
// =============================================================================
// The verdict is a pixel, so the house rule from live-edit-sweep.sh applies:
// never fall back to QT_QPA_PLATFORM=offscreen -- the Null backend makes the
// verdict vacuous. This test SKIPs instead when the score binary is missing,
// when there is no display, or when QT_QPA_PLATFORM is offscreen; and it SKIPs
// when the app reports that a required process UUID did not resolve (a build
// without score_plugin_threedim, which the CMake guard below also covers).
// Any RHI class works: nothing here is golden-compared, so no
// backend-identity pin is needed. The backend is whatever SCORE_TEST_API says,
// defaulting to the platform's own (opengl on Linux), exactly as
// GfxNestedIntervalTest.cpp:236-243 does. Vulkan is in scope for THIS chain:
// the qrhivulkan.cpp UsedWithGenerateMips abort noted at
// ThreedimRenderTest.cpp:44-50 was in ModelDisplayNode, which this chain does
// not use, and is in any case documented there as fixed. UNVERIFIED on this
// host: no build was run for this file, so the Vulkan leg is reasoned, not
// measured.
//
// =============================================================================
// INTENDED REGISTRATION -- add to tests/integration/CMakeLists.txt
// =============================================================================
//
//   # P1-2: a Light process added and then removed mid-render changes the
//   # frame and ONLY the frame: closed-form probe (blue = light_count > 0,
//   # red = coverage plane, green = "scene_counts really bound"), frame C
//   # byte-identical to frame A, plus the SCORE_GFX_TRACE GFX-EDGES half
//   # proving both transitions were consumed incrementally (full=0, the edge
//   # set grows then shrinks back to its original size). Drives the
//   # application binary; needs a real display and the threedim scene
//   # processes, and SKIPs itself otherwise.
//   if(TARGET score AND TARGET score_plugin_gfx AND TARGET score_plugin_threedim
//      AND NOT EMSCRIPTEN)
//     score_add_test(test_integration_gfx_light_live
//       SOURCES GfxLightLiveTest.cpp
//       LIBS ${QT_PREFIX}::Gui)
//     target_compile_definitions(test_integration_gfx_light_live PRIVATE
//       "SCORE_APP_BINARY=\"$<TARGET_FILE:score>\"")
//     set_tests_properties(test_integration_gfx_light_live PROPERTIES
//       TIMEOUT 600 RUN_SERIAL TRUE LABELS "gui")
//   endif()
//
// No GFX_TEST_CORPUS_DIR: the probe shaders are authored in-test, so this file
// has no corpus dependency at all.
//
// Recipe notes inherited from JsGraphE2ETest.cpp / live-edit/common.js:11-14:
// device addresses must be "Window:/"; `var` only in the script (the console
// QJSEngine scopes const/let inside eval, so an outer script cannot see them).
// =============================================================================

#include <QElapsedTimer>
#include <QFile>
#include <QImage>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QThread>
#include <QUdpSocket>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdlib>
#include <regex>
#include <set>
#include <string>
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

// Process / device UUIDs -- see the header block for the file:line each was
// read from.
const char* kUuidWindow = "5a181207-7d40-4ad8-814e-879fcdf8cc31";
const char* kUuidCube = "cf8a328a-1ba6-47f8-929f-2168bdec90b0";
const char* kUuidLight = "9f3c1a5e-4b7d-4e2a-8c5f-1d6e0b9a3c7f";
const char* kUuidPreproc = "a8f2c6d0-1b4e-4c7a-9d3f-5e8b2c1a7f0d";
const char* kUuidRenderPipeline = "dbfc2101-40d7-4807-8804-571e88992e7e";

// ---------------------------------------------------------------- the probe

//! Camera-free orthographic projection of the cube's object-space XY. See the
//! header block: [0,1]^3 * 4 - 2 covers the clip square twice over, with both
//! windings present, so coverage does not depend on the preprocessor's default
//! camera nor on the cull mode.
constexpr const char* kProbeVS = R"__(void main()
{
    gl_Position = clipSpaceCorrMatrix * vec4(position.xy * 4.0 - 2.0, 0.5, 1.0);
}
)__";

//! LAYOUT is SceneCountsUBO field-for-field (ScenePreprocessorNode.cpp:328-334)
//! and the name is what the preprocessor publishes (:2798-2800). "storage" is
//! required, not stylistic (:4157-4168).
constexpr const char* kProbeFS = R"__(/*{
  "DESCRIPTION": "P1-2 probe: encodes the ScenePreprocessor's scene_counts into flat colour. R = fract(gl_FragCoord.x/256) coverage/identity plane, G = draw_count > 0 (proves scene_counts resolved to real data rather than a zero placeholder), B = light_count > 0 (the quantity under test). Reads no TIME, so two renders of the same scene state are byte-identical.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST-SYNTHETIC", "TEST-SCENE"],
  "VERTEX_INPUTS": [
    { "TYPE": "vec4", "NAME": "position" }
  ],
  "VERTEX_OUTPUTS": [],
  "FRAGMENT_INPUTS": [],
  "FRAGMENT_OUTPUTS": [
    { "TYPE": "vec4", "NAME": "isf_FragColor" }
  ],
  "INPUTS": [
    { "NAME": "scene_counts", "TYPE": "storage", "ACCESS": "read_only",
      "LAYOUT": [
        { "NAME": "light_count",    "TYPE": "uint" },
        { "NAME": "material_count", "TYPE": "uint" },
        { "NAME": "draw_count",     "TYPE": "uint" },
        { "NAME": "pad0",           "TYPE": "uint" }
      ]
    }
  ]
}*/

void main()
{
    isf_FragColor = vec4(
        fract(gl_FragCoord.x / 256.0),
        scene_counts.draw_count  > 0u ? 1.0 : 0.0,
        scene_counts.light_count > 0u ? 1.0 : 0.0,
        1.0);
}
)__";

// ---------------------------------------------------------------- the driver

struct Run
{
  int exitCode{-1};
  bool crashed{true};
  bool sawReady{false};
  QString log;
};

//! One OSC message `/script s <code>` to the app's LocalTree device on
//! udp/6666 -- byte-identical to what `oscsend 127.0.0.1 6666 /script s ...`
//! sends in live-edit-sweep.sh.
void sendScript(QUdpSocket& sock, const QByteArray& code)
{
  auto pad4 = [](QByteArray b) {
    b.append('\0');
    while(b.size() % 4)
      b.append('\0');
    return b;
  };
  QByteArray dgram = pad4("/script") + pad4(",s") + pad4(code);
  sock.writeDatagram(dgram, QHostAddress::LocalHost, 6666);
}

//! Runs the app on the given setup script, then injects the phase calls over
//! OSC at the given offsets (ms, measured from the LIGHTLIVE-READY line the
//! script prints right after Score.play()). The last injected call must make
//! the app exit.
Run runPhased(
    const QString& js, const QString& cacheDir,
    const std::vector<std::pair<int, QByteArray>>& phases)
{
  auto env = QProcessEnvironment::systemEnvironment();
  // Offscreen render target: with a mapped window the grab reads the SCREEN at
  // its geometry, i.e. the desktop, which is never blank.
  env.insert("SCORE_FORCE_OFFSCREEN_WINDOW", "Window");
  env.insert("SCORE_AUDIO_BACKEND", "dummy");
  env.insert("SCORE_DISABLE_AUDIOPLUGINS", "1");
  // The whole point: GfxContext prints its edge-consume decisions.
  env.insert("SCORE_GFX_TRACE", "1");
  // The platform's own backend, not OpenGL everywhere -- see JsGraphE2ETest.
#if defined(_WIN32)
  constexpr auto defaultApi = "d3d11";
#elif defined(__APPLE__)
  constexpr auto defaultApi = "metal";
#else
  constexpr auto defaultApi = "opengl";
#endif
  env.insert("QSG_RHI_BACKEND", qEnvironmentVariable("SCORE_TEST_API", defaultApi));
  env.remove("QT_QPA_PLATFORM");
  // Per-run shader/PSO cache. ThreedimRenderTest.cpp:360-365 measured a
  // negative control staying green because the app replayed the previously
  // compiled pipeline out of XDG_CACHE_HOME; isolate it so a product edit is
  // actually recompiled.
  env.insert("XDG_CACHE_HOME", cacheDir);
  // Both the MARK lines (console.log through Qt's message handler) and the
  // GFX-EDGES lines (raw fprintf(stderr, ...)) must reach the same pipe, in
  // wall-clock order, on every platform.
  env.insert("QT_FORCE_STDERR_LOGGING", "1");
  env.insert("QT_ASSUME_STDERR_HAS_CONSOLE", "1");

  Run r;

#if defined(Q_OS_UNIX)
  // OSC port 6666 is machine-global: serialize against live-edit-sweep.sh and
  // scene-js-sweep.sh by taking the very same advisory lock they hold.
  const int lockFd = ::open("/tmp/score-harness.lock", O_CREAT | O_RDWR, 0666);
  if(lockFd >= 0 && ::flock(lockFd, LOCK_EX) != 0)
  {
    // Lock failure is not fatal; the run just risks stray 6666 traffic.
  }
#endif

  QProcess p;
  p.setProcessEnvironment(env);
  p.setProcessChannelMode(QProcess::MergedChannels);
  p.start(appBinary(), {"--no-gui", "--no-restore", "--script", js});

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
    // Wait for the setup script to report play started, up to 60 s.
    QElapsedTimer boot;
    boot.start();
    while(boot.elapsed() < 60000 && p.state() == QProcess::Running
          && !r.log.contains("LIGHTLIVE-READY") && !r.log.contains("LIGHTLIVE-NOPROC"))
      pump(100);
    r.sawReady = r.log.contains("LIGHTLIVE-READY");

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

    if(!p.waitForFinished(60000))
    {
      p.kill();
      p.waitForFinished(5000);
    }
  }
  r.log += QString::fromUtf8(p.readAll());
  r.crashed = p.exitStatus() != QProcess::NormalExit || p.state() != QProcess::NotRunning;
  r.exitCode = p.exitCode();

#if defined(Q_OS_UNIX)
  if(lockFd >= 0)
    ::close(lockFd); // releases the flock
#endif
  return r;
}

bool writeText(const QString& path, const QByteArray& text)
{
  QFile f{path};
  if(!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    return false;
  return f.write(text) == text.size();
}

// ---------------------------------------------------------------- the pixels

//! A grabbed frame, normalised to non-premultiplied RGBA8888 -- the exact
//! format the offscreen readback is wrapped in before being saved
//! (WindowDevice.cpp:147-152).
struct Frame
{
  bool loaded{false};
  QImage img;
  int w{0}, h{0};
};

Frame loadFrame(const QString& path)
{
  Frame f;
  QImage img{path};
  if(img.isNull())
    return f;
  f.img = img.convertToFormat(QImage::Format_RGBA8888);
  f.loaded = !f.img.isNull();
  f.w = f.img.width();
  f.h = f.img.height();
  return f;
}

struct Channels
{
  int r{}, g{}, b{}, a{};
};

Channels at(const Frame& f, int x, int y)
{
  const uchar* p = f.img.constScanLine(y) + 4 * x;
  return {p[0], p[1], p[2], p[3]};
}

//! Distinct R values across the middle scanline. A frame that is a flat clear
//! (nothing ever drew) has exactly one; the probe's fract(x/256) ramp has many.
int distinctRedOnMiddleRow(const Frame& f)
{
  if(!f.loaded || f.h <= 0)
    return 0;
  std::set<int> seen;
  const int y = f.h / 2;
  for(int x = 0; x < f.w; x++)
    seen.insert(at(f, x, y).r);
  return (int)seen.size();
}

struct ChannelStats
{
  int minV{255}, maxV{0};
  int offCount{0}; //!< pixels whose value is not `expect`
};

//! min/max of one channel, plus how many pixels miss `expect` exactly.
ChannelStats channelStats(const Frame& f, int channel, int expect)
{
  ChannelStats s;
  for(int y = 0; y < f.h; y++)
  {
    const uchar* row = f.img.constScanLine(y);
    for(int x = 0; x < f.w; x++)
    {
      const int v = row[4 * x + channel];
      s.minV = std::min(s.minV, v);
      s.maxV = std::max(s.maxV, v);
      if(v != expect)
        s.offCount++;
    }
  }
  return s;
}

struct Diff
{
  int pixels{0};        //!< pixels differing in ANY channel
  int blueOverThr{0};   //!< pixels whose blue differs by >= 200
  int rgaChanged{0};    //!< pixels differing in R, G or alpha
  int worstRgaDelta{0}; //!< worst per-channel |delta| outside blue
  int worstDelta{0};    //!< worst per-channel |delta| over all four channels
};

Diff diff(const Frame& a, const Frame& b)
{
  Diff d;
  for(int y = 0; y < a.h; y++)
  {
    const uchar* ra = a.img.constScanLine(y);
    const uchar* rb = b.img.constScanLine(y);
    for(int x = 0; x < a.w; x++)
    {
      const int dr = std::abs(ra[4 * x + 0] - rb[4 * x + 0]);
      const int dg = std::abs(ra[4 * x + 1] - rb[4 * x + 1]);
      const int db = std::abs(ra[4 * x + 2] - rb[4 * x + 2]);
      const int da = std::abs(ra[4 * x + 3] - rb[4 * x + 3]);
      const int rga = std::max({dr, dg, da});
      d.worstRgaDelta = std::max(d.worstRgaDelta, rga);
      d.worstDelta = std::max(d.worstDelta, std::max(rga, db));
      if(rga > 0)
        d.rgaChanged++;
      if(db >= 200)
        d.blueOverThr++;
      if(rga > 0 || db > 0)
        d.pixels++;
    }
  }
  return d;
}

// ---------------------------------------------------------------- the trace

struct Consume
{
  long oldN{};
  long newN{};
  int full{};
};

//! Every "GFX-EDGES consume old=<n> new=<n> full=<d>" line in a log segment,
//! in order -- the exact line GfxContext.cpp:917-919 emits, parsed the way
//! tests/gfx/GfxEdgeConsumeLatch.cpp parses it.
std::vector<Consume> consumes(const QString& segment)
{
  static const std::regex re{"GFX-EDGES consume old=(\\d+) new=(\\d+) full=(\\d)"};
  const std::string s = segment.toStdString();
  std::vector<Consume> out;
  for(auto it = std::sregex_iterator(s.begin(), s.end(), re);
      it != std::sregex_iterator(); ++it)
    out.push_back(
        {std::stol((*it)[1].str()), std::stol((*it)[2].str()),
         (*it)[3].str() == "1" ? 1 : 0});
  return out;
}

//! Every "GFX-EDGES publish <n>" size in the whole log, in order, with
//! consecutive duplicates collapsed. This is the race-free half of the trace
//! oracle: the publish comes from GfxExecutionAction::endTick on the EXECUTION
//! thread (GfxExecutionAction.cpp:97-98) and the MARK lines from the main
//! thread's console.log, so which of the two reaches the shared stderr pipe
//! first is a coin flip -- MEASURED: the drop's "publish 3" landed one line
//! BEFORE "MARK-DROP", which made a segment-local shrink test read old=3
//! new=3 and go red while the edge set had in fact gone 3 -> 4 -> 3. The
//! published SEQUENCE carries the same information and does not depend on log
//! interleaving at all.
std::vector<long> publishedSizes(const QString& log)
{
  static const std::regex re{"GFX-EDGES publish (\\d+)"};
  const std::string s = log.toStdString();
  std::vector<long> out;
  for(auto it = std::sregex_iterator(s.begin(), s.end(), re);
      it != std::sregex_iterator(); ++it)
  {
    const long n = std::stol((*it)[1].str());
    if(out.empty() || out.back() != n)
      out.push_back(n);
  }
  return out;
}

bool ready()
{
  if(appBinary().isEmpty() || !QFile::exists(appBinary()))
    return false;
  // The offscreen QPA has no GL: the readback comes back flat, proving
  // nothing. Same convention as FrameDeterminismTest.cpp and the
  // live-edit-sweep.sh house rule for pixel verdicts.
  if(qEnvironmentVariable("QT_QPA_PLATFORM") == "offscreen")
    return false;
#if defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__)
  return qEnvironmentVariableIsSet("DISPLAY")
         || qEnvironmentVariableIsSet("WAYLAND_DISPLAY");
#else
  return true;
#endif
}
}

TEST_CASE(
    "a light added or removed mid-render changes the frame, and only that",
    "[integration][gfx][js][threedim][scene]")
{
  if(!ready())
    SKIP("needs the score binary and a display");

  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  if(qEnvironmentVariableIsSet("SCORE_TEST_KEEP_ARTIFACTS"))
  {
    dir.setAutoRemove(false);
    WARN("artifacts kept in " << dir.path().toStdString());
  }

  // RenderPipeline::Model's ctor takes the FRAGMENT path and opens the sibling
  // <baseName>.vs next to it (RenderPipeline/Process.cpp:28-42), so the pair
  // must share a directory and a base name with no extra dots.
  const QString fsPath = dir.filePath("lightprobe.fs");
  const QString vsPath = dir.filePath("lightprobe.vs");
  REQUIRE(writeText(fsPath, QByteArray{kProbeFS}));
  REQUIRE(writeText(vsPath, QByteArray{kProbeVS}));

  const QString pngA = dir.filePath("a-nolight.png");
  const QString pngB = dir.filePath("b-light.png");
  const QString pngC = dir.filePath("c-nolight-again.png");

  QString src;
  src += "var FL = 705600000;\n"; // flicks per second (TimeVal impl units)
  src += QStringLiteral("var UUID_WINDOW = \"%1\";\n").arg(kUuidWindow);
  src += QStringLiteral("var UUID_CUBE = \"%1\";\n").arg(kUuidCube);
  src += QStringLiteral("var UUID_LIGHT = \"%1\";\n").arg(kUuidLight);
  src += QStringLiteral("var UUID_PREPROC = \"%1\";\n").arg(kUuidPreproc);
  src += QStringLiteral("var UUID_RP = \"%1\";\n").arg(kUuidRenderPipeline);
  src += "Score.createDevice(\"Window\", UUID_WINDOW, {});\n";
  // Everything on the root interval, which always executes; the document's own
  // Scenario.1 is removed so nothing else contributes edges.
  src += "var scen = Score.find(\"Scenario.1\"); if (scen) Score.remove(scen);\n";
  src += "var root = Score.rootInterval();\n";
  src += "Score.setIntervalDuration(root, 60 * FL);\n";
  src += "var cube = Score.createProcess(root, UUID_CUBE, \"\");\n";
  src += "var pre = Score.createProcess(root, UUID_PREPROC, \"\");\n";
  src += QStringLiteral(
             "var rp = Score.createProcess(root, UUID_RP, \"%1\");\n")
             .arg(fsPath);
  // A missing UUID means a build without the threedim scene processes: say so
  // on a distinct marker so the parent SKIPs instead of failing.
  src += "if (!cube || !pre || !rp) { console.log(\"LIGHTLIVE-NOPROC\"); "
         "Qt.exit(21); }\n";
  src += "var cIn = Score.inlet(pre, \"Scene In\");\n";
  src += "var gOut = Score.outlet(pre, \"Geometry Out\");\n";
  src += "var gIn = Score.inlet(rp, \"Geometry In\");\n";
  src += "var tOut = Score.outlet(rp, \"Texture Out\");\n";
  src += "if (!cIn || !gOut || !gIn || !tOut) { console.log(\"LIGHTLIVE-NOPORT\"); "
         "Qt.exit(22); }\n";
  src += "if (!Score.createCable(Score.outlet(cube, 0), cIn)) { "
         "console.log(\"LIGHTLIVE-NOCABLE cube\"); Qt.exit(23); }\n";
  src += "if (!Score.createCable(gOut, gIn)) { "
         "console.log(\"LIGHTLIVE-NOCABLE geometry\"); Qt.exit(24); }\n";
  src += "Score.setAddress(tOut, \"Window:/\");\n";
  src += "var dev = Score.device(\"Window\");\n";
  src += "if (!dev) { console.log(\"LIGHTLIVE-NODEV\"); Qt.exit(25); }\n";
  src += "var g_light = null;\n";
  // The phase functions the parent injects over OSC. Defined here so the event
  // loop stays free between phases -- a mid-play graph edit is served through
  // main-thread queues (GfxNestedIntervalTest.cpp:90-108).
  src += "function phaseA() { dev.grabFrame(3, \"" + pngA
         + "\"); console.log(\"MARK-A\"); }\n";
  // One macro, the way a user's drag-and-drop lands it (scene-storm.js:105-121):
  // process + cable in a single undoable command, so exactly one edge change
  // reaches the graph.
  src += "function addLight() {\n"
         "  Score.startMacro();\n"
         "  g_light = Score.createProcess(root, UUID_LIGHT, \"\");\n"
         "  if (g_light) Score.createCable(Score.outlet(g_light, 0), cIn);\n"
         "  else console.log(\"LIGHTLIVE-NOLIGHT\");\n"
         "  Score.endMacro();\n"
         "  console.log(\"MARK-ADD\");\n"
         "}\n";
  src += "function phaseB() { dev.grabFrame(3, \"" + pngB
         + "\"); console.log(\"MARK-B\"); }\n";
  // Removing the process removes its cable with it (scene-storm.js:195, :215).
  src += "function dropLight() { if (g_light) { Score.remove(g_light); "
         "g_light = null; } console.log(\"MARK-DROP\"); }\n";
  src += "function phaseC() { dev.grabFrame(3, \"" + pngC
         + "\"); console.log(\"MARK-C\"); }\n";
  src += "function finish() { Score.stop(); console.log(\"LIGHTLIVE-OK\"); "
         "Qt.exit(0); }\n";
  src += "Score.play();\n";
  src += "console.log(\"LIGHTLIVE-READY\");\n";

  const QString jsPath = dir.filePath("lightlive.js");
  REQUIRE(writeText(jsPath, src.toUtf8()));

  const auto r = runPhased(
      jsPath, dir.filePath("cache"),
      {{800, "phaseA()"},
       {2300, "addLight()"},
       {3800, "phaseB()"},
       {5300, "dropLight()"},
       {6800, "phaseC()"},
       {7800, "finish()"}});
  INFO(r.log.toStdString());

  // -- Environment SKIPs (rule: never fail for what the machine lacks).
  if(r.log.contains("LIGHTLIVE-NOPROC"))
    SKIP("this build has no threedim scene processes (Cube / Scene "
         "Preprocessor / Render Pipeline)");
  REQUIRE(r.sawReady);
  CHECK_FALSE(r.crashed);
  CHECK(r.exitCode == 0);
  REQUIRE(r.log.contains("LIGHTLIVE-OK"));
  // The offscreen forcing worked; nothing grabbed the desktop.
  REQUIRE_FALSE(r.log.contains("capturing the SCREEN"));
  // Any of these means the chain never built, which is a real red, not a skip.
  REQUIRE_FALSE(r.log.contains("LIGHTLIVE-NOPORT"));
  REQUIRE_FALSE(r.log.contains("LIGHTLIVE-NOCABLE"));
  REQUIRE_FALSE(r.log.contains("LIGHTLIVE-NOLIGHT"));

  // Split the merged log into transition segments at the MARK lines.
  const auto iA = r.log.indexOf("MARK-A");
  const auto iB = r.log.indexOf("MARK-B");
  const auto iC = r.log.indexOf("MARK-C");
  REQUIRE(iA >= 0);
  REQUIRE(iB > iA);
  REQUIRE(iC > iB);
  const QString segAdd = r.log.mid(iA, iB - iA);  // crosses the light's arrival
  const QString segDrop = r.log.mid(iB, iC - iB); // crosses its removal

  // -- The three frames exist and agree on extent. A missing PNG here means
  // grabTo refused ("nothing rendered into", WindowDevice.cpp:135-139), i.e.
  // the chain never reached the sink.
  REQUIRE(QFile::exists(pngA));
  REQUIRE(QFile::exists(pngB));
  REQUIRE(QFile::exists(pngC));
  const Frame A = loadFrame(pngA), B = loadFrame(pngB), C = loadFrame(pngC);
  REQUIRE(A.loaded);
  REQUIRE(B.loaded);
  REQUIRE(C.loaded);
  REQUIRE(A.w > 0);
  REQUIRE(A.h > 0);
  REQUIRE(B.w == A.w);
  REQUIRE(B.h == A.h);
  REQUIRE(C.w == A.w);
  REQUIRE(C.h == A.h);
  const int total = A.w * A.h;

  // -- [render] every frame really drew: the R plane carries the probe's
  // fract(gl_FragCoord.x/256) ramp, not a single flat clear value.
  {
    const int dA = distinctRedOnMiddleRow(A);
    const int dB = distinctRedOnMiddleRow(B);
    const int dC = distinctRedOnMiddleRow(C);
    INFO(
        "distinct R on the middle row: A=" << dA << " B=" << dB << " C=" << dC
                                           << " over " << A.w << "x" << A.h);
    REQUIRE(dA >= 16);
    REQUIRE(dB >= 16);
    REQUIRE(dC >= 16);
  }

  // -- [bound] scene_counts resolved to the preprocessor's REAL buffer, not to
  // the zero-filled placeholder an unresolved auxiliary gets
  // (RenderedRawRasterPipelineNode.cpp:1955-1983): draw_count > 0, so G == 255
  // everywhere. Without this, "the light never arrived" and "the probe never
  // bound scene_counts" would be indistinguishable reds.
  {
    const auto gA = channelStats(A, 1, 255);
    const auto gB = channelStats(B, 1, 255);
    const auto gC = channelStats(C, 1, 255);
    INFO(
        "green (draw_count > 0) off-pixels: A=" << gA.offCount << " B="
                                                << gB.offCount << " C="
                                                << gC.offCount << " of " << total);
    REQUIRE(gA.offCount == 0);
    REQUIRE(gB.offCount == 0);
    REQUIRE(gC.offCount == 0);
  }

  // -- [A] no light yet: light_count == 0, blue is exactly 0 everywhere.
  {
    const auto bA = channelStats(A, 2, 0);
    INFO("frame A blue: min=" << bA.minV << " max=" << bA.maxV << " non-zero="
                              << bA.offCount << " of " << total);
    CHECK(bA.offCount == 0);
  }

  // -- [B] the light is live: light_count > 0, blue is exactly 255 everywhere.
  {
    const auto bB = channelStats(B, 2, 255);
    INFO("frame B blue: min=" << bB.minV << " max=" << bB.maxV << " non-255="
                              << bB.offCount << " of " << total);
    CHECK(bB.offCount == 0);
  }

  // -- [A != B] QUANTIFIED, and "only that": every pixel differs, every one of
  // them by >= 200 in blue, and NOTHING differs in R, G or alpha. The R plane
  // is a pure function of the pixel, so R being untouched is exactly "the
  // covered pixel set did not move": adding a light moved no geometry.
  {
    const auto d = diff(A, B);
    INFO(
        "A vs B: " << d.pixels << " of " << total << " pixels differ, "
                   << d.blueOverThr << " with |dBlue| >= 200, " << d.rgaChanged
                   << " with any R/G/alpha change (worst non-blue delta "
                   << d.worstRgaDelta << ")");
    CHECK(d.blueOverThr == total);
    CHECK(d.pixels == total);
    CHECK(d.rgaChanged == 0);
    CHECK(d.worstRgaDelta == 0);
  }

  // -- [C == A] EXACT, no tolerance: removing the light puts every one of the
  // four channels back to the byte it had before the light existed.
  {
    const auto d = diff(A, C);
    INFO(
        "A vs C: " << d.pixels << " of " << total
                   << " pixels differ, worst per-channel delta " << d.worstDelta);
    CHECK(d.worstDelta == 0);
    CHECK(d.pixels == 0);
  }

  // -- The graph was edited INCREMENTALLY at both transitions, and the edge set
  // returned to exactly the size it started at. A full rebuild (full=1) still
  // renders the correct picture, so this half is the only thing that can catch
  // one -- which is why it is separate from the pixel half above.
  {
    const auto atAdd = consumes(segAdd);
    const auto atDrop = consumes(segDrop);
    INFO(
        "consume lines: " << atAdd.size() << " crossing the add, " << atDrop.size()
                          << " crossing the drop");
    REQUIRE_FALSE(atAdd.empty());
    REQUIRE_FALSE(atDrop.empty());
    for(const auto& c : atAdd)
      CHECK(c.full == 0); // [incr-add]
    for(const auto& c : atDrop)
      CHECK(c.full == 0); // [incr-drop]
    INFO(
        "edges: before add old=" << atAdd.front().oldN << ", after add new="
                                 << atAdd.back().newN << ", before drop old="
                                 << atDrop.front().oldN << ", after drop new="
                                 << atDrop.back().newN);
    CHECK(atAdd.back().newN > atAdd.front().oldN); // [grow]

    // [shrink] -- from the published SEQUENCE, not from the drop segment.
    // The drop's publish races the MARK-DROP console.log for the stderr pipe
    // (see publishedSizes): measured, it landed one line early, so a
    // segment-local test saw only "old=3 new=3". The sequence of distinct
    // published edge-set sizes over the whole run must be exactly
    // {n, n+1, n}: nothing but the light's one edge ever moved, it appeared,
    // and it went away again.
    const auto sizes = publishedSizes(r.log);
    INFO("published edge-set sizes (consecutive duplicates collapsed): "
         << [&] {
              std::string acc;
              for(long n : sizes)
                acc += std::to_string(n) + " ";
              return acc;
            }());
    REQUIRE(sizes.size() == 3);
    CHECK(sizes[1] == sizes[0] + 1); // the light's edge appeared
    CHECK(sizes[2] == sizes[0]);     // ...and only it went away again
    // [restored]: the light's edge is the only edge that ever moved.
    CHECK(atDrop.back().newN == atAdd.front().oldN);
  }
}
