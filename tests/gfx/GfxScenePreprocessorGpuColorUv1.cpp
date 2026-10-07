// A scene mesh whose color0 and texcoord1 streams are upstream GPU buffers
// reaches the preprocessor's output with those values, not with the white
// colour and zero texcoord1 the preprocessor substitutes for a missing stream.
//
// Positions are in one GPU buffer; color0 (float4) and texcoord1 (float2) are
// interleaved in a second one at a 24-byte stride, so the copy has to follow
// the source stride and offset of each attribute. The raster shows color0 on
// the left half of the target and texcoord1 on the right half.
#include <score_test/Gfx.hpp>

#include "GfxSceneSource.hpp"

#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/ScenePreprocessorNode.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <array>
#include <cstdlib>
#include <vector>

using namespace score::test::gfx;

namespace
{
constexpr int kSize = 64;
constexpr float kColor[4] = {0.2f, 0.6f, 0.4f, 1.f};
constexpr float kUv1[2] = {0.8f, 0.3f};

QString corpus(const char* file)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR) + QStringLiteral("/") + file;
}

ossia::buffer_resource_ptr gpuResource(QRhiBuffer* buf, int64_t bytes)
{
  auto res = std::make_shared<ossia::buffer_resource>();
  ossia::gpu_buffer_handle h;
  h.native_handle = buf;
  h.byte_size = bytes;
  res->resource = h;
  res->dirty_index = 1;
  return res;
}

ossia::vertex_attribute attr(
    ossia::attribute_semantic sem, ossia::vertex_format fmt, uint32_t buffer,
    uint32_t offset, uint32_t stride)
{
  ossia::vertex_attribute a;
  a.semantic = sem;
  a.format = fmt;
  a.buffer_index = buffer;
  a.byte_offset = offset;
  a.byte_stride = stride;
  a.rate = ossia::vertex_attribute::input_rate::per_vertex;
  return a;
}

std::shared_ptr<ossia::scene_state> makeState(
    QRhiBuffer* positions, int64_t posBytes, QRhiBuffer* extra, int64_t extraBytes)
{
  ossia::mesh_primitive prim;
  prim.vertex_buffers.push_back(gpuResource(positions, posBytes));
  prim.vertex_buffers.push_back(gpuResource(extra, extraBytes));
  prim.attributes.push_back(attr(
      ossia::attribute_semantic::position, ossia::vertex_format::float4, 0, 0, 16));
  prim.attributes.push_back(attr(
      ossia::attribute_semantic::color0, ossia::vertex_format::float4, 1, 0, 24));
  prim.attributes.push_back(attr(
      ossia::attribute_semantic::texcoord1, ossia::vertex_format::float2, 1, 16, 24));
  prim.topology = ossia::primitive_topology::triangles;
  prim.vertex_count = 6;
  prim.stable_id = 0xC0109001u;
  prim.bounds = {{-1.f, -1.f, 0.f}, {1.f, 1.f, 0.f}};

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

struct GpuColorUv1Node final : score::gfx::ProcessNode
{
  GpuColorUv1Node()
  {
    output.push_back(
        new score::gfx::Port{this, {}, score::gfx::Types::Scene, {}});
  }
  score::gfx::NodeRenderer*
  createRenderer(score::gfx::RenderList&) const noexcept override;
};

struct GpuColorUv1Renderer final : score::test::gfx::scene::SourceRenderer
{
  QRhiBuffer* m_pos{};
  QRhiBuffer* m_extra{};

  explicit GpuColorUv1Renderer(const GpuColorUv1Node& n)
      : SourceRenderer{n}
  {
  }

  QRhiBuffer* make(
      score::gfx::RenderList& r, QRhiResourceUpdateBatch& res, const char* name,
      const std::vector<float>& data)
  {
    auto* buf = r.state.rhi->newBuffer(
        QRhiBuffer::Static,
        QRhiBuffer::VertexBuffer | QRhiBuffer::StorageBuffer,
        quint32(data.size() * sizeof(float)));
    buf->setName(name);
    buf->create();
    res.uploadStaticBuffer(buf, data.data());
    return buf;
  }

  void init(score::gfx::RenderList& r, QRhiResourceUpdateBatch& res) override
  {
    const float x0 = -0.9f, x1 = 0.9f, y0 = -0.8f, y1 = 0.8f;
    const std::vector<float> pos{x0, y0, 0, 1, x1, y0, 0, 1, x1, y1, 0, 1,
                                 x0, y0, 0, 1, x1, y1, 0, 1, x0, y1, 0, 1};
    std::vector<float> extra;
    for(int v = 0; v < 6; ++v)
    {
      extra.insert(extra.end(), kColor, kColor + 4);
      extra.insert(extra.end(), kUv1, kUv1 + 2);
    }
    m_pos = make(r, res, "gpu_pos", pos);
    m_extra = make(r, res, "gpu_color0_uv1", extra);
    m_scene.state = makeState(
        m_pos, int64_t(pos.size() * sizeof(float)), m_extra,
        int64_t(extra.size() * sizeof(float)));
    m_initialized = true;
  }

  void update(
      score::gfx::RenderList&, QRhiResourceUpdateBatch&, score::gfx::Edge*) override
  {
  }

  void release(score::gfx::RenderList& r) override
  {
    SourceRenderer::release(r);
    delete m_pos;
    delete m_extra;
    m_pos = m_extra = nullptr;
  }
};

score::gfx::NodeRenderer*
GpuColorUv1Node::createRenderer(score::gfx::RenderList&) const noexcept
{
  return new GpuColorUv1Renderer{*this};
}

bool matches(uint8_t got, float want)
{
  return std::abs(int(got) - int(want * 255.f + 0.5f)) <= 8;
}
}

TEST_CASE(
    "a scene mesh's GPU color0 and texcoord1 reach the preprocessor output",
    "[gfx][scene][gpu-attribute]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  bool built = false;
  bool skipped = false;
  std::string err;
  std::array<uint8_t, 4> color{}, uv1{};

  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int src = p.addNode(std::make_unique<GpuColorUv1Node>());
    const int flat
        = p.addNode(std::make_unique<score::gfx::ScenePreprocessorNode>());
    const int raster = p.addRaster(
        corpus("syn-scene-gpu-color-uv1.vs"), corpus("syn-scene-gpu-color-uv1.fs"));
    if(src < 0 || flat < 0 || raster < 0)
    {
      err = "chain build failed: " + p.error();
      return;
    }
    p.wire(p.nodeSceneOut(src, 0), p.nodeSceneIn(flat, 0));
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
    const auto img = p.readback(sink);
    if(!img.valid())
    {
      err = "readback failed";
      return;
    }
    color = img.at(kSize / 4, kSize / 2);
    uv1 = img.at(3 * kSize / 4, kSize / 2);
  });

  if(skipped)
    SKIP("backend unavailable");

  INFO("backend=" << backend_name(api) << " error=" << err);
  INFO("color0 " << scene::rgba_string(color) << " texcoord1 " << scene::rgba_string(uv1));
  REQUIRE(err.empty());
  REQUIRE(built);

  CHECK(matches(color[0], kColor[0]));
  CHECK(matches(color[1], kColor[1]));
  CHECK(matches(color[2], kColor[2]));
  CHECK(matches(uv1[0], kUv1[0]));
  CHECK(matches(uv1[1], kUv1[1]));
}
