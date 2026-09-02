// =============================================================================
// P1-3 -- SWITCHING THE ACTIVE CAMERA MID-RENDER MOVES THE IMAGE, AND
// SWITCHING BACK REPRODUCES THE FIRST FRAME BYTE-FOR-BYTE.
//
// HOW THE ACTIVE CAMERA IS ACTUALLY SELECTED (established before writing,
// since the spec only guessed `active_camera_id`):
//
//   field       scene_state::active_camera_id, a scene_node_id
//               (3rdparty/libossia/src/ossia/dataflow/geometry_port.hpp:1280).
//   producer    Threedim::Camera mints a stable non-zero node id on first
//               rebuild (Camera.hpp:110), stamps it on its emitted scene_node
//               (Camera.hpp:171) and sets active_camera_id = that id on every
//               rebuild (Camera.hpp:179) -- each Camera nominates ITSELF.
//   merge       ossia::merge_scenes keeps the FIRST non-zero active_camera_id
//               in contributor order (geometry_port.cpp:467-468) and stamps it
//               on the merged state (geometry_port.cpp:556).
//   flatten     flattenScene collects tree-payload cameras with the enclosing
//               node's id (SceneGPUState.cpp:543-551; currentNodeId set in
//               visitNode, SceneGPUState.cpp:644) and resolves
//               FlatScene::activeCameraIndex by matching active_camera_id,
//               with fallback to index 0 when the id is zero or unmatched
//               (SceneGPUState.cpp:930-947).
//   pack        packAndUploadCameras packs the ACTIVE camera into UBO slot 0,
//               the rest after it (`const int active = std::max(0,
//               fs.activeCameraIndex);`, ScenePreprocessorNode.cpp:3685,
//               pack loop :3683-3695); the flatten+pack runs unconditionally
//               every frame (ScenePreprocessorNode.cpp:3766-3781) and the
//               buffer is diff-uploaded.
//   consume     downstream bindings expose ONLY the active slot: "Only bind
//               the ACTIVE camera slot (first 240 bytes) ... Slot 0 is
//               guaranteed to be the active camera by packAndUploadCameras"
//               (ScenePreprocessorNode.cpp:2728-2731); raw-raster shaders get
//               it as the "camera" auxiliary buffer by name
//               (ScenePreprocessorNode.cpp:2800-2802, mechanism proven by
//               tests/gfx/GfxEnvRenderTargetSize.cpp). packCameraUBO's two
//               call sites are the camera's ONLY road into the frame
//               (tests/gfx/GfxCameraProjectionPin.cpp header, verified by
//               grep there).
//
// WHAT THE USER-FACING SWITCHES ARE, and what this file drives:
//   * A CameraSwitch process (Threedim/CameraSwitch.hpp -- the "CameraSwitch
//     index" the spec guessed) whose Select mode forwards the picked
//     upstream camera scene verbatim (CameraSwitch.hpp:320-339, `index`
//     control :92-93). TRANSPORT CAVEAT, found while writing this test: at
//     the score::gfx graph level every scene INPUT field of a halp node
//     receives the SAME merged scene -- scene_inputs_storage::readInputScenes
//     assigns the node's single merged `this->scene` to every field
//     (Crousti/GpuUtils.hpp:1776-1785), and that merged scene is built by
//     NodeRenderer::rebuildMergedScene across ALL (port, source) entries
//     regardless of which port they arrived on (NodeRenderer.cpp:481-527,
//     per-port store :585-596). So wiring two Camera GfxNodes into a
//     GfxNode<CameraSwitch> cannot exercise per-port Select in this harness
//     (all four cam inputs would see the same union). The Select contract is
//     therefore pinned at the halp level (CPU case below), honestly labelled.
//   * Merge ORDER: with two Cameras wired straight into one preprocessor,
//     the merged active_camera_id is the first contributor's
//     (geometry_port.cpp:467-468) -- real, pinned below at CPU level, but the
//     per-(port,source) map order is not a per-frame user knob, so it cannot
//     drive the render leg.
//
// RENDER LEG DESIGN. Both cameras stay IN the scene the whole run, in a
// FIXED roots order [camA, camB]; only active_camera_id toggles between
// phases. That isolates exactly the selection sites named above (the same
// state shape merge_scenes produces, with only the id field moving), makes
// the negative control decisive (see below), and is the state a
// CameraSwitch-class producer authors. The camera data itself -- scene
// nodes, transforms, camera components, ids -- is authored by two REAL
// Threedim::Camera processes ticked by the test on the CPU (the same
// direct-tick pattern GfxEnvRenderTargetSize.cpp uses for EnvironmentLoader);
// the cube mesh is a REAL Threedim::Cube through the proven Crousti GfxNode
// wrapper; the merge, flatten, camera pack and aux-buffer binding are the
// shipped engine (ScenePreprocessorNode fed via NodeRenderer::process, the
// GfxMaterialTextureSwap.cpp publisher pattern).
//
// PIXEL ORACLE. The probe raster (written to a QTemporaryDir; same "camera"
// LAYOUT block as GfxEnvRenderTargetSize.cpp, byte-matching CameraUBOData)
// draws the cube through camera.viewProjection and encodes the ACTIVE
// camera's position into flat colour:
//     R = (cameraPosition.x + 4) / 8    G = (y + 4) / 8    B = (z + 4) / 8
// camA eye (0,1,3)  ->  RGB ~ (128, 159, 223)
// camB eye (3,1,0)  ->  RGB ~ (223, 159, 128)
// so the drawn colour identifies WHICH camera is active, not merely that
// "something changed". Phase A (active=camA) != phase B (active=camB) beyond
// a 4-LSB margin per channel and as whole-frame bytes; phase C (active back
// to camA) must equal phase A BYTE-FOR-BYTE. Byte-exactness is expected
// because the pack is deterministic from identical inputs, the camera buffer
// is diff-uploaded, and the probe does not sample `camera_prev` (which
// legitimately differs across the return -- phase C's previous frame was
// camB); if a backend ever proves non-exact here, the measured max channel
// diff is INFO'd for the record and the gate should be revisited with that
// number in hand.
//
// NOT A REBUILD. The registry snapshot technique from
// GfxMaterialTextureSwap.cpp: the publisher's renderer snapshots the
// BaseColor texture channel (bucket array pointers, layer counts, dynamic
// slots) every frame; all three phases must be identical -- an active-camera
// switch flows through the per-frame camera flatten+pack and the diff-upload
// (ScenePreprocessorNode.cpp:3766-3781), never through a texture-channel
// reallocation. Scope stated honestly: this pins the no-texture-realloc
// half; the byte-exact phase-C return is what pins that nothing downstream
// (passes, bindings, meshes) drifted either -- a recreated/rebound pass that
// lost state would not reproduce phase A exactly.
//
// CPU-ONLY CASES (run on GPU-less boxes): distinct self-nominating ids
// (Camera.hpp:110/179), merge-order first-non-zero-wins
// (geometry_port.cpp:467-468/556), CameraSwitch Select forwarding with exact
// pointer return (CameraSwitch.hpp:320-339), and flattenScene's id-to-index
// resolution incl. the fallback (SceneGPUState.cpp:930-947; flattenScene is
// SCORE_PLUGIN_GFX_EXPORT, SceneGPUState.hpp:597-601).
//
// NEGATIVE CONTROL (product-side, one line, for the orchestrator):
//   src/plugins/score-plugin-gfx/Gfx/Graph/ScenePreprocessorNode.cpp:3685 --
//   change `const int active = std::max(0, fs.activeCameraIndex);` to
//   `const int active = 0;` (hard-code camera index 0 at the selection site,
//   the spec's control). The roots order is FIXED [camA, camB], so slot 0 is
//   then always camA: phase B renders identically to phase A -- its modal
//   colour and the A/B difference oracle go red -- while the CPU cases
//   (including the flattenScene resolution case, which does not go through
//   the pack site) and the phase-C byte return stay green. The failure is
//   attributable to the pack-site selection alone. The other half of the
//   selection lives at SceneGPUState.cpp:936-945 (the id-match loop);
//   deleting that loop instead turns the flattenScene CPU case red.
//
// INTENDED REGISTRATION (tests/gfx/CMakeLists.txt), mirroring the
// test_gfx_env_render_target_size block (Primitive.cpp and Camera.cpp are
// hidden-visibility inside score_plugin_threedim, so they are compiled into
// the test target; CameraSwitch is header-only and needs no extra TU). The
// OffsetAllocator lines are the test_gfx_dynamic_slot insurance carried by
// GfxMaterialTextureSwap.cpp:90-103, because this TU reads
// GpuResourceRegistry state through RenderList::registry():
//
//   if(TARGET score_plugin_threedim AND TARGET score_plugin_avnd)
//     set(_3d_cs "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-threedim/Threedim")
//     score_plugin_hidden_sources(_cam_switch_hidden
//         "${_3d_cs}/Primitive.cpp"
//         "${_3d_cs}/Camera.cpp")
//     score_add_test(test_gfx_camera_switch_live
//       SOURCES GfxCameraSwitchLive.cpp ${_cam_switch_hidden}
//       GUI
//       PLUGINS score_plugin_gfx score_plugin_avnd score_plugin_scenario score_lib_process
//       LIBS test_gfx_engine_glue)
//     target_sources(test_gfx_camera_switch_live PRIVATE
//       "${SCORE_ROOT_SOURCE_DIR}/3rdparty/OffsetAllocator/offsetAllocator.cpp")
//     target_include_directories(test_gfx_camera_switch_live SYSTEM PRIVATE
//       "${SCORE_ROOT_SOURCE_DIR}/3rdparty/OffsetAllocator"
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
//   DISPLAY=:0 SCORE_TEST_API=opengl ctest -R gfx_camera_switch_live
//   DISPLAY=:0 SCORE_TEST_API=vulkan ctest -R gfx_camera_switch_live
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
#include <Threedim/CameraSwitch.hpp>
#include <Threedim/Primitive.hpp>

