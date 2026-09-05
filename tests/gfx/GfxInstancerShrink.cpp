// =============================================================================
// P0-3 -- THE INSTANCER CLAMPS A SHRINKING POINTS BUFFER WITHOUT OVER-READING:
// the LIVE-RENDER half.
//
// (The clamp arithmetic alone is already pinned by
// tests/threedim/InstancerClamp.cpp and the all-buffers handle-change
// fingerprint by tests/threedim/InstancerStaleBuffer.cpp. This file adds what
// neither has: the REAL Threedim::Instancer feeding the REAL
// score::gfx::ScenePreprocessorNode feeding a REAL instanced raster draw on a
// real backend, with a Points transforms buffer that shrinks 100 -> 10
// mid-session, byte_size shrinking AND the handle changing, exactly as the
// spec's scenario states.)
//
// Intended registration (tests/gfx/CMakeLists.txt), mirroring the
// test_gfx_crousti_cpu_nodes block -- Instancer.cpp is hidden-visibility
// inside score_plugin_threedim, so it is compiled into the test target:
//
//   if(TARGET score_plugin_threedim)
//     score_plugin_hidden_sources(_instancer_shrink_hidden
//         "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-threedim/Threedim/Instancer.cpp")
//     score_add_test(test_gfx_instancer_shrink
//       SOURCES GfxInstancerShrink.cpp ${_instancer_shrink_hidden}
//       GUI
//       PLUGINS score_plugin_gfx score_plugin_scenario score_lib_process
//       LIBS test_gfx_engine_glue)
//     target_compile_definitions(test_gfx_instancer_shrink PRIVATE
//       GFX_TEST_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/corpus")
//     target_include_directories(test_gfx_instancer_shrink SYSTEM PRIVATE
//       "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-threedim"
//       "${SCORE_ROOT_BINARY_DIR}/src/plugins/score-plugin-threedim"
//       "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-gfx"
//       "${SCORE_ROOT_BINARY_DIR}/src/plugins/score-plugin-gfx"
//       $<TARGET_PROPERTY:score_plugin_threedim,INCLUDE_DIRECTORIES>
//       $<TARGET_PROPERTY:score_plugin_gfx,INCLUDE_DIRECTORIES>)
//   endif()
//
// WHAT LEVEL IS DRIVEN, AND WHY. Everything from the Instancer's tick down to
// the readback is the shipped engine:
//   * Threedim::Instancer::operator()() / rebuild() -- the real fingerprint
//     rebuild trigger (Instancer.cpp:525-533) and the real capacity clamp
//     (Instancer.cpp:254-324: stride selection 262-275, capacityFor 279-309,
//     clamp 311-323, publish at 363-364).
//   * score::gfx::ScenePreprocessorNode -- the real instance-group draw
//     emission (ScenePreprocessorNode.cpp:2159 zero-count skip, 2206-2226
//     srcTranslations + per-format stride 16/40/64 and the mat4
//     column-3-at-offset-48 rule, 2239/2245 emitDraw with
//     inst.instance_count, 2269 rec.count, 2464-2499 queueInstanceCopy of
//     rec.count regions, 4683+ issuePendingGpuCopies' strided
//     copyBufferRegions at src_offset + v*64).
//   * A real RAW_RASTER_PIPELINE consumer (corpus/syn-instance-index-color)
//     binding the preprocessor's per-instance `translation` attribute
//     (published at ScenePreprocessorNode.cpp:2656, float3, per_instance),
//     drawn on every real backend the box offers, read back as pixels.
//
// The ONE thing the test supplies instead of an upstream producer chain is the
// pair of QRhiBuffers holding the per-point mat4 transforms (plus a CPU quad
// prototype). This is deliberate, not a shortcut:
//   * The only in-tree live path that could produce a shrinking transforms
//     buffer is Crousti's cpu_buffer_output upload
//     (score-plugin-avnd/Crousti/GpuUtils.hpp:728-733, recreateOutputBuffer):
//     on a size change it calls destroy()/setSize()/create() on the SAME
//     QRhiBuffer object -- the handle pointer never changes. The P0-3
//     scenario is "byte_size shrinks, handle changes", which therefore cannot
//     be produced by any in-tree producer without product changes; a fresh
//     QRhiBuffer must come from the test.
//   * ENGINE GAP, documented not asserted: because the live resize path keeps
//     the handle, and Instancer::operator()()'s change detection
//     (Instancer.cpp:525-533) keys on handles / vertices / dirty_mesh only --
//     never byte_size -- a live byte_size-only shrink through
//     recreateOutputBuffer would NOT rebuild, leaving instance_count at the
//     old value against the smaller buffer. Closing that needs a product-side
//     cache key (byte_size in the fingerprint); this test pins the
//     handle-change contract the spec names.
//
// SCENARIO. A Points cloud of 100 points whose buffers[1] carries a
// transform_matrix attribute (so routing forces Mat4, stride 64, and
// effective_count starts from mesh.vertices == 100):
//   Phase 1: buffers[1] = bufA, 100 * 64 = 6400 bytes. Capacity 6400/64 = 100
//            -> instance_count 100, 100 quads drawn across the full frame.
//   Phase 2: buffers[1] = bufB, a NEW QRhiBuffer of 10 * 64 = 640 bytes;
//            vertices stays 100 (that is the point: the clamp, not the vertex
//            count, must bound the draw). The all-buffers fingerprint changes
//            -> rebuild -> instance_count EXACTLY 10, and exactly 10 strips
//            drawn at the 10 authored translations.
//
// NO-OVER-READ EVIDENCE (stated honestly). The per-instance GPU copy is
// GPU->GPU (copyBufferRegions), so heap ASan cannot see it directly; the
// guard here is closed-form instead:
//   * bufB is sized EXACTLY 640 bytes. With the clamp, the last copy region
//     reads bytes [48 + 9*64, 48 + 9*64 + 12) = [624, 636) -- inside. Any
//     regression that publishes counts > 10 makes region v=10 read at byte
//     688 > 640: on Vulkan the validation layer flags the OOB copy, and the
//     asan CI job's run fails on the validation abort; the strip-count oracle
//     below goes red on every backend regardless.
//   * bufA is destroy()ed two frames after the swap (GPU-idle by then in this
//     synchronous offscreen fixture), so any stale record still copying from
//     the old handle fails hard and deterministically instead of reading
//     freed VRAM silently.
//
// PIXEL ORACLE. syn-instance-index-color.vs places each instance at
// position.xy + translation.xy with MODEL_MATRIX identity (its own header
// documents this), and the scene path publishes our mat4 column-3 xyz as that
// per-instance translation. The prototype quad is 2 px wide and full-height.
//   Phase 1: 100 strips starting every 0.02 NDC -> every column lit.
//   Phase 2: 10 strips starting every 0.2 NDC (6.4 px apart, 2 px wide) ->
//            EXACTLY 10 disjoint lit column runs, nothing lit right of
//            column 60. A stale count of 100 (or garbage translations from an
//            over-read) breaks the run count / extent immediately.
//
// NEGATIVE CONTROL (product-side, one line, for the orchestrator):
//   src/plugins/score-plugin-threedim/Threedim/Instancer.cpp:263-264 --
//   change `if(routing.has_matrix) transform_stride = 64;` to
//   `... transform_stride = 16;`. The clamp then computes 640/16 = 40 for
//   phase 2: the CPU assertion (instance_count == 10) goes red, the drawn
//   strip count is wrong, and the copy loop reads up to byte 48 + 39*64 + 12
//   = 2556 of the 640-byte buffer -- the over-read the Vulkan validation
//   layer flags. (The spec's suggested control -- forcing 64 for the
//   Translation format at Instancer.cpp:272 -- points the arithmetic the
//   other way: a larger stride yields a SMALLER capacity, i.e. an
//   under-count, and cannot fire an over-read. The control above is the
//   over-counting direction for the mat4 path this test drives.)
//
//   DISPLAY=:0 SCORE_TEST_API=opengl ctest -R gfx_instancer_shrink
//   DISPLAY=:0 SCORE_TEST_API=vulkan ctest -R gfx_instancer_shrink
// The verdict is pixels: unavailable backends SKIP, never fall back to Null.
// =============================================================================

