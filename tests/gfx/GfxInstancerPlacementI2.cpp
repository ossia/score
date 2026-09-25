// The Instancer's own TRS is the parent of the whole instance cloud, and the
// prototype's own transform does not scale the instance translations.
//
// The prototype quad sits under a 0.25 root scale and the Instancer's Scale is
// 0.5. Instances at x = -1.5, -0.5, 0.5, 1.5 must land at 0.5 * x, as four
// 4 px strips starting at columns 8, 24, 40 and 56 of a 64 px frame. With the
// translations also scaled by the prototype they collapse onto the centre;
// with the Instancer's Scale left out of the spread only the middle two
// (columns 16 and 48) remain in frame.
//
// Registration: see the test_gfx_instancer_placement_i2 target.
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

constexpr int kSize = 64;
constexpr int kCount = 4;
constexpr float kQuadW = 1.f;
constexpr float kProtoScale = 0.25f;
constexpr float kInstancerScale = 0.5f;
constexpr float kTranslations[kCount]{-1.5f, -0.5f, 0.5f, 1.5f};

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
  prim.stable_id = 0x1A12'0001u;

  auto mesh = std::make_shared<ossia::mesh_component>();
  mesh->primitives.push_back(std::move(prim));
  mesh->bounds = mesh->primitives[0].bounds;
  mesh->dirty_index = 1;

  ossia::scene_transform root_scale{};
  root_scale.rotation[3] = 1.f;
  root_scale.scale[0] = root_scale.scale[1] = root_scale.scale[2] = kProtoScale;

  auto children = std::make_shared<std::vector<ossia::scene_payload>>();
  children->push_back(root_scale);
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

// Drives the real Threedim::Instancer the way Crousti's CPU filter renderer
// does: init, then per frame update, runInitialPasses and operator(), then the
// scene is published to the downstream node.
struct InstancerNodeI2 final : score::gfx::ProcessNode
{
  mutable Threedim::Instancer instancer;
  std::shared_ptr<ossia::scene_state> protoScene = makePrototypeScene();

  InstancerNodeI2()
  {
    output.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Scene, {}});
  }

  score::gfx::NodeRenderer* createRenderer(score::gfx::RenderList&) const
      noexcept override;
};

struct InstancerRendererI2 final : score::gfx::NodeRenderer
{
  InstancerNodeI2& self;
  QRhiBuffer* m_transforms{};

  explicit InstancerRendererI2(const InstancerNodeI2& n)
      : NodeRenderer{n}
      , self{const_cast<InstancerNodeI2&>(n)}
  {
  }

  void init(score::gfx::RenderList& r, QRhiResourceUpdateBatch& res) override
  {
    auto* rhi = r.state.rhi;
    m_transforms = rhi->newBuffer(
        QRhiBuffer::Static,
        QRhiBuffer::UsageFlags(score::gfx::compatibleBufferUsage(
            *rhi, QRhiBuffer::VertexBuffer | QRhiBuffer::StorageBuffer)),
        kCount * 16);
    m_transforms->setName("InstancerPlacementI2Test::transforms");
    m_transforms->create();
    std::vector<float> data(kCount * 4, 0.f);
    for(int i = 0; i < kCount; ++i)
    {
      data[i * 4 + 0] = kTranslations[i];
      data[i * 4 + 3] = 1.f;
    }
    res.uploadStaticBuffer(
        m_transforms, 0, quint32(data.size() * sizeof(float)), data.data());

    auto& in = self.instancer.inputs;
    in.scene_in.scene.state = self.protoScene;
    in.transforms.buffer.handle = m_transforms;
    in.transforms.buffer.byte_size = kCount * 16;
    in.transforms.buffer.byte_offset = 0;
    in.format.value = Threedim::Instancer::Translation;
    in.count.value = kCount;
    in.position.value = {0.f, 0.f, 0.f};
    in.rotation.value = {0.f, 0.f, 0.f};
    in.scale.value = {kInstancerScale, kInstancerScale, kInstancerScale};

    self.instancer.init(r, res);
    m_initialized = true;
  }

  void update(
      score::gfx::RenderList& r, QRhiResourceUpdateBatch& res,
      score::gfx::Edge* e) override
  {
    self.instancer.update(r, res, e);
  }

  void runInitialPasses(
      score::gfx::RenderList& renderer, QRhiCommandBuffer& cb,
      QRhiResourceUpdateBatch*& res, score::gfx::Edge& edge) override
  {
    self.instancer.runInitialPasses(renderer, cb, res, edge);
    self.instancer();

    ossia::scene_spec spec;
    spec.state = self.instancer.outputs.scene_out.scene.state;
    if(!spec.state)
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
    rn_it->second->process(int(it - sink->node->input.begin()), spec, edge.source);
  }

  void runRenderPass(
      score::gfx::RenderList&, QRhiCommandBuffer&, score::gfx::Edge&) override
  {
  }
  void removeOutputPass(score::gfx::RenderList&, score::gfx::Edge&) override { }

  void release(score::gfx::RenderList& r) override
  {
    self.instancer.release(r);
    delete m_transforms;
    m_transforms = nullptr;
    m_initialized = false;
  }
};

score::gfx::NodeRenderer*
InstancerNodeI2::createRenderer(score::gfx::RenderList&) const noexcept
{
  return new InstancerRendererI2{*this};
}

struct Strips
{
  std::vector<int> starts;
  int litColumns{};
};

Strips strips(const ReadbackImage& img)
{
  Strips s;
  bool prev = false;
  for(int x = 0; x < img.width; ++x)
  {
    bool lit = false;
    for(int y = 0; y < img.height && !lit; ++y)
      lit = img.at(x, y)[0] >= 200;
    if(lit)
    {
      ++s.litColumns;
      if(!prev)
        s.starts.push_back(x);
    }
    prev = lit;
  }
  return s;
}

struct Outcome
{
  bool skipped{};
  std::string skip_reason;
  std::string error;
  Strips px;
};

Outcome run(score::gfx::GraphicsApi api)
{
  Outcome out;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int hn = p.addNode(std::make_unique<InstancerNodeI2>());
    const int flat = p.addNode(std::make_unique<score::gfx::ScenePreprocessorNode>());
    const int raster
        = p.addRaster(corpus("fixg-instance-model.vs"), corpus("fixg-instance-model.fs"));
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
      out.skipped = p.skipped();
      out.skip_reason = p.skipReason();
      out.error = p.error();
      return;
    }

    p.render(4);
    const auto img = p.readback(sink);
    if(img.width != kSize || img.height != kSize || img.bytes.isEmpty())
    {
      out.error = "empty readback";
      return;
    }
    out.px = strips(img);
  });
  return out;
}
}

TEST_CASE(
    "Instancer Scale scales the instance spread, the prototype's root scale "
    "does not",
    "[gfx][threedim][instancer][scene]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const auto r = run(api);
  if(r.skipped)
    SKIP(r.skip_reason);
  INFO("error=" << r.error);
  REQUIRE(r.error.empty());

  std::string starts;
  for(int x : r.px.starts)
    starts += std::to_string(x) + " ";
  INFO("strip starts: " << starts << " lit columns: " << r.px.litColumns);
  REQUIRE(r.px.starts.size() == std::size_t(kCount));
  for(int i = 0; i < kCount; ++i)
  {
    const int expected
        = int((kInstancerScale * kTranslations[i] + 1.f) * kSize / 2.f);
    CHECK(std::abs(r.px.starts[i] - expected) <= 1);
  }
  CHECK(r.px.litColumns <= kCount * 5);
}