#include <Gfx/Graph/GpuResourceRegistry.hpp>
#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/SceneGPUState.hpp>
#include <Gfx/Graph/ScenePreprocessorNode.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <QFile>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <vector>

using namespace score::test::gfx;

namespace
{
constexpr int kSize = 128; // offscreen sink: 128 x 128 (P1-19's proven framing)

// The two camera eyes. Both look at the origin with fov 60 / near 0.1 /
// far 100; camA is the exact placement GfxEnvRenderTargetSize.cpp proved
// draws the cube with litCount > 40 at this sink size. camB is the same
// distance away on the +X axis, so the cube stays well inside the frustum.
constexpr float kEyeA[3] = {0.f, 1.f, 3.f};
constexpr float kEyeB[3] = {3.f, 1.f, 0.f};

// Colour encoding of cameraPosition in the probe: byte(v) = 255*(v+4)/8.
inline int encByte(double v)
{
  return int(std::lround(255.0 * (v + 4.0) / 8.0));
}

// --- Probe shaders -----------------------------------------------------------
// Same vertex stage and "camera" uniform LAYOUT as GfxEnvRenderTargetSize.cpp
// (proven to name-match the preprocessor's "camera" auxiliary buffer and to
// byte-match CameraUBOData, Gfx/Graph/CameraMath.hpp:23-31; omitting
// VISIBILITY defaults the block to vertex+fragment). Only the fragment
// encoding differs: it identifies the ACTIVE camera by its world position.

constexpr const char* kProbeVS = R"__(void main()
{
    gl_Position = clipSpaceCorrMatrix * camera.viewProjection * vec4(position.xyz, 1.0);
}
)__";