#include <score_test/Gfx.hpp>

#include <Threedim/Instancer.hpp>

#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/ScenePreprocessorNode.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

using namespace score::test::gfx;

namespace
{
QString corpus(const char* f)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR "/") + QString::fromUtf8(f);
}

constexpr int kSize = 64;          // frame is 64 x 64
constexpr int kBigCount = 100;     // phase-1 point / instance count
constexpr int kSmallCount = 10;    // phase-2 buffer capacity in instances
constexpr float kQuadW = 0.0625f;  // prototype quad width: 2 px in NDC

// --- Helpers shared with the CPU-side Instancer tests -----------------------

const ossia::instance_component*
findInstance(const std::shared_ptr<ossia::scene_state>& st)
{
  if(!st || !st->roots || st->roots->empty())
    return nullptr;
  const auto& n0 = (*st->roots)[0];
  if(!n0 || !n0->children)
    return nullptr;
  for(const auto& p : *n0->children)
    if(const auto* inst = ossia::get_if<ossia::instance_component_ptr>(&p))
      if(*inst)
        return inst->get();
  return nullptr;
}

void* handleOf(const ossia::buffer_resource_ptr& r)
{
  if(!r)
    return nullptr;
  const auto* gpu = ossia::get_if<ossia::gpu_buffer_handle>(&r->resource);
  return gpu ? gpu->native_handle : nullptr;
}

