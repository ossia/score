// A uniform block is only bound to a buffer that covers it. A producer
// publishing a uniform buffer smaller than the block the shader declares (a
// 128-byte buffer for a 240-byte, camera-sized block) was bound anyway, and
// every member past its end read whatever followed it in memory (shader board
// U42, plan step 3: insufficient extent). The consumer keeps its zero-filled
// placeholder and says why, for a name-matched uniform INPUT and for a
// uniform AUXILIARY block alike; a buffer that covers the block is bound.
// A storage block fed that uniform-only buffer aborted in Qt on OpenGL
// (bufD->m_usage.testFlag(QRhiBuffer::StorageBuffer)): it keeps its
// placeholder too (plan step 3: incompatible usage).
#include "GfxLogCapture.hpp"
#include "IsfTestCommon.hpp"

#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <QFile>
#include <QTemporaryDir>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <algorithm>
#include <array>
#include <string>
#include <vector>

using namespace score::test::gfx;
using score::test::gfx::isf::corpus;

namespace
{
//! A viewport-covering triangle with a `params` uniform auxiliary of `bytes`
//! bytes, every float 7.
struct UniformAuxNode final : score::gfx::ProcessNode
{
  int bytes{128};
  UniformAuxNode()
  {
    output.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Geometry, {}});
  }
  score::gfx::NodeRenderer* createRenderer(score::gfx::RenderList&) const
      noexcept override;
};

struct UniformAuxRenderer final : score::gfx::NodeRenderer
{
  const UniformAuxNode& node;
  QRhiBuffer* pos{};
  QRhiBuffer* params{};
  ossia::geometry_spec m_spec;

  explicit UniformAuxRenderer(const UniformAuxNode& n)
      : NodeRenderer{n}
      , node{n}
  {
  }

  void init(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    auto& rhi = *renderer.state.rhi;
    pos = rhi.newBuffer(QRhiBuffer::Static, QRhiBuffer::VertexBuffer, 3 * 16);
    pos->setName("ubo_extent.pos");
    pos->create();
    params = rhi.newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, node.bytes);
    params->setName("ubo_extent.params");
    params->create();
    const float tri[12] = {-1.f, -1.f, 0.f, 1.f, 3.f, -1.f, 0.f, 1.f, -1.f, 3.f, 0.f, 1.f};
    res.uploadStaticBuffer(pos, 0, sizeof(tri), tri);
    std::vector<float> v(node.bytes / 4, 7.f);
    res.updateDynamicBuffer(params, 0, node.bytes, v.data());

    ossia::geometry g;
    g.vertices = 3;
    g.topology = ossia::geometry::triangles;
    g.cull_mode = ossia::geometry::none;
    g.front_face = ossia::geometry::counter_clockwise;
    g.buffers.push_back({.data = ossia::geometry::gpu_buffer{pos, 3 * 16}});
    g.buffers.push_back({.data = ossia::geometry::gpu_buffer{params, node.bytes}});
    g.bindings.push_back({.byte_stride = 16});
    ossia::geometry::attribute a_pos;
    a_pos.binding = 0;
    a_pos.location = 0;
    a_pos.format = ossia::geometry::attribute::float4;
    a_pos.semantic = ossia::attribute_semantic::position;
    g.attributes.push_back(a_pos);
    g.input.push_back({.buffer = 0, .byte_offset = 0});
    g.auxiliary.push_back(
        {.name = "params", .buffer = 1, .byte_offset = 0, .byte_size = node.bytes});
    m_spec.meshes = std::make_shared<ossia::mesh_list>();
    m_spec.meshes->meshes.push_back(std::move(g));
    m_initialized = true;
  }

  void update(score::gfx::RenderList&, QRhiResourceUpdateBatch&, score::gfx::Edge*)
      override
  {
  }

  void runInitialPasses(
      score::gfx::RenderList& renderer, QRhiCommandBuffer&, QRhiResourceUpdateBatch*&,
      score::gfx::Edge& edge) override
  {
    auto* sink = edge.sink;
    if(edge.source != node.output[0] || !m_spec.meshes || !sink || !sink->node)
      return;
    auto rn_it = sink->node->renderedNodes.find(&renderer);
    if(rn_it == sink->node->renderedNodes.end())
      return;
    auto it = std::find(sink->node->input.begin(), sink->node->input.end(), sink);
    if(it == sink->node->input.end())
      return;
    rn_it->second->process(int(it - sink->node->input.begin()), m_spec, edge.source);
  }
  void runRenderPass(score::gfx::RenderList&, QRhiCommandBuffer&, score::gfx::Edge&)
      override
  {
  }
  void removeOutputPass(score::gfx::RenderList&, score::gfx::Edge&) override { }
  void release(score::gfx::RenderList& r) override
  {
    for(auto** b : {&pos, &params})
    {
      if(*b)
        r.releaseBuffer(*b);
      *b = nullptr;
    }
    m_spec = {};
    m_initialized = false;
  }
};