constexpr const char* kProbeFS = R"__(/*{
  "DESCRIPTION": "P1-3 probe: draws scene geometry through camera.viewProjection and encodes the ACTIVE camera's world position into flat colour: R=(x+4)/8, G=(y+4)/8, B=(z+4)/8. Slot 0 of the packed camera UBO is the active camera, so the drawn colour identifies WHICH camera the ScenePreprocessor selected.",
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
        (camera.cameraPosition.x + 4.0) / 8.0,
        (camera.cameraPosition.y + 4.0) / 8.0,
        (camera.cameraPosition.z + 4.0) / 8.0,
        1.0);
}
)__";

bool writeFile(const QString& path, const char* text)
{
  QFile f{path};
  if(!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    return false;
  return f.write(text) >= 0;
}

// --- Real Threedim::Camera, ticked on the CPU --------------------------------
// The direct-tick pattern of GfxEnvRenderTargetSize.cpp's CPU cases: set the
// halp inputs, rebuild(), tick. Returns the camera's published scene_state
// (roots = one scene_node carrying {scene_transform, camera_component}, id =
// the camera's minted node id, active_camera_id = that id -- Camera.hpp:
// 103-183).
std::shared_ptr<ossia::scene_state>
tickCamera(Threedim::Camera& cam, const float (&eye)[3])
{
  cam.inputs.eye.value = {eye[0], eye[1], eye[2]};
  cam.inputs.target.value = {0.f, 0.f, 0.f};
  cam.inputs.fov.value = 60.f;
  cam.inputs.near_plane.value = 0.1f;
  cam.inputs.far_plane.value = 100.f;
  cam.rebuild();
  cam();
  return cam.m_state;
}

// The published two-camera state: the exact shape merge_scenes produces for
// [camA, camB] (roots concatenated in contributor order, one non-zero
// active_camera_id -- pinned by the CPU merge case below), with only the id
// field driven by the test. Roots order is FIXED across phases; a fresh
// scene_state per phase shares the same roots vector, so upstream caches
// invalidate on (pointer, version) while the camera payloads stay identical.
std::shared_ptr<ossia::scene_state> makeSwitchedState(
    std::shared_ptr<const std::vector<ossia::scene_node_ptr>> roots,
    ossia::scene_node_id active, int64_t version)
{
  auto st = std::make_shared<ossia::scene_state>();
  st->roots = std::move(roots);
  st->active_camera_id = active;
  st->version = version;
  st->dirty_index = version;
  return st;
}

// --- Crousti glue (cloned from GfxEnvRenderTargetSize.cpp) -------------------

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

// --- Registry snapshot (cloned from GfxMaterialTextureSwap.cpp) --------------
// Taken on the render thread by the publisher's renderer each frame and read
// from the (same-thread, synchronous offscreen fixture) test body between
// render() calls. Raw pointers are captured for IDENTITY comparison only,
// never dereferenced.
struct ChannelSnap
{
  bool taken = false;
  std::vector<QRhiTexture*> bucketArrays; // per bucket: array pointer
  std::vector<int> bucketLayers;          // per bucket: layer count
  std::vector<QRhiTexture*> dyn;          // dynamic slot -> texture
};

void takeSnap(score::gfx::RenderList& r, ChannelSnap& out)
{
  auto& reg = r.registry();
  const auto& ch = reg.textureChannel(
      score::gfx::GpuResourceRegistry::TextureChannel::BaseColor);
  ChannelSnap s;
  s.taken = true;
  s.bucketArrays.reserve(ch.buckets.size());
  s.bucketLayers.reserve(ch.buckets.size());
  for(const auto& b : ch.buckets)
  {
    s.bucketArrays.push_back(b.array);
    s.bucketLayers.push_back(b.layers);
  }
  s.dyn = ch.dynamicTextures;
  out = std::move(s);
}

// --- The publisher: a data-only score::gfx scene producer --------------------
// The GfxMaterialTextureSwap.cpp harness-node pattern: publishes its
// scene_state to the downstream ScenePreprocessor exactly as every real
// scene producer does -- NodeRenderer::process(port, scene_spec, edge.source)
// from runInitialPasses, every frame; the preprocessor short-circuits on
// state pointer + version. The test swaps pendingState between render()
// calls (single-threaded offscreen fixture).

struct CamPublisherNode final : score::gfx::ProcessNode
{
  std::shared_ptr<ossia::scene_state> pendingState;

  // Written by the renderer every frame, read by the test between frames.
  mutable ChannelSnap snap;

  CamPublisherNode()
  {
    output.push_back(
        new score::gfx::Port{this, {}, score::gfx::Types::Scene, {}});
  }
  ~CamPublisherNode() override = default;

  score::gfx::NodeRenderer* createRenderer(score::gfx::RenderList&) const
      noexcept override;
};

struct CamPublisherRenderer final : score::gfx::NodeRenderer
{
  CamPublisherNode& self;
  ossia::scene_spec m_scene;

  explicit CamPublisherRenderer(const CamPublisherNode& n)
      : NodeRenderer{n}
      , self{const_cast<CamPublisherNode&>(n)}
  {
  }

  void init(score::gfx::RenderList&, QRhiResourceUpdateBatch&) override
  {
    m_initialized = true;
  }

  void update(
      score::gfx::RenderList& r, QRhiResourceUpdateBatch&,
      score::gfx::Edge*) override
  {
    if(self.pendingState && m_scene.state != self.pendingState)
      m_scene.state = self.pendingState;

    // Registry snapshot AFTER the phase logic: reflects the preprocessor's
    // state as of the END of the previous frame (this producer updates
    // before the preprocessor in topological order), settled by the time
    // the test reads it -- each phase renders several frames.
    takeSnap(r, self.snap);
  }

  void runInitialPasses(
      score::gfx::RenderList& renderer, QRhiCommandBuffer&,
      QRhiResourceUpdateBatch*&, score::gfx::Edge& edge) override
  {
    if(!m_scene.state)
      return;
    auto* sink = edge.sink;
    if(!sink || !sink->node)
      return;
    auto rn_it = sink->node->renderedNodes.find(&renderer);
    if(rn_it == sink->node->renderedNodes.end())
      return;
    auto it
        = std::find(sink->node->input.begin(), sink->node->input.end(), sink);
    if(it == sink->node->input.end())
      return;
    const int port_idx = int(it - sink->node->input.begin());
    rn_it->second->process(port_idx, m_scene, edge.source);
  }

  void runRenderPass(
      score::gfx::RenderList&, QRhiCommandBuffer&, score::gfx::Edge&) override
  {
  }
  void removeOutputPass(score::gfx::RenderList&, score::gfx::Edge&) override
  {
  }

  void release(score::gfx::RenderList&) override
  {
    m_scene = {};
    m_initialized = false;
  }
};

score::gfx::NodeRenderer*
CamPublisherNode::createRenderer(score::gfx::RenderList&) const noexcept
{
  return new CamPublisherRenderer{*this};
}

// --- Pixel analysis (cloned from GfxEnvRenderTargetSize.cpp) -----------------

struct Phase
{
  bool valid = false;
  int width = 0, height = 0; // readback extent
  int litCount = 0;
  int r = -1, g = -1, b = -1; // modal drawn colour
};

Phase analyze(const ReadbackImage& img)
{
  Phase p;
  p.width = img.width;
  p.height = img.height;
  if(img.bytes.isEmpty() || img.width <= 0 || img.height <= 0)
    return p;
  p.valid = true;

  // Both encodings carry G = (1+4)/8*255 ~ 159 on every drawn pixel; the
  // clear colour is black. Threshold the green channel well below that.
  std::map<uint32_t, int> hist;
  for(int y = 0; y < img.height; ++y)
  {
    for(int x = 0; x < img.width; ++x)
    {
      const auto px = img.at(x, y);
      if(int(px[1]) >= 64)
      {
        ++p.litCount;
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
  }
  return p;
}

// --- The render scenario -----------------------------------------------------

struct Outcome
{
  bool skipped = false;
  std::string skip_reason;
  std::string error;
  std::string backend;

  bool idsDistinct = false;

  Phase pA; // active = camA
  Phase pB; // active = camB
  Phase pC; // active back to camA
  bool diffAB = false;
  bool exactReturn = false; // frame C bytes == frame A bytes
  int maxDiffAC = -1;       // measured for the record when not exact

  ChannelSnap snapA, snapB, snapC;
};

Outcome run_camera_switch(score::gfx::GraphicsApi api)
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
    const QString vsPath = shaderDir.filePath("cam-switch-probe.vs");
    const QString fsPath = shaderDir.filePath("cam-switch-probe.fs");
    if(!writeFile(vsPath, kProbeVS) || !writeFile(fsPath, kProbeFS))
    {
      out.error = "cannot write the probe shaders";
      return;
    }

    // Two REAL Camera processes, ticked on the CPU. Their states (and the
    // scene nodes / transforms / camera components inside) are what the
    // publisher republishes -- all camera data in the frame is
    // Camera-authored.
    Threedim::Camera camA, camB;
    const auto stateA = tickCamera(camA, kEyeA);
    const auto stateB = tickCamera(camB, kEyeB);
    if(!stateA || !stateA->roots || !stateB || !stateB->roots)
    {
      out.error = "Camera did not publish a scene_state with roots";
      return;
    }
    out.idsDistinct = camA.m_id.value != 0 && camB.m_id.value != 0
                      && camA.m_id.value != camB.m_id.value;

    // FIXED roots order [camA, camB] shared by every phase's state.
    auto combined = std::make_shared<std::vector<ossia::scene_node_ptr>>();
    for(const auto& r : *stateA->roots)
      combined->push_back(r);
    for(const auto& r : *stateB->roots)
      combined->push_back(r);
    const std::shared_ptr<const std::vector<ossia::scene_node_ptr>> roots
        = combined;

    HalpProcesses procs;
    GfxPipeline p;

    const int cube = p.addNode(procs.make<Threedim::Cube>(doc->context()));

    auto pub_uptr = std::make_unique<CamPublisherNode>();
    auto* pub = pub_uptr.get();
    pub->pendingState = makeSwitchedState(roots, camA.m_id, /*version=*/1);
    const int pubIdx = p.addNode(std::move(pub_uptr));

    const int flat
        = p.addNode(std::make_unique<score::gfx::ScenePreprocessorNode>());
    const int raster = p.addRaster(vsPath, fsPath);
    if(cube < 0 || pubIdx < 0 || flat < 0 || raster < 0)
    {
      out.error = "chain build failed: " + p.error();
      return;
    }

    auto* cubeOut = p.nodeSceneOut(cube, 0);
    auto* pubOut = p.nodeSceneOut(pubIdx, 0);
    auto* flatIn = p.nodeSceneIn(flat, 0);
    auto* flatOut = p.nodeGeometryOut(flat, 0);
    if(!cubeOut || !pubOut || !flatIn || !flatOut)
    {
      out.error = "scene ports missing on the chain";
      return;
    }
    p.wire(cubeOut, flatIn);
    p.wire(pubOut, flatIn);
    p.wire(flatOut, p.geometryIn(raster, 0));
    const int sink = p.addSink({kSize, kSize});
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));

    if(!p.create(api))
    {
      out.backend = p.backend();
      out.skipped = p.skipped();
      out.skip_reason = p.skipReason();
      out.error = p.error();
      return;
    }
    out.backend = p.backend();

    // ---- Phase A: active_camera_id = camA. Cube's state carries no
    // active_camera_id (grep: Primitive.cpp never sets it), so the merged
    // scene's id is the publisher's regardless of the per-(port,source)
    // iteration order (first NON-ZERO wins, geometry_port.cpp:467-468).
    p.render(6);
    const auto imgA = p.readback(sink);
    out.pA = analyze(imgA);
    out.snapA = pub->snap;

    // ---- Phase B: the SWITCH. Same roots pointer, same cameras, same
    // order; only active_camera_id moves to camB (version bumped so the
    // merge memo re-keys). flattenScene resolves index 1
    // (SceneGPUState.cpp:940-942), packAndUploadCameras packs camB at slot 0
    // (ScenePreprocessorNode.cpp:3685-3691), the probe turns camB-coloured.
    pub->pendingState = makeSwitchedState(roots, camB.m_id, /*version=*/2);
    p.render(6);
    const auto imgB = p.readback(sink);
    out.pB = analyze(imgB);
    out.snapB = pub->snap;
    out.diffAB = imgA.bytes != imgB.bytes;

    // ---- Phase C: switch BACK to camA. Must reproduce phase A exactly.
    pub->pendingState = makeSwitchedState(roots, camA.m_id, /*version=*/3);
    p.render(6);
    const auto imgC = p.readback(sink);
    out.pC = analyze(imgC);
    out.snapC = pub->snap;
    out.exactReturn = imgA.bytes == imgC.bytes && !imgA.bytes.isEmpty();
    if(imgA.bytes.size() == imgC.bytes.size() && !imgA.bytes.isEmpty())
    {
      int md = 0;
      for(qsizetype i = 0; i < imgA.bytes.size(); ++i)
        md = std::max(
            md, std::abs(
                    int(uint8_t(imgA.bytes[i])) - int(uint8_t(imgC.bytes[i]))));
      out.maxDiffAC = md;
    }
  });
  return out;
}

} // namespace