// Column-major identity mat4 with translation (tx, ty, 0) in column 3
// (floats 12..14 == byte offset 48 -- the offset
// ScenePreprocessorNode.cpp:2216-2219 reads for transform_format::mat4).
void writeMat4(float* out, float tx, float ty)
{
  std::memset(out, 0, 16 * sizeof(float));
  out[0] = out[5] = out[10] = out[15] = 1.f;
  out[12] = tx;
  out[13] = ty;
  out[14] = 0.f;
}

// Prototype: a 2 px wide, full-height quad with CPU positions + uint32
// indices -- the same CPU-backed mesh_primitive shape glTF/OBJ loaders
// publish, which the preprocessor's slab path uploads
// (ScenePreprocessorNode.cpp:1889 extractCpuAttribute<12>(position)).
std::shared_ptr<ossia::scene_state> makePrototypeScene()
{
  auto positions = std::make_shared<std::vector<float>>(std::vector<float>{
      0.f,    -1.f, 0.f, //
      kQuadW, -1.f, 0.f, //
      kQuadW, 1.f,  0.f, //
      0.f,    -1.f, 0.f, //
      kQuadW, 1.f,  0.f, //
      0.f,    1.f,  0.f, //
  });
  auto indices = std::make_shared<std::vector<uint32_t>>(
      std::vector<uint32_t>{0, 1, 2, 3, 4, 5});

  auto posRes = std::make_shared<ossia::buffer_resource>();
  {
    ossia::buffer_data bd;
    bd.data = std::shared_ptr<const void>(positions, positions->data());
    bd.byte_size = int64_t(positions->size() * sizeof(float));
    bd.usage_hint = ossia::buffer_data::usage::vertex_buffer;
    posRes->resource = bd;
    posRes->dirty_index = 1;
  }
  auto idxRes = std::make_shared<ossia::buffer_resource>();
  {
    ossia::buffer_data bd;
    bd.data = std::shared_ptr<const void>(indices, indices->data());
    bd.byte_size = int64_t(indices->size() * sizeof(uint32_t));
    bd.usage_hint = ossia::buffer_data::usage::index_buffer;
    idxRes->resource = bd;
    idxRes->dirty_index = 1;
  }

  ossia::mesh_primitive prim;
  prim.vertex_buffers.push_back(posRes);
  prim.index_buffer = idxRes;
  {
    ossia::vertex_attribute a;
    a.semantic = ossia::attribute_semantic::position;
    a.format = ossia::vertex_format::float3;
    a.buffer_index = 0;
    a.byte_offset = 0;
    a.byte_stride = 12;
    a.rate = ossia::vertex_attribute::input_rate::per_vertex;
    prim.attributes.push_back(a);
  }
  prim.topology = ossia::primitive_topology::triangles;
  prim.index_type = ossia::index_format::uint32;
  prim.vertex_count = 6;
  prim.index_count = 6;
  prim.bounds = ossia::compute_aabb_from_positions(positions->data(), 6);
  prim.stable_id = 0x51AB1E01u;

  auto mesh = std::make_shared<ossia::mesh_component>();
  mesh->primitives.push_back(std::move(prim));
  mesh->bounds = mesh->primitives[0].bounds;
  mesh->dirty_index = 1;

  auto children = std::make_shared<std::vector<ossia::scene_payload>>();
  children->push_back(ossia::mesh_component_ptr(std::move(mesh)));
  auto root = std::make_shared<ossia::scene_node>();
  root->children = std::move(children);
  auto roots = std::make_shared<std::vector<ossia::scene_node_ptr>>();
  roots->push_back(std::move(root));

  auto st = std::make_shared<ossia::scene_state>();
  st->roots = std::move(roots);
  st->version = 1;
  st->dirty_index = 1;
  return st;
}

