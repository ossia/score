// A scene mesh whose position stream is an upstream GPU buffer is copied into
// the preprocessor's arena, and follows that buffer when the producer swaps it
// for another behind the same mesh.
//
// The preprocessor keeps the upstream QRhiBuffer* in m_pendingGpuCopies and
// re-issues the copies every frame while the mesh fingerprint holds; the
// fingerprint mixes in each attribute buffer's identity, so a swap must force
// a rebuild that copies from the new buffer, and must stop copying from the
// old one, which the producer deletes.
//
// Phase 1 publishes a quad over the left half of the screen, phase 2 the same
// mesh (same stable_id) with its positions in a new buffer covering the right
// half, the old buffer deleteLater'd.
//
// Registration:
//   score_add_gfx_test(scene_gpu_attribute_swap GfxScenePreprocessorGpuAttributeSwap.cpp)
#include <score_test/Gfx.hpp>

#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/ScenePreprocessorNode.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <algorithm>
#include <array>
#include <atomic>

using namespace score::test::gfx;

namespace
{
constexpr int kSize = 64;

QString corpus(const char* file)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR) + QStringLiteral("/") + file;
}

std::array<float, 24> quad(float x0, float x1)
{
  const float y0 = -0.8f, y1 = 0.8f;
  return {x0, y0, 0, 1, x1, y0, 0, 1, x1, y1, 0, 1,
          x0, y0, 0, 1, x1, y1, 0, 1, x0, y1, 0, 1};
}

std::shared_ptr<ossia::scene_state>
makeState(QRhiBuffer* positions, int64_t version)
{
  auto posRes = std::make_shared<ossia::buffer_resource>();
  ossia::gpu_buffer_handle h;
  h.native_handle = positions;
  h.byte_size = 24 * sizeof(float);
  posRes->resource = h;
  posRes->dirty_index = version;

  ossia::mesh_primitive prim;
  prim.vertex_buffers.push_back(posRes);
  ossia::vertex_attribute a;
  a.semantic = ossia::attribute_semantic::position;
  a.format = ossia::vertex_format::float4;
  a.buffer_index = 0;
  a.byte_offset = 0;
  a.byte_stride = 16;
  a.rate = ossia::vertex_attribute::input_rate::per_vertex;
  prim.attributes.push_back(a);
  prim.topology = ossia::primitive_topology::triangles;
  prim.vertex_count = 6;
  prim.stable_id = 0x5CA7E501u;
  prim.bounds = {{-1.f, -1.f, 0.f}, {1.f, 1.f, 0.f}};

  auto mesh = std::make_shared<ossia::mesh_component>();
  mesh->primitives.push_back(std::move(prim));
  mesh->bounds = mesh->primitives[0].bounds;
  mesh->dirty_index = version;

  auto children = std::make_shared<std::vector<ossia::scene_payload>>();
  children->push_back(ossia::mesh_component_ptr(std::move(mesh)));
  auto root = std::make_shared<ossia::scene_node>();
  root->children = std::move(children);
  auto roots = std::make_shared<std::vector<ossia::scene_node_ptr>>();
  roots->push_back(std::move(root));

  auto st = std::make_shared<ossia::scene_state>();
  st->roots = std::move(roots);
  st->version = version;
  st->dirty_index = version;
  return st;
}

struct GpuPosNode final : score::gfx::ProcessNode
{
  std::atomic<int> requestedPhase{1};
  std::atomic<int> appliedPhase{0};

  GpuPosNode()
  {
    output.push_back(
        new score::gfx::Port{this, {}, score::gfx::Types::Scene, {}});
  }
  score::gfx::NodeRenderer*
  createRenderer(score::gfx::RenderList&) const noexcept override;
};

struct GpuPosRenderer final : score::gfx::NodeRenderer
{
  GpuPosNode& self;
  ossia::scene_spec m_scene;
  QRhiBuffer* m_current{};

  explicit GpuPosRenderer(const GpuPosNode& n)
      : NodeRenderer{n}
      , self{const_cast<GpuPosNode&>(n)}
  {
  }