// =============================================================================
// CPU-only halves: run everywhere, including GPU-less CI.
// =============================================================================

TEST_CASE(
    "two Camera processes mint distinct non-zero node ids and each stamps "
    "itself as the active camera",
    "[gfx][threedim][camera][p1-3]")
{
  Threedim::Camera a, b;
  const auto sa = tickCamera(a, kEyeA);
  const auto sb = tickCamera(b, kEyeB);
  REQUIRE(sa);
  REQUIRE(sb);

  // Camera.hpp:110 -- non-zero id minted on first rebuild; :179 -- each
  // camera nominates itself on every rebuild.
  CHECK(a.m_id.value != 0);
  CHECK(b.m_id.value != 0);
  CHECK(a.m_id.value != b.m_id.value);
  CHECK(sa->active_camera_id.value == a.m_id.value);
  CHECK(sb->active_camera_id.value == b.m_id.value);
  REQUIRE(sa->roots);
  REQUIRE(sb->roots);
  REQUIRE(sa->roots->size() == 1);
  // Camera.hpp:171 -- the emitted scene_node carries the id, which is what
  // flattenScene attributes the camera payload to.
  CHECK((*sa->roots)[0]->id.value == a.m_id.value);
}

TEST_CASE(
    "merge_scenes resolves active_camera_id to the first non-zero "
    "contributor, so the publish ORDER of two Cameras picks the camera",
    "[gfx][threedim][camera][p1-3]")
{
  Threedim::Camera a, b;
  const auto sa = tickCamera(a, kEyeA);
  const auto sb = tickCamera(b, kEyeB);

  // geometry_port.cpp:467-468 (first non-zero wins), :556 (stamped on the
  // merged state). Both cameras stay present either way.
  {
    std::array<ossia::scene_spec, 2> ab;
    ab[0].state = sa;
    ab[1].state = sb;
    const auto merged
        = ossia::merge_scenes(std::span<const ossia::scene_spec>{ab});
    REQUIRE(merged.state);
    CHECK(merged.state->active_camera_id.value == a.m_id.value);
    REQUIRE(merged.state->roots);
    CHECK(merged.state->roots->size() == 2);
  }
  {
    std::array<ossia::scene_spec, 2> ba;
    ba[0].state = sb;
    ba[1].state = sa;
    const auto merged
        = ossia::merge_scenes(std::span<const ossia::scene_spec>{ba});
    REQUIRE(merged.state);
    CHECK(merged.state->active_camera_id.value == b.m_id.value);
    REQUIRE(merged.state->roots);
    CHECK(merged.state->roots->size() == 2);
  }
}