// --- The harness node --------------------------------------------------------
//
// A data-only score::gfx producer node (modelled exactly on
// RenderedMergeGeometriesNode, MergeGeometriesNode.cpp) whose renderer owns
// the transform QRhiBuffers, ticks the REAL Threedim::Instancer against them,
// and publishes the resulting scene to its output edges the same way every
// scene producer does: NodeRenderer::process(port, scene_spec, edge.source)
// (the exact publish call in Crousti's scene_outputs_storage,
// GpuUtils.hpp:1800+, and in RenderedMergeGeometriesNode::runInitialPasses).

struct InstancerShrinkNode final : score::gfx::ProcessNode
{
  // The REAL engine object under test, ticked by the renderer.
  mutable Threedim::Instancer instancer;
  std::shared_ptr<ossia::scene_state> protoScene = makePrototypeScene();

  // Test-driven phase: 1 = 100-instance buffer, 2 = shrunk 10-instance
  // buffer with a brand-new handle. Flip between render() calls.
  std::atomic<int> requestedPhase{1};
  std::atomic<int> appliedPhase{0};

  // Exposed for the CPU-side assertions (same thread as render()).
  mutable QRhiBuffer* bufA{};
  mutable QRhiBuffer* bufB{};

  InstancerShrinkNode()
  {
    output.push_back(
        new score::gfx::Port{this, {}, score::gfx::Types::Scene, {}});
  }
  ~InstancerShrinkNode() override = default;

  score::gfx::NodeRenderer* createRenderer(score::gfx::RenderList&) const
      noexcept override;
};

struct InstancerShrinkRenderer final : score::gfx::NodeRenderer
{
  InstancerShrinkNode& self;
  ossia::scene_spec m_scene;

  QRhiBuffer* m_posBuf{};
  QRhiBuffer* m_retire{};
  int m_retireCountdown{0};

  explicit InstancerShrinkRenderer(const InstancerShrinkNode& n)
      : NodeRenderer{n}
      , self{const_cast<InstancerShrinkNode&>(n)}
  {
  }

  void init(score::gfx::RenderList& r, QRhiResourceUpdateBatch& res) override
  {
    auto* rhi = r.state.rhi;

    // Points primary buffer (positions). Never consumed downstream -- the
    // prototype supplies the drawn vertices -- but the Instancer requires a
    // non-null handle on buffers[0] to detect a wired Points input, and it
    // participates in the all-buffers fingerprint, so make it real and keep
    // it byte-stable across both phases (the P0-3 scenario shrinks ONLY the
    // secondary transforms buffer).
    m_posBuf = rhi->newBuffer(
        QRhiBuffer::Static,
        QRhiBuffer::UsageFlags(
            score::gfx::compatibleBufferUsage(
                *rhi, QRhiBuffer::VertexBuffer | QRhiBuffer::StorageBuffer)),
        kBigCount * 12);
    m_posBuf->setName("InstancerShrinkTest::points_positions");
    m_posBuf->create();
    {
      std::vector<float> zeros(kBigCount * 3, 0.f);
      res.uploadStaticBuffer(
          m_posBuf, 0, quint32(zeros.size() * sizeof(float)), zeros.data());
    }

    // Static Instancer inputs: prototype scene + the Points cloud shape.
    // Mirrors tests/threedim/InstancerStaleBuffer.cpp exactly: buffers[0] is
    // the primary positions buffer, buffers[1] the transform_matrix
    // attribute buffer whose handle/byte_size the phases swap.
    self.instancer.inputs.scene_in.scene.state = self.protoScene;

    auto& mesh = self.instancer.inputs.points.mesh;
    mesh.buffers.resize(2);
    mesh.buffers[0].handle = m_posBuf;
    mesh.buffers[0].byte_size = kBigCount * 12;

    mesh.input.resize(2);
    mesh.input[0].buffer = 0;
    mesh.input[0].byte_offset = 0;
    mesh.input[1].buffer = 1;
    mesh.input[1].byte_offset = 0;

    mesh.attributes.resize(2);
    mesh.attributes[0].binding = 0;
    mesh.attributes[0].semantic = halp::attribute_semantic::position;
    mesh.attributes[0].byte_offset = 0;
    mesh.attributes[1].binding = 1;
    mesh.attributes[1].semantic = halp::attribute_semantic::transform_matrix;
    mesh.attributes[1].byte_offset = 0;

    // vertices stays 100 in BOTH phases: the shrink must be bounded by the
    // buffer capacity clamp (Instancer.cpp:311-323), not by a vertex-count
    // change.
    mesh.vertices = kBigCount;
    self.instancer.inputs.points.dirty_mesh = false;

    m_initialized = true;
  }

