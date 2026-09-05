// P1-12 (SPEC-SCENE-RENDER-TESTS.md:958) -- a decoded video frame textures 3D
// geometry.
//
// The lossless clip JsGraphE2ETest.cpp already generates (MovingPattern.hpp,
// exact-equality oracle) is decoded once and fed to TWO sinks in the same
// document, in the same process, on the same render step:
//
//   Video --texture--> Render Pipeline (geoquad.fs/.vs) --> WinGeo:/
//     ^                     ^
//     |                Plane (3d_plane) --geometry-->
//     |
//     +--texture--> ISF passthrough --------------------> WinRef:/
//
// WinRef is the DECODER REFERENCE: Video -> isf-passthrough-plain.fs -> Window
// is the graph JsGraphE2ETest.cpp:254-313 already pins byte-exact
// (JsGraphE2ETest.cpp:311 `REQUIRE(v.exact == v.checked)`). Its frame index is
// therefore the decoder's own frame index, read out of a pixel path that is
// already known good. That is what makes "the index ON THE GEOMETRY advances
// 1:1 with the decoder" an assertion rather than a wall-clock guess: each
// geometry grab is bracketed by a reference grab taken immediately before and
// immediately after it, and the geometry's index must lie in that bracket.
//
// ---------------------------------------------------------------- the shape
//
// Geometry: Threedim::Plane, uuid 1e923d52-3494-49e8-8698-b001405000da
// (Primitive.hpp:44). Its mesh is vcg::tri::Grid(m, hdivs, vdivs, 1., 1.)
// (Primitive.cpp:159), whose vertices are (j*wl/(w-1), i*hl/(h-1), 0) --
// vcglib/vcg/complex/algorithms/create/platonic.h:883 -- i.e. x,y in [0,1] at
// z = 0, 16x16 divisions by default (Primitive.hpp:51-52) = 450 triangles. It
// is a real mesh travelling the real geometry port, not a full-screen quad
// shortcut.
//
// Material: Gfx::RenderPipeline::Model, uuid
// dbfc2101-40d7-4807-8804-571e88992e7e (RenderPipeline/Metadata.hpp:10), the
// "Render Pipeline material" branch the spec offers. Its ports are, in order,
// Geometry In (RenderPipeline/Process.cpp:170) then whatever the shader's
// INPUTS declare (Process.cpp:173) -- so inlet 0 is geometry and inlet 1 is
// the shader's image input, exactly as ThreedimRenderTest.cpp:683 wires it.
//
// Shaders: the FRAGMENT source is a byte-for-byte copy of the committed corpus
// shader tests/gfx/corpus/syn-rrp-image-input.fs (a pure
// `isf_FragColor = IMG_NORM_PIXEL(tex, v_uv);` -- no lighting, no colour maths,
// no second sampler), which tests/gfx/SyntheticFeatures.cpp:8-20 already
// exercises on every platform backend. Only the two-line VERTEX body is
// written here, and only because the corpus one
// (`gl_Position = vec4(position.xy, 0.0, 1.0)`) is written for the [-1,1]
// full-screen triangle syn-geo-producer.cs emits, while the Plane spans [0,1]:
//
//     v_uv = position.xy;
//     gl_Position = vec4(position.xy * 2.0 - 1.0, 0.0, 1.0);
//
// `v_uv = position.xy` is not a re-derivation of the UVs, it is numerically the
// mesh's own texcoord attribute: for a Z-facing face Primitive.cpp:89-108
// writes uv = (p.x, p.y) verbatim. Declaring `position` as vec4 while the mesh
// supplies float3 is harmless -- the vertex input FORMAT and byte offset come
// from the geometry, not from the declared type: `remapPipelineVertexInputs`
// says so in as many words at Gfx/Graph/Utils.cpp:594 ("binding/format/offset
// from GEOMETRY, location from SHADER") and builds the attribute from
// `match->format` / `match->byte_offset` at Utils.cpp:595-598; the
// descriptor-aware overload at Utils.cpp:610 does the same. The name resolves
// through findGeometryAttribute -> ossia::name_to_semantic
// (Utils.cpp:546-570).
//
// -------------------------------------------------- why byte-exact is honest
//
// "Byte-exact" here means max per-channel difference 0 over the inner half of
// every one of the 16 pattern blocks (MovingPattern.hpp:251-261), which is the
// region MovingPattern was built to make exactly-representable. The chain,
// step by step:
//
//  1. NO COLOUR CONVERSION. The clip is rawvideo RGBA muxed into NUT
//     (writeClip below, the same recipe as JsGraphE2ETest.cpp:76-101 and for
//     the same reason: every other muxer converts and the test would then be
//     asserting ffmpeg's conversion). No YUV, no matrix, no range remap.
//  2. NO TRANSFER-FUNCTION DRIFT. Every palette entry has channels that are
//     only 0 or 255 (MovingPattern.hpp:44-55) -- fixed points of every gamma
//     curve, sRGB step and 8-bit rounding a render target can apply
//     (MovingPattern.hpp:14-19). So even if some leg of the pipeline were
//     sRGB-encoded, these sixteen colours survive it.
//  3. NO FILTERING ERROR IN THE INTERIOR. The sampler is built from the port's
//     render-target spec, whose filters default to QRhiSampler::Linear
//     (Gfx/Graph/Node.hpp:62) -- so this is bilinear magnification, NOT
//     nearest. That is fine, and it is exactly the property MovingPattern's
//     header claims at MovingPattern.hpp:213-216: a bilinear tap taken at
//     least one texel inside a flat block reads four identical texels, and the
//     weighted mean of four identical 0/255 values is that value in float and
//     again after the 8-bit round trip. The blocks are 40x30 source texels
//     (160x120 / 4x4, MovingPattern.hpp:39-42) and only the inner half is
//     compared, so every sample sits >= 10 texels from a block edge in x and
//     >= 7 in y. Block EDGES are not compared and are not claimed exact.
//  4. NO MIPMAPPING. Video textures are not created UsedWithGenerateMips, and
//     the one place in the model pipeline that asks for mips guards on that
//     flag (ModelDisplayNode.cpp:1487-1498). The RenderPipeline path does not
//     request them at all.
//  5. NO BLENDING, NO PERSPECTIVE DIVIDE. The vertex shader writes w = 1 and
//     the plane is axis-aligned in NDC, so the uv->pixel map is affine and the
//     quad covers the viewport exactly; nothing composites over it.
//
// What is NOT claimed exact, and is deliberately left as a bounded statement:
//   * the block edges (resampled, see 3);
//   * the VERTICAL ORIENTATION. The raw-raster vertex path applies no
//     clipSpaceCorrMatrix (matching the corpus .vs it is modelled on) and the
//     backends disagree about framebuffer origin -- the model pipeline needs an
//     explicit `gl_Position.y = -gl_Position.y` on HLSL/MSL and an explicit uv
//     flip on SPIRV for exactly this reason (ModelDisplayNode.cpp:275-282 and
//     :362-366). Which orientation THIS path yields on THIS backend is
//     unverified (never run in this session), so the test requires exactness in
//     one orientation and requires that orientation to be the SAME for every
//     grab, and only CHECKs (non-fatally) that it is top-left. The pattern is
//     asymmetric in both axes so it can tell the two apart with certainty
//     (MovingPattern.hpp:151-153). Whoever first runs this green should tighten
//     that CHECK to a REQUIRE and record the answer.
//
// ------------------------------------------------------- the 1:1 index claim
//
// snap(i) does, in one JS statement sequence:
//     WinRef.grabFrame(2, refA_i)   WinGeo.grabFrame(2, geo_i)   WinRef.grabFrame(2, refB_i)
// grabFrame(n, path) is renderFrames(n) then grabTo(path)
// (Gfx/WindowDevice.cpp:234-238), and renderFrames drives the whole gfx
// context, so all three readbacks are separated only by a handful of
// milliseconds of rendering. All consumers of one video read the SAME decoded
// frame: VideoNode::update() calls reader.readNextFrame() once per gfx frame
// (Gfx/Graph/VideoNode.cpp:105-118) into a shared VideoFrameShare, and each
// per-RenderList VideoNodeRenderer merely notices that
// reader.m_currentFrameIdx advanced (VideoNodeRenderer.cpp:243-259). So the
// bracket min(refA, refB) <= geo <= max(refA, refB) is a true statement about
// one instant, and at 10 fps (kClipFps) the bracket can only be one frame wide
// unless a snap triple took longer than 100 ms to render -- which the test
// reports when it happens (refA != refB) rather than hiding it.
//
// ------------------------------------------------------------ NEGATIVE CONTROL
//
// The spec asks for "freeze the texture upload after frame 1 so the index stops
// advancing". The real hook is Gfx/Graph/VideoNodeRenderer.cpp:243-244:
//
//     auto reader_frame = reader.m_currentFrameIdx;
//     if(reader_frame > this->m_currentFrameIdx)
//
// becomes
//
//     if(reader_frame > this->m_currentFrameIdx && this->m_currentFrameIdx < 0)
//
// -- displayFrame() then uploads exactly the first frame and never again.
// Honest scope: that is per-VideoNodeRenderer but the SAME code runs for both
// windows (one renderer per RenderList, VideoNode.cpp:72-91), so it freezes
// the reference too. It MUST redden: "geometry frame index strictly
// increasing" and "reference frame index strictly increasing". It MUST stay
// green: every per-grab byte-exactness assertion -- a frozen frame is still a
// perfectly valid, perfectly exact frame of this pattern, which is the whole
// point of MovingPattern::Mutation::Frozen (MovingPattern.hpp:93-95) -- and
// the bracket, since a constant index still brackets itself.
//
// A second, GEOMETRY-LEG-ONLY control, for isolating this test from the
// decoder: in Gfx/Graph/RenderedRawRasterPipelineNode.cpp:119 change
//     if(sampl.texture != tex)
// to
//     if(false)
// so the Render Pipeline never binds the video texture into its SRB. That
// MUST redden every WinGeo assertion (the readback goes uniform, so
// `uniform == true` and the run fails on "no picture on the geometry") and
// MUST leave every WinRef assertion green -- which is the discrimination the
// two-window shape exists for.
//
// A third control needs no product edit and no rebuild: run with
// SCORE_TEST_PATTERN_NEGATIVE=frozen. That mutates the CLIP, not the
// assertions (MovingPattern.hpp:87-109, honoured by writeClip below), so every
// frame is frame 0: the two monotonicity assertions go red and every
// exactness assertion stays green -- the same signature as the product-side
// freeze, obtained from the producer side.
//
// --------------------------------------------------------------- SKIP policy
//
// Never red for an environment reason. ready() gates on the binary, the two
// corpus shaders, an ffmpeg to mux the clip (the libav gate: no ffmpeg, no
// lossless clip, no oracle) and a real display -- the offscreen QPA resolves
// to the Null RHI, which renders a stable, reproducible, wrong picture, so a
// pixel verdict there would be vacuous (the live-edit-sweep.sh house rule
// restated in SPEC-SCENE-RENDER-TESTS.md section 3.0, "Hardware": do not fall
// back to offscreen/Null for a case whose verdict is a pixel; exit 77 / SKIP
// instead). score_plugin_threedim is discovered
// dynamically at run time (tests/integration/CMakeLists.txt notes this at the
// splat-reload block), so it cannot be gated at configure time: instead the
// build script exits 77 when Score.createProcess returns null for the Plane or
// the Render Pipeline, and 77 becomes a Catch2 SKIP here.
//
// ------------------------------------------------------------- registration
//
// Intended block for tests/integration/CMakeLists.txt (same shape as
// test_integration_gfx_nested_interval, CMakeLists.txt:554-563). NOT added by
// this change -- cmake/ScoreTestRegistrationGuard.cmake will FATAL_ERROR the
// configure until it is:
//
//   # P1-12: a decoded video frame textures 3D geometry. The lossless
//   # MovingPattern clip drives a Render Pipeline material over a Plane mesh;
//   # the frame index visible on the geometry is bracketed by the decoder's own
//   # index read from a second window, and every geometry grab is exact on the
//   # block interiors. Drives the application binary; needs a real display and
//   # ffmpeg, and SKIPs itself otherwise.
//   if(TARGET score AND TARGET score_plugin_gfx AND NOT EMSCRIPTEN)
//     score_add_test(test_integration_gfx_video_on_geometry
//       SOURCES GfxVideoOnGeometryTest.cpp
//       LIBS ${QT_PREFIX}::Gui)
//     target_compile_definitions(test_integration_gfx_video_on_geometry PRIVATE
//       "SCORE_APP_BINARY=\"$<TARGET_FILE:score>\""
//       "GFX_TEST_CORPUS_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/../gfx/corpus\"")
//     set_tests_properties(test_integration_gfx_video_on_geometry PROPERTIES
//       TIMEOUT 600 RUN_SERIAL TRUE LABELS "gui")
//   endif()
//
// ------------------------------------------------------------- recipe notes
//
// Inherited, and each one cost a run to learn:
//   * device addresses must be "Name:/" -- a bare "Name:" fails to parse with
//     no error and no log (JsGraphE2ETest.cpp:24-26);
//   * everything goes on Score.rootInterval(). Score.createBox makes a
//     FLOATING box that never executes: ossia::scenario::get_roots() collects
//     only syncs with is_start() (libossia
//     editor/scenario/detail/scenario_execution.cpp:25-39) and only the
//     scenario's own initial sync is ever marked one (ScenarioModel.cpp:64) --
//     measured, see GfxNestedIntervalTest.cpp:12-34;
//   * `var` only in the injected script: the console engine scopes const/let
//     inside eval, so a `let` defined in the setup script is invisible to the
//     phase functions the parent injects over OSC;
//   * the phases are injected over OSC rather than busy-waited in one --script,
//     the way GfxNestedIntervalTest.cpp:90-108 and live-edit-sweep.sh do it, so
//     the main-thread event loop stays free between grabs. Port 6666 is
//     machine-global, hence the /tmp/score-harness.lock flock.
//   * Qt.vector3d(x, y, z) in the console engine returns a ZEROED vector
//     (measured, ThreedimRenderTest.cpp:650-655). Nothing here needs it: the
//     raw-raster vertex path has no camera at all, which is also why this case
//     uses the Render Pipeline branch of the spec rather than the Model
//     Display branch -- framing a Model Display camera means setting two vec3
//     controls, and that is the API gap ThreedimRenderTest.cpp:654 SKIPs on.

