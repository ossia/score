// Instancer placement through the scene preprocessor (N61 options B and D).
//
// The prototype quad sits under a 0.25 root scale and the Instancer's Scale is
// 0.5. Instances at x = -1.5, -0.5, 0.5, 1.5 must land at 0.5 * x, as four
// strips starting at columns 8, 24, 40 and 56 of a 64 px frame, in both the
// full-matrix and the translation-only layout (option B: the Instancer's TRS
// is the parent of the cloud, the prototype's transform does not scale the
// spread).
//
// Full matrix (option D) also carries per-instance rotation/scale and custom
// data: with TRS input scaling instance i by (i + 1) on x, the strips are
// 4, 8, 12 and 16 px wide, and each strip takes its instance's custom colour.
// Translation only keeps the compact layout: every strip is 4 px and white.
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
#include <array>
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
constexpr float kCustom[kCount][4]{
    {1.f, 0.f, 0.f, 1.f}, {0.f, 1.f, 0.f, 1.f}, {0.f, 0.f, 1.f, 1.f}, {1.f, 1.f, 0.f, 1.f}};

struct Params
{
  Threedim::Instancer::InstanceTransformMode mode{Threedim::Instancer::FullMatrix};
  bool trs{};
  bool custom{};
};

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
  Params params;

  explicit InstancerNodeI2(Params p)
      : params{p}
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
  QRhiBuffer* m_custom{};

  explicit InstancerRendererI2(const InstancerNodeI2& n)
      : NodeRenderer{n}
      , self{const_cast<InstancerNodeI2&>(n)}
  {
  }

  void init(score::gfx::RenderList& r, QRhiResourceUpdateBatch& res) override
  {
    auto* rhi = r.state.rhi;
    const Params& prm = self.params;
    const int stride = prm.trs ? 10 : 4;
    const auto usage = QRhiBuffer::UsageFlags(score::gfx::compatibleBufferUsage(
        *rhi, QRhiBuffer::VertexBuffer | QRhiBuffer::StorageBuffer));
    m_transforms = rhi->newBuffer(QRhiBuffer::Static, usage, kCount * stride * 4);
    m_transforms->setName("InstancerPlacementI2Test::transforms");
    m_transforms->create();
    std::vector<float> data(kCount * stride, 0.f);
    for(int i = 0; i < kCount; ++i)
    {
      float* e = &data[i * stride];
      e[0] = kTranslations[i];
      if(prm.trs)
      {
        e[6] = 1.f;
        e[7] = float(i + 1);
        e[8] = 1.f;
        e[9] = 1.f;
      }
      else
        e[3] = 1.f;
    }
    res.uploadStaticBuffer(
        m_transforms, 0, quint32(data.size() * sizeof(float)), data.data());

    auto& in = self.instancer.inputs;
    in.scene_in.scene.state = self.protoScene;
    in.transforms.buffer.handle = m_transforms;
    in.transforms.buffer.byte_size = kCount * stride * 4;
    in.transforms.buffer.byte_offset = 0;
    in.format.value = prm.trs ? Threedim::Instancer::TRS : Threedim::Instancer::Translation;
    in.transform_mode.value = prm.mode;
    in.count.value = kCount;
    in.position.value = {0.f, 0.f, 0.f};
    in.rotation.value = {0.f, 0.f, 0.f};
    in.scale.value = {kInstancerScale, kInstancerScale, kInstancerScale};

    if(prm.custom)
    {
      m_custom = rhi->newBuffer(QRhiBuffer::Static, usage, kCount * 16);
      m_custom->setName("InstancerPlacementI2Test::custom");
      m_custom->create();
      res.uploadStaticBuffer(m_custom, 0, kCount * 16, &kCustom[0][0]);
      in.custom.buffer.handle = m_custom;
      in.custom.buffer.byte_size = kCount * 16;
      in.custom.buffer.byte_offset = 0;
    }

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
    delete m_custom;
    m_custom = nullptr;
    m_initialized = false;
  }
};