  void applyPhase(
      int phase, score::gfx::RenderList& r, QRhiResourceUpdateBatch& res)
  {
    auto* rhi = r.state.rhi;
    auto& mesh = self.instancer.inputs.points.mesh;

    if(phase == 1)
    {
      // 100 mat4s, translations tiling the full NDC width every 0.02.
      self.bufA = rhi->newBuffer(
          QRhiBuffer::Static,
          QRhiBuffer::UsageFlags(
              score::gfx::compatibleBufferUsage(
                *rhi, QRhiBuffer::VertexBuffer | QRhiBuffer::StorageBuffer)),
          kBigCount * 64);
      self.bufA->setName("InstancerShrinkTest::transforms_100");
      self.bufA->create();
      std::vector<float> mats(kBigCount * 16);
      for(int i = 0; i < kBigCount; ++i)
        writeMat4(mats.data() + i * 16, -1.f + 0.02f * float(i), 0.f);
      res.uploadStaticBuffer(
          self.bufA, 0, quint32(mats.size() * sizeof(float)), mats.data());

      mesh.buffers[1].handle = self.bufA;
      mesh.buffers[1].byte_size = kBigCount * 64;
    }
    else
    {
      // The shrink: a brand-NEW QRhiBuffer (fresh handle -- pointsBuffer-
      // fingerprint changes, Instancer.cpp:525-533 fires rebuild) sized for
      // EXACTLY 10 instances: 640 bytes, so the clamp's last mat4 column-3
      // read ends at byte 636 and one extra instance would read past the
      // end. Translations every 0.2 NDC: 10 disjoint 2 px strips.
      self.bufB = rhi->newBuffer(
          QRhiBuffer::Static,
          QRhiBuffer::UsageFlags(
              score::gfx::compatibleBufferUsage(
                *rhi, QRhiBuffer::VertexBuffer | QRhiBuffer::StorageBuffer)),
          kSmallCount * 64);
      self.bufB->setName("InstancerShrinkTest::transforms_10");
      self.bufB->create();
      std::vector<float> mats(kSmallCount * 16);
      for(int i = 0; i < kSmallCount; ++i)
        writeMat4(mats.data() + i * 16, -1.f + 0.2f * float(i), 0.f);
      res.uploadStaticBuffer(
          self.bufB, 0, quint32(mats.size() * sizeof(float)), mats.data());

      mesh.buffers[1].handle = self.bufB;
      mesh.buffers[1].byte_size = kSmallCount * 64;

      // Retire the 100-instance buffer with a 2-frame grace: the
      // preprocessor consumes the republished scene next frame (its update
      // rebuilds the copy records onto bufB), and this frame's still-queued
      // copies legitimately read bufA. After the grace, destroy() frees the
      // native buffer so any STALE record still pointing at it fails hard
      // and deterministically instead of silently reading freed VRAM.
      m_retire = self.bufA;
      m_retireCountdown = 2;
    }

    // vertices stays kBigCount; dirty_mesh stays false -- the ONLY change
    // the Instancer can see in phase 2 is the buffers[1] handle.
    self.instancer.inputs.points.dirty_mesh = false;

    // Tick the real engine object: operator()() runs the fingerprint check
    // and rebuild() runs the capacity clamp. This is the exact entry point
    // the Crousti CPU-node wrapper drives per frame.
    self.instancer();

    m_scene.state = self.instancer.m_wrapped_state;
    self.appliedPhase.store(phase);
  }