#include "MovingPattern.hpp"

#include <QElapsedTimer>
#include <QFile>
#include <QImage>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUdpSocket>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdlib>
#include <regex>
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

QString corpusDir()
{
#if defined(GFX_TEST_CORPUS_DIR)
  return QStringLiteral(GFX_TEST_CORPUS_DIR);
#else
  return {};
#endif
}

// 10 fps: one decoded frame every 100 ms, which is the width of the bracket a
// snap triple has to fit inside. 300 frames = 30 s, comfortably longer than the
// ~7 s this run needs.
constexpr int kClipFps = 10;
constexpr int kClipFrames = 300;

//! Grabs. 8 x 500 ms = 5 decoded frames apart, so "strictly increasing" has
//! four frames of margin per step.
constexpr int kGrabs = 8;
constexpr int kFirstSnapMs = 900;
constexpr int kSnapPeriodMs = 500;

//! Exit code the build script uses for "this process factory is not in this
//! build" -- the autotools/ctest skip convention run-corpus.sh already uses.
constexpr int kSkipExit = 77;

const char* kUuidVideo = "32dc5341-7748-4c31-a226-82e6bd685744";
const char* kUuidIsf = "74ca45ff-92c9-44a0-8f1a-754dea05ee1b";
const char* kUuidWindow = "5a181207-7d40-4ad8-814e-879fcdf8cc31";
//! Gfx::RenderPipeline::Model -- RenderPipeline/Metadata.hpp:10.
const char* kUuidRenderPipeline = "dbfc2101-40d7-4807-8804-571e88992e7e";
//! Threedim::Plane -- Primitive.hpp:44.
const char* kUuidPlane = "1e923d52-3494-49e8-8698-b001405000da";