score::gfx::NodeRenderer*
InstancerNodeI2::createRenderer(score::gfx::RenderList&) const noexcept
{
  return new InstancerRendererI2{*this};
}

struct Strip
{
  int start{};
  int width{};
  std::array<uint8_t, 3> color{};
};

std::vector<Strip> strips(const ReadbackImage& img)
{
  std::vector<Strip> out;
  const int y = img.height / 2;
  bool prev = false;
  for(int x = 0; x < img.width; ++x)
  {
    const auto px = img.at(x, y);
    const bool lit = px[0] >= 200 || px[1] >= 200 || px[2] >= 200;
    if(lit && !prev)
      out.push_back(Strip{x, 0, {px[0], px[1], px[2]}});
    if(lit)
      out.back().width++;
    prev = lit;
  }
  return out;
}

struct Outcome
{
  bool skipped{};
  std::string skip_reason;
  std::string error;
  std::vector<Strip> px;
};

Outcome run(score::gfx::GraphicsApi api, Params prm)
{
  Outcome out;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int hn = p.addNode(std::make_unique<InstancerNodeI2>(prm));
    const int flat = p.addNode(std::make_unique<score::gfx::ScenePreprocessorNode>());
    const int raster
        = p.addRaster(corpus("instancer-d-model.vs"), corpus("instancer-d-model.fs"));
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

namespace
{
std::string describe(const std::vector<Strip>& px)
{
  std::string d;
  for(const auto& st : px)
    d += std::to_string(st.start) + "+" + std::to_string(st.width) + "("
         + std::to_string(st.color[0]) + "," + std::to_string(st.color[1]) + ","
         + std::to_string(st.color[2]) + ") ";
  return d;
}

int expectedStart(int i)
{
  return int((kInstancerScale * kTranslations[i] + 1.f) * kSize / 2.f);
}
}

TEST_CASE(
    "Instancer Scale scales the instance spread, the prototype's root scale "
    "does not",
    "[gfx][threedim][instancer][scene]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const auto mode = GENERATE(
      Threedim::Instancer::FullMatrix, Threedim::Instancer::TranslationOnly);
  CAPTURE(backend_name(api), int(mode));

  const auto r = run(api, Params{mode, false, false});
  if(r.skipped)
    SKIP(r.skip_reason);
  INFO("error=" << r.error);
  REQUIRE(r.error.empty());
  INFO("strips: " << describe(r.px));
  REQUIRE(r.px.size() == std::size_t(kCount));
  for(int i = 0; i < kCount; ++i)
  {
    CHECK(std::abs(r.px[i].start - expectedStart(i)) <= 1);
    CHECK(std::abs(r.px[i].width - 4) <= 1);
  }
}

TEST_CASE(
    "Full-matrix instances carry their own scale and custom data, the compact "
    "layout does not",
    "[gfx][threedim][instancer][scene]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const auto mode = GENERATE(
      Threedim::Instancer::FullMatrix, Threedim::Instancer::TranslationOnly);
  CAPTURE(backend_name(api), int(mode));
  const bool full = mode == Threedim::Instancer::FullMatrix;

  const auto r = run(api, Params{mode, true, true});
  if(r.skipped)
    SKIP(r.skip_reason);
  INFO("error=" << r.error);
  REQUIRE(r.error.empty());
  INFO("strips: " << describe(r.px));
  REQUIRE(r.px.size() == std::size_t(kCount));
  for(int i = 0; i < kCount; ++i)
  {
    CHECK(std::abs(r.px[i].start - expectedStart(i)) <= 1);
    const int width = std::min(full ? 4 * (i + 1) : 4, kSize - expectedStart(i));
    CHECK(std::abs(r.px[i].width - width) <= 1);
    for(int c = 0; c < 3; ++c)
    {
      const int want = full ? int(kCustom[i][c] * 255.f) : 255;
      CHECK(std::abs(int(r.px[i].color[c]) - want) <= 2);
    }
  }
}