TEST_CASE(
    "CameraSwitch in Select mode forwards the picked camera state, and "
    "switching back restores the exact state pointer",
    "[gfx][threedim][camera][p1-3]")
{
  // The halp-level contract of the process the spec guessed at
  // (CameraSwitch.hpp:320-339: Select forwards the picked upstream
  // scene_spec verbatim, per-tick, driven by the `index` control :92-93).
  // Pinned HERE and not in the render leg because the score::gfx transport
  // hands every scene input field of a halp node the SAME merged scene
  // (Crousti/GpuUtils.hpp:1776-1785; NodeRenderer.cpp:481-527 merges across
  // all (port, source) entries), so a GfxNode<CameraSwitch> wired to two
  // Camera GfxNodes cannot see them as separate inputs in this harness.
  Threedim::Camera a, b;
  const auto sa = tickCamera(a, kEyeA);
  const auto sb = tickCamera(b, kEyeB);

  Threedim::CameraSwitch sw;
  sw.inputs.mode.value = Threedim::CameraSwitch::ins::Select;
  sw.inputs.cam0.scene.state = sa;
  sw.inputs.cam1.scene.state = sb;

  sw.inputs.index.value = 0;
  sw();
  CHECK(sw.outputs.scene_out.scene.state == sa);
  REQUIRE(sw.outputs.scene_out.scene.state);
  CHECK(sw.outputs.scene_out.scene.state->active_camera_id.value == a.m_id.value);

  sw.inputs.index.value = 1;
  sw();
  CHECK(sw.outputs.scene_out.scene.state == sb);
  CHECK(sw.outputs.scene_out.scene.state->active_camera_id.value == b.m_id.value);

  // The exact-pointer return is what makes the render-level byte-for-byte
  // return possible: downstream memo caches re-key onto the same state.
  sw.inputs.index.value = 0;
  sw();
  CHECK(sw.outputs.scene_out.scene.state == sa);
}