// ---------------------------------------------------------------- the clip

//! A lossless RGBA clip of the frame-numbered pattern. rawvideo in NUT is the
//! only combination that carries the bytes through untouched; every other
//! muxer converts and the test would be asserting ffmpeg's conversion instead
//! of score's decode. Byte-for-byte the recipe JsGraphE2ETest.cpp:76-101 uses,
//! deliberately, so the two cases share one oracle.
bool writeClip(const QString& rawPath, const QString& clipPath)
{
  // SCORE_TEST_PATTERN_NEGATIVE mutates the CLIP, never the assertions
  // (MovingPattern.hpp:87-109). `frozen` must turn the two monotonicity checks
  // red and leave every exactness check green.
  const auto mut = MovingPattern::mutationFromEnvironment();
  if(mut != MovingPattern::Mutation::None)
    WARN("SCORE_TEST_PATTERN_NEGATIVE is set: this run is a negative control "
         "and is EXPECTED to fail");
  if(!MovingPattern::writeRawFrames(rawPath, kClipFrames, mut))
    return false;

  QProcess ff;
  ff.setProcessChannelMode(QProcess::MergedChannels);
  ff.start(
      "ffmpeg",
      {"-nostdin", "-loglevel", "error", "-y", "-f", "rawvideo", "-pix_fmt", "rgba",
       "-s",
       QString::number(MovingPattern::kWidth) + "x"
           + QString::number(MovingPattern::kHeight),
       "-r", QString::number(kClipFps), "-i", rawPath, "-c:v", "rawvideo", "-f", "nut",
       clipPath});
  if(!ff.waitForStarted(10000) || !ff.waitForFinished(120000))
    return false;
  return ff.exitCode() == 0 && QFile::exists(clipPath);
}

