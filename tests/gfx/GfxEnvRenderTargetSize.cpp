// =============================================================================
// P1-19 -- ENVIRONMENTLOADER'S render_target_size OVERRIDE IS HONOURED.
//
// What EnvironmentLoader actually is (spec G11): Threedim/EnvironmentLoader.hpp
// has NO file input -- it authors ambient / exposure / gamma / fog and
// `render_target_size`. This file tests the render_target_size half:
//
//   producer half   EnvironmentLoader.cpp:55-61 -- when both spinbox values are
//                   > 0, rebuild() stamps scene_environment::render_target_size
//                   and ORs params_render_target_size (geometry_port.hpp:1215)
//                   into params_set; 0,0 (the default) does NOT stamp the bit.
//   merge half      libossia geometry_port.cpp:425-429 -- merge_scenes carries
//                   the field group to the merged scene_state iff the bit is
//                   set, so an EnvironmentLoader overlays cleanly onto a
//                   mesh+camera scene from other producers (the exact overlay
//                   NodeRenderer::rebuildMergedScene performs per frame,
//                   NodeRenderer.cpp:476-510).
//   consumer half   ScenePreprocessorNode.cpp:3653-3665, packAndUploadCameras:
//                   `QSize rsize = renderer.state.renderSize;` (:3653) is
//                   REPLACED by the environment's render_target_size when the
//                   bit is set and both dimensions are > 0 (:3657-3664). rsize
//                   then feeds BOTH packCameraUBO call sites -- the
//                   default-camera branch (:3677) and the flattened-scene
//                   camera branch (:3688) -- which derive the projection
//                   aspect (CameraMath.cpp: aspect = w/h when h > 0) and the
//                   camera UBO's renderSize.xy from it.
//
// SCOPE, stated honestly (verified by grep across src/): packAndUploadCameras
// is the ONLY consumer of scene_environment::render_target_size in the render
// engine. The override therefore changes the CAMERA PASS's size -- the packed
// projection aspect and camera.renderSize the flattened-scene shaders receive,
// and hence the drawn frame's aspect -- but it does NOT reallocate the physical
// render target: the readback extent stays the sink size in every phase, and
// this file pins that as the current contract (the "extent" half of the spec
// sentence is the UBO renderSize the shaders see, not the texture allocation).
//
// WHAT LEVEL IS DRIVEN. Everything from the control message to the readback is
// the shipped engine, cloned from the proven CroustiCpuNodes.cpp pattern:
//   * REAL Threedim::EnvironmentLoader / Cube / Camera through the Crousti
//     wrapper (oscr::ProcessModel<T> + oscr::GfxNode<T>): a control message
//     reaches the halp struct via GpuControlIns::processControlIn
//     (GpuUtils.hpp:273-282), fires the port's update() -> rebuild()
//     (GpuUtils.hpp:144, `if_possible(t.update(state))`), and the node's
//     operator()() republishes its scene each frame
//     (CpuFilterNode.hpp:286/300, per-frame processControlIn + update).
//   * REAL scene merge: three producers wired into ScenePreprocessorNode's
//     scene input; NodeRenderer keys per (port, source) and merges with
//     ossia::merge_scenes (NodeRenderer.cpp:525).
//   * REAL camera pass: packAndUploadCameras packs the camera UBO with rsize;
//     the preprocessor publishes it as the auxiliary buffer named "camera" on
//     Geometry Out (ScenePreprocessorNode.cpp:2799-2802), and the raw-raster
//     consumer name-matches its uniform INPUT "camera" against the geometry's
//     auxiliary_buffers (RenderedRawRasterPipelineNode.cpp:1918-1924 -- the
//     documented mechanism scene_lights / per_draws / camera / env all use).
//
// PIXEL ORACLE. The probe raster shader (written to a QTemporaryDir at run
// time; the corpus has no fragment-visible camera probe) draws the cube through
// camera.viewProjection and encodes the camera UBO into flat colour:
//     R = camera.renderSize.x / 255      G = camera.renderSize.y / 255
//     B = camera.projection[0][0] / 4    A = 1
// With the Camera pinned at fov 60 (yfov/2 = 30 deg, cot = sqrt(3) = 1.7320):
//   phase 1  no override, 128x128 sink -> aspect 1: RGB = (128, 128, ~110)
//   phase 2  override {96,48}          -> aspect 2: RGB = ( 96,  48,  ~55)
//            and the cube's drawn footprint HALVES horizontally (the
//            projection x-scale is cot/aspect) while its height holds --
//            the "downstream frame's aspect follows" observable, in geometry.
//   phase 3  override cleared to {0,0} -> the sink size returns: (128,128,~110)
//            and the footprint is restored.
// The readback extent is asserted to stay 128x128 throughout (see SCOPE).
// Validation is readback dimensions/bytes, per the spec's "readback (2)".
//
// CPU-ONLY CASES (run on GPU-less boxes where every backend SKIPs): the
// producer stamp/clear contract of EnvironmentLoader.cpp:55-61, the
// merge_scenes carry (geometry_port.cpp:425-429) and packCameraUBO's
// renderSize/aspect derivation (CameraMath.cpp) are pinned without hardware.
//
// NEGATIVE CONTROL (product-side, one line, for the orchestrator):
//   src/plugins/score-plugin-gfx/Gfx/Graph/ScenePreprocessorNode.cpp:3657 --
//   change the guard
//     if((env.params_set & ossia::scene_environment::params_render_target_size)
//        && env.render_target_size[0] > 0 && env.render_target_size[1] > 0)
//   to `if(false)` (i.e. ignore the override, the spec's suggested control).
//   Phase 2 then reads (128,128,~110) instead of (96,48,~55), the footprint
//   ratio stays ~1 and the phase-1-vs-2 difference oracle goes false -- all
//   red -- while phases 1/3 and the CPU-side producer/merge cases stay green,
//   so the failure is attributable to the consumer half alone.
//
// INTENDED REGISTRATION (tests/gfx/CMakeLists.txt). EnvironmentLoader.cpp,
// Primitive.cpp and Camera.cpp are hidden-visibility inside
// score_plugin_threedim, so they are compiled into the test target, mirroring
// the test_gfx_crousti_cpu_nodes block (packCameraUBO itself is
// SCORE_PLUGIN_GFX_EXPORT and needs nothing extra):
//
//   if(TARGET score_plugin_threedim AND TARGET score_plugin_avnd)
//     set(_3d_e "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-threedim/Threedim")
//     score_plugin_hidden_sources(_env_rts_hidden
//         "${_3d_e}/Primitive.cpp"
//         "${_3d_e}/Camera.cpp"
//         "${_3d_e}/EnvironmentLoader.cpp")
//     score_add_test(test_gfx_env_render_target_size
//       SOURCES GfxEnvRenderTargetSize.cpp ${_env_rts_hidden}
//       GUI
//       PLUGINS score_plugin_gfx score_plugin_avnd score_plugin_scenario score_lib_process
//       LIBS test_gfx_engine_glue)
//     target_include_directories(test_gfx_env_render_target_size SYSTEM PRIVATE
//       "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-threedim"
//       "${SCORE_ROOT_BINARY_DIR}/src/plugins/score-plugin-threedim"
//       "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-gfx"
//       "${SCORE_ROOT_BINARY_DIR}/src/plugins/score-plugin-gfx"
//       "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-avnd"
//       "${SCORE_ROOT_BINARY_DIR}/src/plugins/score-plugin-avnd"
//       $<TARGET_PROPERTY:score_plugin_threedim,INCLUDE_DIRECTORIES>
//       $<TARGET_PROPERTY:score_plugin_avnd,INCLUDE_DIRECTORIES>
//       $<TARGET_PROPERTY:score_plugin_gfx,INCLUDE_DIRECTORIES>)
//   endif()
//
// (No GFX_TEST_CORPUS_DIR define is needed: the probe shaders are authored by
// the test into a QTemporaryDir, because no committed corpus shader exposes
// the camera UBO to the fragment stage.)
//
//   DISPLAY=:0 SCORE_TEST_API=opengl ctest -R gfx_env_render_target_size
//   DISPLAY=:0 SCORE_TEST_API=vulkan ctest -R gfx_env_render_target_size
// The render case GENERATEs over platform_backends(); unavailable backends
// SKIP, never fall back to Null. The CPU cases always run.
// =============================================================================