TEST_CASE(
    "flattenScene resolves activeCameraIndex from active_camera_id and "
    "falls back to camera 0 when the id is zero or unmatched",
    "[gfx][threedim][camera][p1-3]")
{
  // SceneGPUState.cpp:930-947: default index 0 (:935), id-match loop
  // (:936-945). Cameras are collected with the enclosing node's id
  // (:543-551). flattenScene is SCORE_PLUGIN_GFX_EXPORT
  // (SceneGPUState.hpp:597-601).
  Threedim::Camera a, b;
  const auto sa = tickCamera(a, kEyeA);
  const auto sb = tickCamera(b, kEyeB);

  auto combined = std::make_shared<std::vector<ossia::scene_node_ptr>>();
  for(const auto& r : *sa->roots)
    combined->push_back(r);
  for(const auto& r : *sb->roots)
    combined->push_back(r);
  const std::shared_ptr<const std::vector<ossia::scene_node_ptr>> roots
      = combined;

  auto check = [&](ossia::scene_node_id active, int expectedIndex) {
    ossia::scene_spec spec;
    spec.state = makeSwitchedState(roots, active, 1);
    score::gfx::FlatScene fs;
    score::gfx::flattenScene(spec, fs, 1.f);
    REQUIRE(fs.cameras.size() == 2);
    CHECK(fs.cameras[0].node_id.value == a.m_id.value);
    CHECK(fs.cameras[1].node_id.value == b.m_id.value);
    CHECK(fs.activeCameraIndex == expectedIndex);
  };

  check(a.m_id, 0);
  check(b.m_id, 1);                         // THE switch, at flatten level
  check(ossia::scene_node_id{}, 0);         // unset id -> first camera
  check(ossia::scene_node_id{0xDEADBEEFu}, 0); // unknown id -> fallback 0
}