score::gfx::NodeRenderer*
UniformAuxNode::createRenderer(score::gfx::RenderList&) const noexcept
{
  return new UniformAuxRenderer{*this};
}

// 240 bytes in std140: first, three mat4, two vec4. Red = first.x / 255.
// `kind` is the block's TYPE: "uniform", or "storage" for a storage block the
// uniform-only buffer cannot back.
std::string fragment(bool nested, const std::string& kind = "uniform")
{
  const std::string block
      = R"({ "NAME": "params", "TYPE": ")" + kind
        + R"(", "ACCESS": "read_only", "LAYOUT": [
      { "NAME": "first", "TYPE": "vec4" }, { "NAME": "m", "TYPE": "mat4[3]" },
      { "NAME": "b", "TYPE": "vec4" }, { "NAME": "last", "TYPE": "vec4" } ] })";
  return R"_(/*{
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "VERTEX_INPUTS": [ { "TYPE": "vec4", "NAME": "position" } ],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "seen" } ],
  )_" + std::string(nested ? R"("AUXILIARY": [ )" + block + " ],\n  \"INPUTS\": []"
                           : R"("INPUTS": [ )" + block + " ]")
         + R"_(,
  "PIPELINE_STATE": { "DEPTH_TEST": false, "DEPTH_WRITE": false, "CULL_MODE": "none" }
}*/
void main() { seen = vec4(params.first.x / 255.0, params.last.x / 255.0, 0.0, 1.0); }
)_";
}

struct Shot
{
  bool skipped = false;
  std::string err;
  std::array<uint8_t, 4> centre{};
  int warnings = 0;
};

Shot shoot(
    score::gfx::GraphicsApi api, bool nested, int bytes,
    const std::string& kind = "uniform", const char16_t* warning = u"smaller than")
{
  Shot s;
  QTemporaryDir dir;
  const QString fs = dir.filePath("ubo-extent.fs");
  {
    QFile f{fs};
    if(!dir.isValid() || !f.open(QIODevice::WriteOnly))
    {
      s.err = "cannot write the shader";
      return s;
    }
    f.write(QByteArray::fromStdString(fragment(nested, kind)));
  }
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    LogCapture log;
    GfxPipeline p;
    auto n = std::make_unique<UniformAuxNode>();
    n->bytes = bytes;
    const int producer = p.addNode(std::move(n));
    const int raster = p.addRaster(corpus("rr-aux-count.vs"), fs);
    if(producer < 0 || raster < 0)
    {
      s.err = "build failed: " + p.error();
      return;
    }
    p.wire(p.nodeGeometryOut(producer, 0), p.geometryIn(raster, 0));
    const int sink = p.addSink({16, 16});
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      s.skipped = p.skipped();
      s.err = s.skipped ? std::string{} : p.error();
      return;
    }
    p.render(3);
    const auto img = p.readback(sink);
    if(!img.valid())
    {
      s.err = "readback failed";
      return;
    }
    s.centre = img.center();
    s.warnings = log.count({u"params", warning});
  });
  return s;
}
}

TEST_CASE(
    "a uniform block is not bound to a buffer smaller than it", "[gfx][ubo][U42]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const bool nested = GENERATE(true, false);
  CAPTURE(backend_name(api), nested);

  const auto shortBuffer = shoot(api, nested, 128);
  if(shortBuffer.skipped)
    SKIP("backend unavailable");
  INFO("error=" << shortBuffer.err);
  REQUIRE(shortBuffer.err.empty());
  INFO("128 bytes: first " << int(shortBuffer.centre[0]) << ", last "
                           << int(shortBuffer.centre[1]));
  // The placeholder: every member reads zero, and one warning says why.
  CHECK(int(shortBuffer.centre[0]) == 0);
  CHECK(int(shortBuffer.centre[1]) == 0);
  CHECK(shortBuffer.warnings == 1);

  const auto covering = shoot(api, nested, 256);
  INFO("error=" << covering.err);
  REQUIRE(covering.err.empty());
  INFO("256 bytes: first " << int(covering.centre[0]) << ", last "
                           << int(covering.centre[1]));
  CHECK(int(covering.centre[0]) == 7);
  CHECK(int(covering.centre[1]) == 7);
  CHECK(covering.warnings == 0);
}

TEST_CASE(
    "a storage block is not bound to a buffer without storage usage",
    "[gfx][ubo][U42]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const bool nested = GENERATE(true, false);
  CAPTURE(backend_name(api), nested);
  const auto s = shoot(api, nested, 256, "storage", u"StorageBuffer usage");
  if(s.skipped)
    SKIP("backend unavailable");
  INFO("error=" << s.err);
  REQUIRE(s.err.empty());
  CHECK(int(s.centre[0]) == 0);
  CHECK(s.warnings == 1);
}