#include <score_test/Gfx.hpp>
#include <score_test/Document.hpp>

#include <Crousti/CpuFilterNode.hpp>
#include <Crousti/GfxNode.hpp>
#include <Crousti/CpuAnalysisNode.hpp>
#include <Crousti/ProcessModel.hpp>

#include <Threedim/Camera.hpp>
#include <Threedim/EnvironmentLoader.hpp>
#include <Threedim/Primitive.hpp>

#include <Gfx/Graph/CameraMath.hpp>
#include <Gfx/Graph/ScenePreprocessorNode.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <QFile>
#include <QMatrix4x4>
#include <QTemporaryDir>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <vector>

using namespace score::test::gfx;
using Catch::Approx;

namespace
{
constexpr int kSize = 128;     // offscreen sink: 128 x 128
constexpr int kOverrideW = 96; // the spec's override
constexpr int kOverrideH = 48;

// Camera pinned by the test: fov 60 -> yfov/2 = 30 deg -> cot = sqrt(3).
// projection[0][0] = cot / aspect (CameraMath.cpp, setReverseZPerspective).
constexpr double kCot30 = 1.7320508075688772;
inline int p00Byte(double aspect)
{
  return int(std::lround(255.0 * (kCot30 / aspect) / 4.0));
}

// --- Probe shaders -----------------------------------------------------------
// No committed corpus shader exposes the camera UBO to the fragment stage
// (syn-scene-xform.fs restricts it to "VISIBILITY": "vertex"), so the probe
// pair is authored here and written to a QTemporaryDir. Omitting VISIBILITY
// uses libisf's uniform default "vertex+fragment" (3rdparty/libisf/src/
// isf.hpp:224), so the same block serves the vertex transform and the
// fragment encoding. The LAYOUT matches CameraUBOData byte for byte
// (Gfx/Graph/CameraMath.hpp:23-31); binding is by name against the
// preprocessor's "camera" auxiliary buffer, slot 0 = active camera.

constexpr const char* kProbeVS = R"__(void main()
{
    gl_Position = clipSpaceCorrMatrix * camera.viewProjection * vec4(position.xyz, 1.0);
}
)__";