// -------------------------------------------------------------- the shaders

//! The vertex half of the material. See the header: the Plane spans [0,1]^2
//! (Primitive.cpp:159 -> platonic.h:883) and its own texcoord attribute is
//! (x, y) (Primitive.cpp:89-108), so this maps the mesh onto the whole
//! viewport, axis-aligned and unscaled, with w = 1 (no perspective divide).
//! Kept comment-free: the committed corpus .vs files carry no comments and the
//! ISF vertex preprocessor has never been asked to survive one here.
constexpr auto kVertexShader = R"GLSL(void main()
{
  v_uv = position.xy;
  gl_Position = vec4(position.xy * 2.0 - 1.0, 0.0, 1.0);
}
)GLSL";

//! Copy tests/gfx/corpus/syn-rrp-image-input.fs into `dst` verbatim and write
//! the vertex sibling next to it. RenderPipeline::Model finds the .vs by
//! basename in the .fs's own directory (RenderPipeline/Process.cpp:32), so the
//! pair has to live in the same directory -- hence the copy rather than
//! pointing straight at the corpus.
bool writeMaterial(const QString& fsDst, const QString& vsDst)
{
  QFile src{corpusDir() + "/syn-rrp-image-input.fs"};
  if(!src.open(QIODevice::ReadOnly))
    return false;
  const QByteArray fs = src.readAll();
  if(fs.isEmpty())
    return false;

  QFile out{fsDst};
  if(!out.open(QIODevice::WriteOnly) || out.write(fs) != fs.size())
    return false;
  out.close();

  QFile vs{vsDst};
  if(!vs.open(QIODevice::WriteOnly))
    return false;
  const QByteArray v = QByteArray{kVertexShader};
  return vs.write(v) == v.size();
}

// --------------------------------------------------------------- the runner

struct Run
{
  int exitCode{-1};
  bool crashed{true};
  bool sawReady{false};
  QString log;
};