  void init(score::gfx::RenderList&, QRhiResourceUpdateBatch&) override
  {
    m_initialized = true;
  }

  void update(
      score::gfx::RenderList& r, QRhiResourceUpdateBatch& res,
      score::gfx::Edge*) override
  {
    const int want = self.requestedPhase.load();
    if(want == self.appliedPhase.load())
      return;
    const auto data = want == 1 ? quad(-0.9f, -0.1f) : quad(0.1f, 0.9f);
    auto* buf = r.state.rhi->newBuffer(
        QRhiBuffer::Static,
        QRhiBuffer::VertexBuffer | QRhiBuffer::StorageBuffer,
        sizeof(float) * data.size());
    buf->setName(want == 1 ? "gpu_pos_left" : "gpu_pos_right");
    buf->create();
    res.uploadStaticBuffer(buf, data.data());
    if(m_current)
      m_current->deleteLater();
    m_current = buf;
    m_scene.state = makeState(buf, want);
    self.appliedPhase.store(want);
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
    rn_it->second->process(
        int(it - sink->node->input.begin()), m_scene, edge.source);
  }

  void runRenderPass(
      score::gfx::RenderList&, QRhiCommandBuffer&, score::gfx::Edge&) override
  {
  }
  void removeOutputPass(score::gfx::RenderList&, score::gfx::Edge&) override { }

  void release(score::gfx::RenderList&) override
  {
    delete m_current;
    m_current = nullptr;
    m_scene = {};
    m_initialized = false;
  }
};

score::gfx::NodeRenderer*
GpuPosNode::createRenderer(score::gfx::RenderList&) const noexcept
{
  return new GpuPosRenderer{*this};
}
}

TEST_CASE(
    "a scene mesh follows its GPU position buffer when the producer swaps it",
    "[gfx][scene][lifetime][gpu-attribute]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  bool built = false;
  bool skipped = false;
  std::string err;
  std::array<uint8_t, 4> l1{}, r1{}, l2{}, r2{};

  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    auto node = std::make_unique<GpuPosNode>();
    auto* producer = node.get();
    const int hn = p.addNode(std::move(node));
    const int flat
        = p.addNode(std::make_unique<score::gfx::ScenePreprocessorNode>());
    const int raster = p.addRaster(
        corpus("syn-scene-gpu-pos.vs"), corpus("syn-scene-gpu-pos.fs"));
    if(hn < 0 || flat < 0 || raster < 0)
    {
      err = "chain build failed: " + p.error();
      return;
    }
    p.wire(p.nodeSceneOut(hn, 0), p.nodeSceneIn(flat, 0));
    p.wire(p.nodeGeometryOut(flat, 0), p.geometryIn(raster, 0));
    const int sink = p.addSink({kSize, kSize});
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      skipped = p.skipped();
      err = skipped ? std::string{} : p.error();
      return;
    }
    built = true;

    p.render(4);
    const auto img1 = p.readback(sink);
    if(!img1.valid())
    {
      err = "readback failed in phase 1";
      return;
    }
    l1 = img1.at(kSize / 4, kSize / 2);
    r1 = img1.at(3 * kSize / 4, kSize / 2);

    producer->requestedPhase.store(2);
    p.render(6);
    const auto img2 = p.readback(sink);
    if(!img2.valid())
    {
      err = "readback failed in phase 2";
      return;
    }
    l2 = img2.at(kSize / 4, kSize / 2);
    r2 = img2.at(3 * kSize / 4, kSize / 2);
  });

  if(skipped)
    SKIP("backend unavailable");

  INFO("backend=" << backend_name(api) << " error=" << err);
  INFO(
      "phase1 left g=" << int(l1[1]) << " right g=" << int(r1[1])
                       << " | phase2 left g=" << int(l2[1])
                       << " right g=" << int(r2[1]));
  REQUIRE(err.empty());
  REQUIRE(built);

  CHECK(l1[1] > 215);
  CHECK(r1[1] < 40);
  CHECK(l2[1] < 40);
  CHECK(r2[1] > 215);
}