constexpr const char* kProbeFS = R"__(/*{
  "DESCRIPTION": "P1-19 probe: draws scene geometry through camera.viewProjection and encodes the camera UBO into flat colour: R = renderSize.x/255, G = renderSize.y/255, B = projection[0][0]/4. The ScenePreprocessor's render_target_size override must be visible in all three channels; the drawn footprint's aspect follows projection[0][0].",
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
    { "NAME": "camera", "TYPE": "uniform",
      "LAYOUT": [
        { "NAME": "view",           "TYPE": "mat4" },
        { "NAME": "projection",     "TYPE": "mat4" },
        { "NAME": "viewProjection", "TYPE": "mat4" },
        { "NAME": "cameraPosition", "TYPE": "vec4" },
        { "NAME": "renderSize",     "TYPE": "vec4" },
        { "NAME": "params",         "TYPE": "vec4" }
      ]
    }
  ]
}*/

void main()
{
    isf_FragColor = vec4(
        camera.renderSize.x / 255.0,
        camera.renderSize.y / 255.0,
        camera.projection[0][0] / 4.0,
        1.0);
}
)__";

bool writeFile(const QString& path, const char* text)
{
  QFile f{path};
  if(!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    return false;
  f.write(text);
  return true;
}

// --- Crousti glue (cloned from CroustiCpuNodes.cpp) --------------------------

//! Owns the ProcessModels the GfxNodes hold references to. Must outlive the
//! GfxPipeline, so declare it first at every call site.
struct HalpProcesses
{
  std::vector<std::unique_ptr<Process::ProcessModel>> models;
  int next = 1;

  template <typename T>
  std::unique_ptr<score::gfx::Node> make(const score::DocumentContext& ctx)
  {
    auto model = std::make_unique<oscr::ProcessModel<T>>(
        TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{next}, ctx, nullptr);
    auto* raw = model.get();
    models.push_back(std::move(model));
    return std::unique_ptr<score::gfx::Node>{
        new oscr::GfxNode<T>{*raw, {}, Gfx::exec_controls{}, next++, ctx}};
  }
};

//! Deliver control values to a Crousti node. CustomGfxNodeBase::process()
//! merges into last_message and keeps what it already had when a later message
//! carries none, so a call between render() batches persists across the empty
//! per-frame Timings messages the fixture pump sends -- and the renderer's
//! update() re-runs processControlIn every frame (CpuFilterNode.hpp:286), so
//! the change lands on the next rendered frame.
void setInputs(score::gfx::Node& n, std::vector<ossia::value> vals)
{
  score::gfx::Message m;
  m.node_id = n.nodeId;
  for(auto& v : vals)
    m.input.push_back(std::move(v));
  n.process(std::move(m));
}

//! EnvironmentLoader field order (EnvironmentLoader.hpp ins struct):
//! 0 ambient_color, 1 ambient_intensity, 2 ev100, 3 exposure_stops, 4 gamma,
//! 5 fog_enabled, 6 fog_color, 7 fog_start, 8 fog_end, 9 render_target_size.
//! Only slot 9 is driven; the rest stay ossia::monostate so GpuProcessIns
//! leaves them (and their update() callbacks) untouched.
void setRenderTargetSize(score::gfx::Node& envNode, int w, int h)
{
  score::gfx::Message m;
  m.node_id = envNode.nodeId;
  m.input.resize(10);
  m.input[9] = ossia::value{ossia::vec2f{float(w), float(h)}};
  envNode.process(std::move(m));
}

// --- Pixel analysis ----------------------------------------------------------

struct Phase
{
  bool valid = false;
  int width = 0, height = 0; // readback extent
  int litCount = 0;
  int r = -1, g = -1, b = -1; // modal drawn colour
  int bboxW = -1, bboxH = -1; // lit bounding box
};

Phase analyze(const ReadbackImage& img)
{
  Phase p;
  p.width = img.width;
  p.height = img.height;
  if(img.bytes.isEmpty() || img.width <= 0 || img.height <= 0)
    return p;
  p.valid = true;

  // Drawn pixels carry R = renderSize.x/255 >= 48/255 in every phase; the
  // clear colour is black. Threshold well below 48 and well above blends.
  std::map<uint32_t, int> hist;
  int minX = img.width, maxX = -1, minY = img.height, maxY = -1;
  for(int y = 0; y < img.height; ++y)
  {
    for(int x = 0; x < img.width; ++x)
    {
      const auto px = img.at(x, y);
      if(int(px[0]) >= 24)
      {
        ++p.litCount;
        minX = std::min(minX, x);
        maxX = std::max(maxX, x);
        minY = std::min(minY, y);
        maxY = std::max(maxY, y);
        const uint32_t key = (uint32_t(px[0]) << 16) | (uint32_t(px[1]) << 8)
                             | uint32_t(px[2]);
        ++hist[key];
      }
    }
  }
  if(p.litCount > 0)
  {
    uint32_t bestKey = 0;
    int bestCount = -1;
    for(const auto& [k, c] : hist)
    {
      if(c > bestCount)
      {
        bestCount = c;
        bestKey = k;
      }
    }
    p.r = int((bestKey >> 16) & 0xFF);
    p.g = int((bestKey >> 8) & 0xFF);
    p.b = int(bestKey & 0xFF);
    p.bboxW = maxX - minX + 1;
    p.bboxH = maxY - minY + 1;
  }
  return p;
}

struct Outcome
{
  bool skipped = false;
  std::string skip_reason;
  std::string error;
  std::string backend;

  Phase ph1; // no override
  Phase ph2; // override {96, 48}
  Phase ph3; // override cleared to {0, 0}
  bool diff12 = false;
};

Outcome run_env_rts(score::gfx::GraphicsApi api)
{
  Outcome out;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto* doc = score::test::new_document(app);
    if(!doc)
    {
      out.error = "cannot create a document";
      return;
    }

    QTemporaryDir shaderDir;
    if(!shaderDir.isValid())
    {
      out.error = "cannot create a temporary shader directory";
      return;
    }
    const QString vsPath = shaderDir.filePath("env-rts-probe.vs");
    const QString fsPath = shaderDir.filePath("env-rts-probe.fs");
    if(!writeFile(vsPath, kProbeVS) || !writeFile(fsPath, kProbeFS))
    {
      out.error = "cannot write the probe shaders";
      return;
    }

    HalpProcesses procs;
    GfxPipeline p;

    // Cube + Camera + EnvironmentLoader all merged into one ScenePreprocessor
    // scene input (per-(port, source) keying, NodeRenderer.cpp:476-510) --
    // the exact shape of the 15 motivating environment_loader scores. The
    // Camera makes the flattened-camera branch (:3688) the one under test,
    // not just the no-camera default (:3677); both consume the same rsize.
    const int cube = p.addNode(procs.make<Threedim::Cube>(doc->context()));
    const int cam = p.addNode(procs.make<Threedim::Camera>(doc->context()));
    const int env
        = p.addNode(procs.make<Threedim::EnvironmentLoader>(doc->context()));
    const int flat
        = p.addNode(std::make_unique<score::gfx::ScenePreprocessorNode>());
    const int raster = p.addRaster(vsPath, fsPath);
    if(cube < 0 || cam < 0 || env < 0 || flat < 0 || raster < 0)
    {
      out.error = "chain build failed: " + p.error();
      return;
    }

    auto* cubeOut = p.nodeSceneOut(cube, 0);
    auto* camOut = p.nodeSceneOut(cam, 0);
    auto* envOut = p.nodeSceneOut(env, 0);
    auto* flatIn = p.nodeSceneIn(flat, 0);
    auto* flatOut = p.nodeGeometryOut(flat, 0);
    if(!cubeOut || !camOut || !envOut || !flatIn || !flatOut)
    {
      out.error = "scene ports missing on the chain";
      return;
    }
    p.wire(cubeOut, flatIn);
    p.wire(camOut, flatIn);
    p.wire(envOut, flatIn);
    p.wire(flatOut, p.geometryIn(raster, 0));
    const int sink = p.addSink({kSize, kSize});
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));

    // Pin the camera so the oracle's closed form does not depend on control
    // defaults drifting: eye (0,1,3) looking at the origin, fov 60, the cube
    // well inside the frustum. Field order per Camera.hpp ins:
    // eye, target, fov, near_plane, far_plane.
    setInputs(
        *p.node(cam),
        {ossia::value{ossia::vec3f{0.f, 1.f, 3.f}},
         ossia::value{ossia::vec3f{0.f, 0.f, 0.f}}, ossia::value{60.f},
         ossia::value{0.1f}, ossia::value{100.f}});

    if(!p.create(api))
    {
      out.backend = p.backend();
      out.skipped = p.skipped();
      out.skip_reason = p.skipReason();
      out.error = p.error();
      return;
    }
    out.backend = p.backend();

    // ---- Phase 1: EnvironmentLoader present, override at its 0,0 default.
    // rebuild() must NOT stamp params_render_target_size, so the camera pass
    // sizes from renderer.state.renderSize == the 128x128 sink.
    p.render(6);
    const auto img1 = p.readback(sink);
    out.ph1 = analyze(img1);

    // ---- Phase 2: override {96, 48} -> aspect 2.
    setRenderTargetSize(*p.node(env), kOverrideW, kOverrideH);
    p.render(6);
    const auto img2 = p.readback(sink);
    out.ph2 = analyze(img2);
    out.diff12 = img1.bytes != img2.bytes;

    // ---- Phase 3: cleared to {0, 0} -> the sink size returns.
    setRenderTargetSize(*p.node(env), 0, 0);
    p.render(6);
    const auto img3 = p.readback(sink);
    out.ph3 = analyze(img3);
  });
  return out;
}

} // namespace