//! One OSC `/script s <code>` to the app's LocalTree script node on udp/6666 --
//! byte-identical to what `oscsend 127.0.0.1 6666 /script s ...` sends in
//! live-edit-sweep.sh, and evaluated in the same console engine as --script
//! (JS/ApplicationPlugin.cpp:192-205), so the setup script's `var`s are in
//! scope.
void sendScript(QUdpSocket& sock, const QByteArray& code)
{
  auto pad4 = [](QByteArray b) {
    b.append('\0');
    while(b.size() % 4)
      b.append('\0');
    return b;
  };
  sock.writeDatagram(
      pad4("/script") + pad4(",s") + pad4(code), QHostAddress::LocalHost, 6666);
}

Run runPhased(const QString& js, const std::vector<std::pair<int, QByteArray>>& phases)
{
  auto env = QProcessEnvironment::systemEnvironment();
  // Both window devices render offscreen: with a mapped window the grab reads
  // the SCREEN at its geometry, i.e. the desktop, which is never blank and
  // would sail through any non-blankness check.
  env.insert("SCORE_FORCE_OFFSCREEN_WINDOW", "WinGeo,WinRef");
  env.insert("SCORE_AUDIO_BACKEND", "dummy");
  env.insert("SCORE_DISABLE_AUDIOPLUGINS", "1");
  // GfxContext prints its edge-consume decisions; used here as a diagnostic
  // that the graph really acquired the geometry and texture edges, not as the
  // pixel verdict.
  env.insert("SCORE_GFX_TRACE", "1");
  // The platform's own backend, not OpenGL everywhere: a headless Windows
  // session has no WGL and no opengl32sw, so an OpenGL request there creates no
  // context and every grab comes back blank (JsGraphE2ETest.cpp:119-128).
#if defined(_WIN32)
  constexpr auto defaultApi = "d3d11";
#elif defined(__APPLE__)
  constexpr auto defaultApi = "metal";
#else
  constexpr auto defaultApi = "opengl";
#endif
  env.insert("QSG_RHI_BACKEND", qEnvironmentVariable("SCORE_TEST_API", defaultApi));
  env.remove("QT_QPA_PLATFORM");
  // Every verdict about ordering is read out of the child's merged log, and on
  // Windows a process with no console gets the debugger as its message handler.
  env.insert("QT_FORCE_STDERR_LOGGING", "1");
  env.insert("QT_ASSUME_STDERR_HAS_CONSOLE", "1");

  Run r;

#if defined(Q_OS_UNIX)
  // OSC port 6666 is machine-global: serialize against live-edit-sweep.sh by
  // taking the very same advisory lock it holds around each scenario, the way
  // GfxNestedIntervalTest.cpp:253-261 does.
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
    QElapsedTimer boot;
    boot.start();
    while(boot.elapsed() < 90000 && p.state() == QProcess::Running
          && !r.log.contains("GEO-READY"))
      pump(100);
    r.sawReady = r.log.contains("GEO-READY");

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

#if defined(Q_OS_UNIX)
  if(lockFd >= 0)
    ::close(lockFd); // releases the flock
#endif
  return r;
}

QString writeScript(const QTemporaryDir& dir, const QString& name, const QString& src)
{
  const QString path = dir.filePath(name);
  QFile f{path};
  REQUIRE(f.open(QIODevice::WriteOnly));
  f.write(src.toUtf8());
  return path;
}

// -------------------------------------------------------------- the oracles

//! A reading that accepts EITHER vertical orientation, and says which one it
//! got. MovingPattern::read() only calls top-left a match (by design:
//! MovingPattern.hpp:268-272) and, when the picture is flipped, returns the
//! top-left reading whose frame index was decoded from the wrong row -- so the
//! index cannot be recovered from it. detail::readAt is the same comparison,
//! per orientation, which is what this needs. See the header for why this path
//! is not allowed to assume the orientation.
struct GeoReading
{
  MovingPattern::Reading r;
  MovingPattern::Orientation orientation{MovingPattern::Orientation::TopLeft};
  bool loaded{false};
};

GeoReading readEitherOrientation(const QString& path)
{
  GeoReading g;
  QImage in{path};
  if(in.isNull())
    return g;
  g.loaded = true;
  const QImage img = in.convertToFormat(QImage::Format_RGB32);
  if(MovingPattern::isUniform(img))
  {
    g.r.uniform = true;
    return g;
  }
  auto tl = MovingPattern::detail::readAt(img, MovingPattern::Orientation::TopLeft);
  if(tl.exact())
  {
    g.r = tl;
    g.orientation = MovingPattern::Orientation::TopLeft;
    return g;
  }
  auto bl = MovingPattern::detail::readAt(img, MovingPattern::Orientation::BottomLeft);
  if(bl.exact())
  {
    g.r = bl;
    g.orientation = MovingPattern::Orientation::BottomLeft;
    return g;
  }
  // Neither is exact: report the top-left failure, which is the one a reader
  // wants to see first.
  g.r = tl;
  return g;
}

struct Consume
{
  long oldN{};
  long newN{};
  int full{};
};

//! Every "GFX-EDGES consume old=<n> new=<n> full=<d>" line, in order -- the
//! exact line GfxContext.cpp:918 emits under SCORE_GFX_TRACE, parsed the way
//! GfxNestedIntervalTest.cpp:371-382 and GfxEdgeConsumeLatch.cpp parse it.
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