// =============================================================================
// The render half: real cube + real Camera-authored scenes, real merge,
// real flatten + camera pack, real pixels, on every backend the box offers.
// =============================================================================

TEST_CASE(
    "P1-3: switching the active camera mid-render moves the image, and "
    "switching back reproduces the first frame byte-for-byte",
    "[gfx][threedim][camera][scene][p1-3]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const auto r = run_camera_switch(api);
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);

  INFO("backend=" << r.backend << " error=" << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.idsDistinct);

  REQUIRE(r.pA.valid);
  REQUIRE(r.pB.valid);
  REQUIRE(r.pC.valid);

  INFO(
      "phaseA rgb=(" << r.pA.r << "," << r.pA.g << "," << r.pA.b
                     << ") lit=" << r.pA.litCount);
  INFO(
      "phaseB rgb=(" << r.pB.r << "," << r.pB.g << "," << r.pB.b
                     << ") lit=" << r.pB.litCount);
  INFO(
      "phaseC rgb=(" << r.pC.r << "," << r.pC.g << "," << r.pC.b
                     << ") lit=" << r.pC.litCount);
  INFO("diffAB=" << r.diffAB << " exactReturn=" << r.exactReturn
                 << " maxDiffAC=" << r.maxDiffAC);

  // The cube must actually be drawn in every phase -- an unresolved "camera"
  // aux binding would zero viewProjection and draw nothing, which fails here
  // attributably rather than passing vacuously.
  REQUIRE(r.pA.litCount > 40);
  REQUIRE(r.pB.litCount > 40);
  REQUIRE(r.pC.litCount > 40);

  // Readback extent is the sink in every phase.
  CHECK(r.pA.width == kSize);
  CHECK(r.pA.height == kSize);
  CHECK(r.pB.width == kSize);
  CHECK(r.pB.height == kSize);
  CHECK(r.pC.width == kSize);
  CHECK(r.pC.height == kSize);

  const int tol = 4; // 0.5 LSB encoding rounding + a couple LSB per backend

  // ---- Phase A: the active camera is camA -- the drawn colour encodes
  // camA's eye (0,1,3).
  CHECK(std::abs(r.pA.r - encByte(kEyeA[0])) <= tol);
  CHECK(std::abs(r.pA.g - encByte(kEyeA[1])) <= tol);
  CHECK(std::abs(r.pA.b - encByte(kEyeA[2])) <= tol);

  // ---- Phase B: THE SWITCH moved the image, and to the RIGHT camera: the
  // colour now encodes camB's eye (3,1,0). The R/B channels move by ~95
  // against a 4-LSB tolerance -- the "beyond a stated margin" of the spec.
  CHECK(std::abs(r.pB.r - encByte(kEyeB[0])) <= tol);
  CHECK(std::abs(r.pB.g - encByte(kEyeB[1])) <= tol);
  CHECK(std::abs(r.pB.b - encByte(kEyeB[2])) <= tol);
  CHECK(r.diffAB);

  // ---- Phase C: switching BACK reproduces the first frame exactly.
  // Expected byte-for-byte (deterministic pack from identical inputs;
  // camera_prev differs across the return but the probe never samples it).
  // maxDiffAC is INFO'd above so a failing backend leaves its measured
  // margin in the log.
  CHECK(r.exactReturn);
  CHECK(std::abs(r.pC.r - encByte(kEyeA[0])) <= tol);
  CHECK(std::abs(r.pC.g - encByte(kEyeA[1])) <= tol);
  CHECK(std::abs(r.pC.b - encByte(kEyeA[2])) <= tol);

  // ---- NOT A REBUILD: the switch flowed through the per-frame camera
  // flatten + diff-upload (ScenePreprocessorNode.cpp:3766-3781), not through
  // any texture-channel reallocation -- every BaseColor bucket's array
  // pointer, layer count and dynamic slot is identical across all three
  // phases (the GfxMaterialTextureSwap.cpp snapshot technique; its scope,
  // and why the byte-exact return covers the rest, is in the file header).
  REQUIRE(r.snapA.taken);
  REQUIRE(r.snapB.taken);
  REQUIRE(r.snapC.taken);
  CHECK(r.snapB.bucketArrays == r.snapA.bucketArrays);
  CHECK(r.snapB.bucketLayers == r.snapA.bucketLayers);
  CHECK(r.snapB.dyn == r.snapA.dyn);
  CHECK(r.snapC.bucketArrays == r.snapA.bucketArrays);
  CHECK(r.snapC.bucketLayers == r.snapA.bucketLayers);
  CHECK(r.snapC.dyn == r.snapA.dyn);
}