// =============================================================================
// CPU-only halves: run everywhere, including GPU-less CI.
// =============================================================================

TEST_CASE(
    "EnvironmentLoader stamps render_target_size only when both dimensions "
    "are positive, and clearing it restores the unstamped default",
    "[gfx][threedim][environment][p1-19]")
{
  Threedim::EnvironmentLoader env;

  // Default 0,0: no stamp (EnvironmentLoader.cpp:55-56 guard).
  env.rebuild();
  env();
  REQUIRE(env.m_state);
  CHECK(
      (env.m_state->environment.params_set
       & ossia::scene_environment::params_render_target_size)
      == 0u);
  const auto v0 = env.m_state->version;

  // {96, 48}: stamped (EnvironmentLoader.cpp:58-60).
  env.inputs.render_target_size.value = {kOverrideW, kOverrideH};
  env.rebuild();
  env();
  const auto& stamped = env.m_state->environment;
  CHECK(
      (stamped.params_set & ossia::scene_environment::params_render_target_size)
      != 0u);
  CHECK(stamped.render_target_size[0] == uint32_t(kOverrideW));
  CHECK(stamped.render_target_size[1] == uint32_t(kOverrideH));
  const auto v1 = env.m_state->version;
  CHECK(v1 > v0); // downstream merge memos key on (state ptr, version)

  // merge_scenes carries the stamped group onto the merged state
  // (geometry_port.cpp:425-429) -- the overlay rebuildMergedScene performs.
  {
    auto plain = std::make_shared<ossia::scene_state>();
    plain->roots = std::make_shared<std::vector<ossia::scene_node_ptr>>();
    plain->version = 1;
    std::array<ossia::scene_spec, 2> specs;
    specs[0].state = plain;
    specs[1] = env.outputs.scene_out.scene;
    const auto merged
        = ossia::merge_scenes(std::span<const ossia::scene_spec>{specs});
    REQUIRE(merged.state);
    CHECK(
        (merged.state->environment.params_set
         & ossia::scene_environment::params_render_target_size)
        != 0u);
    CHECK(merged.state->environment.render_target_size[0] == uint32_t(kOverrideW));
    CHECK(merged.state->environment.render_target_size[1] == uint32_t(kOverrideH));
  }

  // Half-authored {96, 0}: NOT stamped -- other producers with legitimate
  // sizes must still win the merge (EnvironmentLoader.cpp:47-54 comment).
  env.inputs.render_target_size.value = {kOverrideW, 0};
  env.rebuild();
  env();
  CHECK(
      (env.m_state->environment.params_set
       & ossia::scene_environment::params_render_target_size)
      == 0u);

  // Cleared {0, 0}: rebuild() resets env wholesale (env = {}), so the bit and
  // the values are gone and the preprocessor falls back to the sink size.
  env.inputs.render_target_size.value = {0, 0};
  env.rebuild();
  env();
  const auto& cleared = env.m_state->environment;
  CHECK(
      (cleared.params_set & ossia::scene_environment::params_render_target_size)
      == 0u);
  CHECK(cleared.render_target_size[0] == 0u);
  CHECK(cleared.render_target_size[1] == 0u);
  CHECK(env.m_state->version > v1);
}