bool ready()
{
  if(appBinary().isEmpty() || !QFile::exists(appBinary()))
    return false;
  if(corpusDir().isEmpty())
    return false;
  if(!QFile::exists(corpusDir() + "/syn-rrp-image-input.fs"))
    return false;
  if(!QFile::exists(corpusDir() + "/isf-passthrough-plain.fs"))
    return false;
  // The libav gate: no ffmpeg, no lossless clip, no exact-equality oracle.
  if(QStandardPaths::findExecutable("ffmpeg").isEmpty())
    return false;
  // The offscreen QPA has no GL: the readback comes back flat, proving
  // nothing. Same convention as FrameDeterminismTest.cpp and
  // GfxNestedIntervalTest.cpp:384-400.
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
    "a decoded video frame textures 3D geometry",
    "[integration][gfx][js][media][threedim]")
{
  if(!ready())
    SKIP("needs the score binary, the gfx corpus, ffmpeg and a display");

  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  if(qEnvironmentVariableIsSet("SCORE_TEST_KEEP_ARTIFACTS"))
  {
    dir.setAutoRemove(false);
    WARN("artifacts kept in " << dir.path().toStdString());
  }

  const QString clip = dir.filePath("pattern.nut");
  INFO("ffmpeg must be able to mux rawvideo into NUT");
  REQUIRE(writeClip(dir.filePath("frames.rgba"), clip));

  const QString fs = dir.filePath("geoquad.fs");
  const QString vs = dir.filePath("geoquad.vs");
  INFO("the fragment half is a verbatim copy of corpus/syn-rrp-image-input.fs");
  REQUIRE(writeMaterial(fs, vs));

  const QString geoStem = dir.filePath("geo");
  const QString refAStem = dir.filePath("refA");
  const QString refBStem = dir.filePath("refB");

  QString src;
  src += QStringLiteral("var UUID_VIDEO = \"%1\";\n").arg(kUuidVideo);
  src += QStringLiteral("var UUID_ISF = \"%1\";\n").arg(kUuidIsf);
  src += QStringLiteral("var UUID_RP = \"%1\";\n").arg(kUuidRenderPipeline);
  src += QStringLiteral("var UUID_PLANE = \"%1\";\n").arg(kUuidPlane);
  src += QStringLiteral("var UUID_WINDOW = \"%1\";\n").arg(kUuidWindow);
  src += "Score.createDevice(\"WinGeo\", UUID_WINDOW, {});\n";
  src += "Score.createDevice(\"WinRef\", UUID_WINDOW, {});\n";
  // A Scenario created from script ends up nested and never executes; the
  // document's own Scenario.1 is removed and everything hangs off the root
  // interval, which is the only place a process is guaranteed to tick.
  src += "var s = Score.find(\"Scenario.1\"); if (s) Score.remove(s);\n";
  src += "var root = Score.rootInterval();\n";

  // -- the decoder, once, feeding both sinks.
  src += "var vid = Score.createProcess(root, UUID_VIDEO, \"" + clip + "\");\n";
  src += "if (!vid) { console.log(\"GEO-SKIP: no Video process factory\"); "
         "Qt.exit(77); }\n";
  // Scale.hpp:9-15 -- 3 = Stretch, so the 160x120 clip fills the video node's
  // output texture instead of sitting 1:1 in a corner of it.
  src += "vid.scaleMode = 3;\n";
  // Scale.hpp:20-25 -- 2 = FrameQueue, so the queue renderer is used
  // deterministically rather than left to the AutoPlayback codec heuristic.
  src += "vid.playbackMode = 2;\n";
  src += "console.log(\"SCALEMODE \" + vid.scaleMode + \" PLAYBACK \" + "
         "vid.playbackMode);\n";

  // -- the geometry leg: Plane -> Render Pipeline(geoquad) -> WinGeo.
  src += "var plane = Score.createProcess(root, UUID_PLANE, \"\");\n";
  src += "if (!plane) { console.log(\"GEO-SKIP: no Plane primitive "
         "(score_plugin_threedim not loaded)\"); Qt.exit(77); }\n";
  src += "var rp = Score.createProcess(root, UUID_RP, \"" + fs + "\");\n";
  src += "if (!rp) { console.log(\"GEO-SKIP: no Render Pipeline process "
         "factory\"); Qt.exit(77); }\n";
  // Inlet 0 is Geometry In (RenderPipeline/Process.cpp:170); the shader's
  // INPUTS follow (Process.cpp:173), so "tex" is inlet 1. Ask by name first so
  // a future extra port cannot silently shift the index.
  src += "var geoIn = Score.inlet(rp, 0);\n";
  src += "var texIn = Score.inlet(rp, \"tex\") || Score.inlet(rp, 1);\n";
  src += "if (!geoIn || !texIn) { console.log(\"GEO-ERROR: render pipeline "
         "ports (\" + Score.inlets(rp) + \" inlets)\"); Qt.exit(10); }\n";
  src += "var cGeo = Score.createCable(Score.outlet(plane, 0), geoIn);\n";
  src += "var cTex = Score.createCable(Score.outlet(vid, 0), texIn);\n";
  src += "if (!cGeo || !cTex) { console.log(\"GEO-ERROR: geometry/texture "
         "cables\"); Qt.exit(11); }\n";
  src += "Score.setAddress(Score.outlet(rp, 0), \"WinGeo:/\");\n";

  // -- the reference leg: the graph JsGraphE2ETest.cpp already pins exact.
  src += "var flt = Score.createProcess(root, UUID_ISF, \"" + corpusDir()
         + "/isf-passthrough-plain.fs\");\n";
  src += "if (!flt) { console.log(\"GEO-SKIP: no ISF process factory\"); "
         "Qt.exit(77); }\n";
  src += "var cRef = Score.createCable(Score.outlet(vid, 0), Score.inlet(flt, 0));\n";
  src += "if (!cRef) { console.log(\"GEO-ERROR: reference cable\"); Qt.exit(12); }\n";
  src += "Score.setAddress(Score.outlet(flt, 0), \"WinRef:/\");\n";

  src += "var dGeo = Score.device(\"WinGeo\"), dRef = Score.device(\"WinRef\");\n";
  src += "if (!dGeo || !dRef) { console.log(\"GEO-ERROR: window devices\"); "
         "Qt.exit(13); }\n";

  // The phase functions the parent injects over OSC. Defined here so the event
  // loop stays free between snaps. The reference is grabbed immediately before
  // AND immediately after the geometry, so the geometry's index is bracketed
  // by the decoder's own index at that instant.
  src += "function snap(i) {\n";
  src += "  dRef.grabFrame(2, \"" + refAStem + "\" + i + \".png\");\n";
  src += "  dGeo.grabFrame(2, \"" + geoStem + "\" + i + \".png\");\n";
  src += "  dRef.grabFrame(2, \"" + refBStem + "\" + i + \".png\");\n";
  src += "  console.log(\"MARK-GRAB \" + i);\n";
  src += "}\n";
  src += "function finish() { Score.stop(); console.log(\"GEO-OK\"); Qt.exit(0); }\n";
  src += "Score.play();\n";
  src += "console.log(\"GEO-READY\");\n";

  std::vector<std::pair<int, QByteArray>> phases;
  for(int i = 0; i < kGrabs; i++)
    phases.emplace_back(
        kFirstSnapMs + i * kSnapPeriodMs,
        QByteArray{"snap("} + QByteArray::number(i) + ")");
  phases.emplace_back(kFirstSnapMs + kGrabs * kSnapPeriodMs + 500, "finish()");

  const auto r = runPhased(writeScript(dir, "video-on-geometry.js", src), phases);
  INFO(r.log.toStdString());

  // A process factory that is not in this build is an environment fact, not a
  // defect: score_plugin_threedim is loaded dynamically and cannot be gated at
  // configure time.
  if(r.exitCode == kSkipExit || r.log.contains("GEO-SKIP:"))
    SKIP("a required process factory is not in this build: "
         << r.log.right(400).toStdString());

  REQUIRE(r.sawReady);
  CHECK_FALSE(r.crashed);
  CHECK(r.exitCode == 0);
  REQUIRE(r.log.contains("GEO-OK"));
  // The offscreen forcing worked; nothing grabbed the desktop.
  REQUIRE_FALSE(r.log.contains("capturing the SCREEN"));

  // -- the graph really built: at least one edge-consume decision, and the
  // last one holds a non-empty edge set. Diagnostic, not the pixel verdict:
  // the picture is the verdict, this only makes "the geometry cable never
  // materialised" readable at a glance.
  {
    const auto cs = consumes(r.log);
    INFO("GFX-EDGES consume lines: " << cs.size());
    CHECK_FALSE(cs.empty());
    if(!cs.empty())
    {
      INFO("last consume: old=" << cs.back().oldN << " new=" << cs.back().newN);
      CHECK(cs.back().newN > 0);
    }
  }

  // ------------------------------------------------------------- the pixels
  struct Snap
  {
    GeoReading geo;
    MovingPattern::Reading refA, refB;
    bool present{false};
  };
  std::vector<Snap> snaps;
  QString detail;
  for(int i = 0; i < kGrabs; i++)
  {
    const QString g = geoStem + QString::number(i) + ".png";
    const QString a = refAStem + QString::number(i) + ".png";
    const QString b = refBStem + QString::number(i) + ".png";
    Snap s;
    s.present = QFile::exists(g) && QFile::exists(a) && QFile::exists(b);
    if(s.present)
    {
      s.geo = readEitherOrientation(g);
      s.refA = MovingPattern::readFile(a);
      s.refB = MovingPattern::readFile(b);
    }
    detail += QStringLiteral(
                  "snap %1: present=%2 geo{%3 frame=%4 sampled=%5 mismatched=%6 "
                  "orient=%7} refA{%8 frame=%9} refB{%10 frame=%11}\n")
                  .arg(i)
                  .arg(s.present ? 1 : 0)
                  .arg(s.geo.r.uniform ? "flat" : "picture")
                  .arg(s.geo.r.frame)
                  .arg(s.geo.r.sampled)
                  .arg(s.geo.r.mismatched)
                  .arg(
                      s.geo.orientation == MovingPattern::Orientation::TopLeft
                          ? "top-left"
                          : "BOTTOM-LEFT")
                  .arg(s.refA.uniform ? "flat" : "picture")
                  .arg(s.refA.frame)
                  .arg(s.refB.uniform ? "flat" : "picture")
                  .arg(s.refB.frame);
    snaps.push_back(s);
  }
  INFO(detail.toStdString());

  // Leading flat grabs are the readback before the first buffer arrives, not a
  // defect (MovingPattern.hpp:177-179). Two are tolerated; the run is 900 ms in
  // before the first snap, so more than that means the graph was slow to build
  // and the remaining evidence is thin.
  std::size_t first = 0;
  while(first < snaps.size()
        && (!snaps[first].present || snaps[first].geo.r.uniform
            || snaps[first].refA.uniform || snaps[first].refB.uniform))
    first++;
  INFO("first usable snap: " << first << " of " << snaps.size());
  REQUIRE(first <= 2);
  REQUIRE(snaps.size() - first >= 5);

  // -- 1. The REFERENCE leg is the graph JsGraphE2ETest.cpp:254-313 already
  // pins byte-exact, top-left. If this half is red the run says nothing about
  // the geometry: the clip, the decode or the display is at fault.
  for(std::size_t i = first; i < snaps.size(); i++)
  {
    INFO("reference readback, snap " << i);
    REQUIRE(snaps[i].refA.exact());
    REQUIRE(snaps[i].refB.exact());
  }

  // -- 2. The frame ON THE GEOMETRY is byte-exact: every pixel of the inner
  // half of every block is EXACTLY the colour the generator wrote, max
  // per-channel difference 0. See the header for why that region is
  // exactly-representable through this pipeline and why the block edges are
  // not part of the claim.
  MovingPattern::Orientation orientation = snaps[first].geo.orientation;
  for(std::size_t i = first; i < snaps.size(); i++)
  {
    const auto& g = snaps[i].geo;
    INFO(
        "geometry readback, snap " << i << ": sampled " << g.r.sampled
                                   << " pixels, " << g.r.mismatched
                                   << " not exactly the expected colour");
    REQUIRE(g.loaded);
    REQUIRE_FALSE(g.r.uniform); // a flat picture means no frame reached the mesh
    REQUIRE(g.r.frame >= 0);
    REQUIRE(g.r.sampled > 0);
    REQUIRE(g.r.mismatched == 0);
    // One orientation for the whole run: a path that flips only sometimes is a
    // defect even if each frame is individually exact.
    REQUIRE(g.orientation == orientation);
  }
  // Not a REQUIRE: which orientation this backend yields through the
  // raw-raster path is unverified here (see the header). A bottom-left run is
  // still exact, and this is the line that records it.
  CHECK(orientation == MovingPattern::Orientation::TopLeft);

  // -- 3. The index on the geometry advances 1:1 with the decoder: for every
  // snap it lies between the reference indices taken immediately before and
  // immediately after it. A wide bracket (refA != refB) means the triple
  // straddled a decoded-frame boundary, which is legal at 10 fps but is
  // reported.
  int straddled = 0;
  for(std::size_t i = first; i < snaps.size(); i++)
  {
    const int a = snaps[i].refA.frame;
    const int b = snaps[i].refB.frame;
    const int g = snaps[i].geo.r.frame;
    const int lo = std::min(a, b), hi = std::max(a, b);
    INFO(
        "snap " << i << ": decoder bracket [" << lo << ", " << hi
                << "], geometry shows " << g);
    CHECK(b >= a); // the decoder never runs backwards inside one triple
    REQUIRE(g >= lo);
    REQUIRE(g <= hi);
    if(a != b)
      straddled++;
  }
  INFO(
      straddled << " of " << (snaps.size() - first)
                << " snaps straddled a decoded-frame boundary");

  // -- 4. Both indices actually MOVE, strictly, snap to snap. This is the half
  // the frozen-upload negative control has to redden; #2 and #3 must stay
  // green under it.
  for(std::size_t i = first + 1; i < snaps.size(); i++)
  {
    INFO("snap " << (i - 1) << " -> " << i);
    REQUIRE(snaps[i].geo.r.frame > snaps[i - 1].geo.r.frame);
    REQUIRE(snaps[i].refA.frame > snaps[i - 1].refA.frame);
  }
  // ...and by the same total, to within the one decoded frame a bracket can be
  // wide. A geometry leg that advanced at half the decoder's rate would pass
  // "strictly increasing" and fails here.
  {
    const int dGeo = snaps.back().geo.r.frame - snaps[first].geo.r.frame;
    const int dRef = snaps.back().refA.frame - snaps[first].refA.frame;
    INFO("geometry advanced " << dGeo << " frames, decoder " << dRef);
    REQUIRE(dGeo > 0);
    REQUIRE(std::abs(dGeo - dRef) <= 1);
  }
}