  void update(
      score::gfx::RenderList& r, QRhiResourceUpdateBatch& res,
      score::gfx::Edge*) override
  {
    if(m_retireCountdown > 0 && --m_retireCountdown == 0 && m_retire)
      m_retire->destroy();

    const int want = self.requestedPhase.load();
    if(want != self.appliedPhase.load())
      applyPhase(want, r, res);
  }

  // Publish to the downstream sink exactly as MergeGeometriesNode /
  // scene_outputs_storage do -- every frame; consumers short-circuit on
  // state pointer identity + version.
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
    auto it = std::find(sink->node->input.begin(), sink->node->input.end(), sink);
    if(it == sink->node->input.end())
      return;
    const int port_idx = int(it - sink->node->input.begin());
    rn_it->second->process(port_idx, m_scene, edge.source);
  }

  void runRenderPass(
      score::gfx::RenderList&, QRhiCommandBuffer&, score::gfx::Edge&) override
  {
  }
  void removeOutputPass(score::gfx::RenderList&, score::gfx::Edge&) override { }

  void release(score::gfx::RenderList&) override
  {
    delete self.bufA;
    self.bufA = nullptr;
    delete self.bufB;
    self.bufB = nullptr;
    delete m_posBuf;
    m_posBuf = nullptr;
    m_retire = nullptr;
    m_scene = {};
    m_initialized = false;
  }
};

score::gfx::NodeRenderer*
InstancerShrinkNode::createRenderer(score::gfx::RenderList&) const noexcept
{
  return new InstancerShrinkRenderer{*this};
}

// --- Pixel analysis ----------------------------------------------------------

struct ColumnStats
{
  int litColumns{};
  int litRuns{};
  int maxLitColumn{-1};
};

ColumnStats analyze(const ReadbackImage& img)
{
  ColumnStats s;
  bool prevLit = false;
  for(int x = 0; x < img.width; ++x)
  {
    bool lit = false;
    for(int y = 0; y < img.height && !lit; ++y)
    {
      const auto p = img.at(x, y);
      lit = int(p[0]) >= 200; // R == 1.0 is the drawn-coverage marker
    }
    if(lit)
    {
      ++s.litColumns;
      s.maxLitColumn = x;
      if(!prevLit)
        ++s.litRuns;
    }
    prevLit = lit;
  }
  return s;
}

struct Outcome
{
  bool skipped = false;
  std::string skip_reason;
  std::string error;
  std::string backend;

  // Phase 1 (100-instance buffer).
  uint32_t count1 = 0;
  int64_t version1 = -1;
  ColumnStats px1{};
  bool valid1 = false;

  // Phase 2 (shrunk 10-instance buffer, new handle).
  uint32_t count2 = 0;
  int64_t version2 = -1;
  void* handle2 = nullptr;
  void* expectedHandle2 = nullptr;
  ColumnStats px2{};
  bool valid2 = false;
  bool framesDiffer = false;

  int appliedPhase = 0;
};