TEST_CASE(
    "packCameraUBO derives the projection aspect and renderSize from the "
    "overridden size",
    "[gfx][threedim][environment][p1-19]")
{
  using namespace score::gfx;
  // The exact call shape of both ScenePreprocessorNode.cpp call sites
  // (:3677, :3688): no aspectOverride, so aspect = w/h (CameraMath.cpp).
  ossia::camera_component cam{}; // yfov 45 deg, aspect_ratio 1
  const QMatrix4x4 world;        // identity
  CameraUBOData sinkSized{}, overridden{};
  packCameraUBO(sinkSized, cam, world, QSize{kSize, kSize}, 0.f);
  packCameraUBO(overridden, cam, world, QSize{kOverrideW, kOverrideH}, 0.f);

  CHECK(sinkSized.renderSize[0] == float(kSize));
  CHECK(sinkSized.renderSize[1] == float(kSize));
  CHECK(overridden.renderSize[0] == float(kOverrideW));
  CHECK(overridden.renderSize[1] == float(kOverrideH));

  // projection[0][0] = cot(yfov/2) / aspect: doubling the aspect (96/48 = 2
  // against 128/128 = 1) halves the x scale; the y scale (cot, element [5]
  // column-major) must not move. This is the geometric "frame aspect follows"
  // half of the pixel oracle, in closed form.
  CHECK(overridden.projection[0] == Approx(sinkSized.projection[0] * 0.5f));
  CHECK(overridden.projection[5] == Approx(sinkSized.projection[5]));
}

// =============================================================================
// The render half: real producers, real merge, real camera pass, real pixels.
// =============================================================================

TEST_CASE(
    "EnvironmentLoader's render_target_size override is honoured by the "
    "ScenePreprocessor camera pass, and clearing it restores the sink size",
    "[gfx][threedim][environment][scene][p1-19]")
{
  const auto api = GENERATE(from_range(platform_backends()));

  const auto r = run_env_rts(api);
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);

  INFO("backend=" << r.backend << " error=" << r.error);
  REQUIRE(r.error.empty());

  REQUIRE(r.ph1.valid);
  REQUIRE(r.ph2.valid);
  REQUIRE(r.ph3.valid);

  INFO(
      "ph1 rgb=(" << r.ph1.r << "," << r.ph1.g << "," << r.ph1.b << ") lit="
                  << r.ph1.litCount << " bbox=" << r.ph1.bboxW << "x"
                  << r.ph1.bboxH);
  INFO(
      "ph2 rgb=(" << r.ph2.r << "," << r.ph2.g << "," << r.ph2.b << ") lit="
                  << r.ph2.litCount << " bbox=" << r.ph2.bboxW << "x"
                  << r.ph2.bboxH);
  INFO(
      "ph3 rgb=(" << r.ph3.r << "," << r.ph3.g << "," << r.ph3.b << ") lit="
                  << r.ph3.litCount << " bbox=" << r.ph3.bboxW << "x"
                  << r.ph3.bboxH);

  // The cube must actually be drawn in every phase -- an unresolved "camera"
  // aux binding would zero viewProjection and draw nothing, which fails here
  // attributably rather than passing vacuously.
  REQUIRE(r.ph1.litCount > 40);
  REQUIRE(r.ph2.litCount > 40);
  REQUIRE(r.ph3.litCount > 40);

  // SCOPE PIN: the override never reallocates the physical render target --
  // packAndUploadCameras is render_target_size's only consumer in the engine
  // (grep across src/: ScenePreprocessorNode.cpp:3657-3663 alone) -- so the
  // readback extent stays the 128x128 sink in all three phases. If this ever
  // goes red, the override gained a second consumer and this file's SCOPE
  // note must be revisited.
  CHECK(r.ph1.width == kSize);
  CHECK(r.ph1.height == kSize);
  CHECK(r.ph2.width == kSize);
  CHECK(r.ph2.height == kSize);
  CHECK(r.ph3.width == kSize);
  CHECK(r.ph3.height == kSize);

  const int tolRS = 3;  // fixture: a couple LSB between backends
  const int tolP00 = 4; // + rounding of cot(30)/4
  const int p00Sink = p00Byte(1.0);       // ~110
  const int p00Override = p00Byte(2.0);   // ~55

  // ---- Phase 1: no override -> the camera pass sizes from the sink.
  CHECK(std::abs(r.ph1.r - kSize) <= tolRS);
  CHECK(std::abs(r.ph1.g - kSize) <= tolRS);
  CHECK(std::abs(r.ph1.b - p00Sink) <= tolP00);

  // ---- Phase 2: override {96, 48} -> camera.renderSize follows exactly and
  // the projection aspect doubles (x scale halves).
  CHECK(std::abs(r.ph2.r - kOverrideW) <= tolRS);
  CHECK(std::abs(r.ph2.g - kOverrideH) <= tolRS);
  CHECK(std::abs(r.ph2.b - p00Override) <= tolP00);
  CHECK(r.diff12); // difference oracle: the override is visible in the frame

  // The DRAWN geometry follows the aspect, not just the UBO bytes: with the
  // x scale halved, the cube's footprint halves horizontally while its
  // height holds (fovY unchanged).
  const double wRatio = double(r.ph2.bboxW) / double(r.ph1.bboxW);
  const double hRatio = double(r.ph2.bboxH) / double(r.ph1.bboxH);
  INFO("footprint ratios: w=" << wRatio << " h=" << hRatio);
  CHECK(wRatio > 0.30);
  CHECK(wRatio < 0.70);
  CHECK(hRatio > 0.78);
  CHECK(hRatio < 1.28);

  // ---- Phase 3: cleared -> the sink size returns, bytes and footprint.
  CHECK(std::abs(r.ph3.r - kSize) <= tolRS);
  CHECK(std::abs(r.ph3.g - kSize) <= tolRS);
  CHECK(std::abs(r.ph3.b - p00Sink) <= tolP00);
  const double restoreRatio = double(r.ph3.bboxW) / double(r.ph1.bboxW);
  INFO("restore footprint ratio: w=" << restoreRatio);
  CHECK(restoreRatio > 0.85);
  CHECK(restoreRatio < 1.18);
}