Outcome run_shrink(score::gfx::GraphicsApi api)
{
  Outcome out;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;

    auto harness_uptr = std::make_unique<InstancerShrinkNode>();
    auto* harness = harness_uptr.get();

    const int hn = p.addNode(std::move(harness_uptr));
    const int flat
        = p.addNode(std::make_unique<score::gfx::ScenePreprocessorNode>());
    const int raster = p.addRaster(
        corpus("syn-instance-index-color.vs"),
        corpus("syn-instance-index-color.fs"));
    if(hn < 0 || flat < 0 || raster < 0)
    {
      out.error = "chain build failed: " + p.error();
      return;
    }

    auto* sceneOut = p.nodeSceneOut(hn, 0);
    auto* flatIn = p.nodeSceneIn(flat, 0);
    auto* flatOut = p.nodeGeometryOut(flat, 0);
    if(!sceneOut || !flatIn || !flatOut)
    {
      out.error = "scene ports missing on the chain";
      return;
    }
    p.wire(sceneOut, flatIn);
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

    // ---- Phase 1: 100 instances backed by a 6400-byte buffer. ----
    p.render(4);
    {
      const auto* inst = findInstance(harness->instancer.m_wrapped_state);
      if(!inst)
      {
        out.error = "phase 1: no instance_component published";
        return;
      }
      out.count1 = inst->instance_count;
      out.version1 = harness->instancer.m_wrapped_state->version;
    }
    const auto img1 = p.readback(sink);
    out.valid1 = img1.width == kSize && img1.height == kSize
                 && !img1.bytes.isEmpty();
    if(out.valid1)
      out.px1 = analyze(img1);

    // ---- Phase 2: replace buffers[1] with a NEW 640-byte buffer. ----
    harness->requestedPhase.store(2);
    p.render(4);
    out.appliedPhase = harness->appliedPhase.load();
    {
      const auto* inst = findInstance(harness->instancer.m_wrapped_state);
      if(!inst)
      {
        out.error = "phase 2: no instance_component published";
        return;
      }
      out.count2 = inst->instance_count;
      out.version2 = harness->instancer.m_wrapped_state->version;
      out.handle2 = handleOf(inst->instance_transforms);
      out.expectedHandle2 = harness->bufB;
    }
    const auto img2 = p.readback(sink);
    out.valid2 = img2.width == kSize && img2.height == kSize
                 && !img2.bytes.isEmpty();
    if(out.valid2)
      out.px2 = analyze(img2);
    out.framesDiffer = img1.bytes != img2.bytes;
  });
  return out;
}

} // namespace

TEST_CASE(
    "Instancer clamps a shrinking Points transforms buffer live: 100 -> 10 "
    "instances drawn, no over-read",
    "[gfx][threedim][instancer][scene][p0-3]")
{
  const auto api = GENERATE(from_range(platform_backends()));

  const auto r = run_shrink(api);
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);

  INFO("backend=" << r.backend << " error=" << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.appliedPhase == 2); // the harness renderer actually ran

  // ---- The clamp itself, against real QRhiBuffer capacities. ----
  // Phase 1: 6400 bytes / 64-byte mat4 stride = exactly the 100 points.
  CHECK(r.count1 == uint32_t(kBigCount));
  // Phase 2: byte_size shrank to 640 and the handle changed; the rebuild
  // fired (version advanced), instance_transforms points at the NEW buffer,
  // and instance_count is EXACTLY 10 -- not 100, not 0.
  CHECK(r.count2 == uint32_t(kSmallCount));
  CHECK(r.version2 > r.version1);
  REQUIRE(r.handle2 != nullptr);
  CHECK(r.handle2 == r.expectedHandle2);

  // ---- The drawn instance count follows, in pixels. ----
  REQUIRE(r.valid1);
  REQUIRE(r.valid2);
  INFO(
      "phase1: litCols=" << r.px1.litColumns << " runs=" << r.px1.litRuns
                         << " maxCol=" << r.px1.maxLitColumn);
  INFO(
      "phase2: litCols=" << r.px2.litColumns << " runs=" << r.px2.litRuns
                         << " maxCol=" << r.px2.maxLitColumn);

  // Phase 1: 100 two-px strips starting every 0.64 px tile the whole width.
  CHECK(r.px1.litColumns >= 48);

  // Phase 2: exactly 10 disjoint strips, 6.4 px apart, 2 px wide. The run
  // count is the drawn-instance counter: 100 stale instances (or garbage
  // translations from an over-read past the 640-byte buffer) cannot produce
  // 10 disjoint runs ending by column 60.
  CHECK(r.px2.litRuns == kSmallCount);
  CHECK(r.px2.litColumns >= kSmallCount);      // every strip >= 1 column
  CHECK(r.px2.litColumns <= 4 * kSmallCount);  // and <= 4 columns wide
  CHECK(r.px2.maxLitColumn <= 60); // last strip starts at 57.6 px; nothing
                                   // may be drawn beyond it
  CHECK(r.framesDiffer);           // difference oracle: the shrink is visible
}
