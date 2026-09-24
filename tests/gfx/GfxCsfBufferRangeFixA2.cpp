// CSF storage bindings follow what the producer publishes (agent A2: N54, N59).
//
//   * N54: a geometry attribute, a geometry AUXILIARY and a storage input whose
//     producer publishes a sub-range of a larger QRhiBuffer are bound at that
//     range: length() counts the published elements and element 0 is the first
//     published one, not the start of the allocation.
//   * N59: a read_write modifier on a GPU upstream that writes its buffer once
//     gives the same result every frame instead of accumulating, and leaves the
//     upstream buffer untouched.
//
//   DISPLAY=:0 SCORE_GPU_VALIDATION=0 ctest -R gfx_csf_buffer_range_fixa2
#include "IsfTestCommon.hpp"

#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <cstring>
#include <vector>

using namespace score::test::gfx;
using namespace score::test::gfx::isf;

namespace
{
struct Shot
{
  bool skipped = false;
  std::string skip_reason, backend, error;
  ReadbackImage image;
};

template <typename Build>
Shot render_pipeline(score::gfx::GraphicsApi be, int frames, Build&& build)
{
  Shot r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int sink = build(p);
    if(sink < 0 || !p.error().empty())
    {
      r.error = p.error().empty() ? "pipeline build failed" : p.error();
      return;
    }
    if(!p.create(be))
    {
      r.skipped = p.skipped();
      r.skip_reason = p.skipReason();
      r.backend = p.backend();
      r.error = p.error();
      return;
    }
    r.backend = p.backend();
    p.render(frames);
    r.image = p.readback(sink);
    if(r.error.empty())
      r.error = p.error();
    if(r.error.empty() && !r.image.valid())
      r.error = "empty readback";
  });
  return r;
}

#define FIXA2_REQUIRE_LIVE(s, backend)                                        \
  if((s).skipped)                                                            \
    SKIP((s).backend + ": " + (s).skip_reason);                              \
  if(const char* why = compute_shader_skip_reason(backend))                  \
    SKIP(std::string{backend_name(backend)} + ": " + why);                   \
  CAPTURE((s).backend);                                                      \
  REQUIRE((s).error.empty());                                                \
  REQUIRE((s).image.valid())

// A producer whose data lives in GPU buffers written once, at init.
//
// Geometry output: position and color, vec4 each, `count` vertices published
// out of a `capacity`-vertex allocation, plus an AUXILIARY "extra" holding five
// floats 42..46 published at byte 512 of a 1024-byte buffer.
// Buffer output: seven vec4 (x = 100..106) published at byte 256 of a
// 1024-byte buffer.
struct GpuStaticMeshNode final : score::gfx::ProcessNode
{
  std::vector<float> positions; // xyzw per vertex
  std::vector<float> colors;    // rgba per vertex
  int capacity{};

  GpuStaticMeshNode(std::vector<float> pos, std::vector<float> col, int cap)
      : positions{std::move(pos)}
      , colors{std::move(col)}
      , capacity{cap}
  {
    output.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Geometry, {}});
    output.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Buffer, {}});
  }
  int count() const noexcept { return int(positions.size() / 4); }

  score::gfx::NodeRenderer* createRenderer(score::gfx::RenderList&) const
      noexcept override;
};

struct GpuStaticMeshRenderer final : score::gfx::NodeRenderer
{
  static constexpr int kExtraOffset = 512;
  static constexpr int kValsOffset = 256;
  static constexpr int kSideSize = 1024;

  const GpuStaticMeshNode& node;
  QRhiBuffer* pos{};
  QRhiBuffer* col{};
  QRhiBuffer* extra{};
  QRhiBuffer* vals{};
  ossia::geometry_spec m_spec;

  explicit GpuStaticMeshRenderer(const GpuStaticMeshNode& n)
      : NodeRenderer{n}
      , node{n}
  {
  }

  QRhiBuffer* make(QRhi& rhi, int size, const char* name)
  {
    auto* b = rhi.newBuffer(
        QRhiBuffer::Static, QRhiBuffer::StorageBuffer | QRhiBuffer::VertexBuffer, size);
    b->setName(name);
    b->create();
    return b;
  }

  void init(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    auto& rhi = *renderer.state.rhi;
    const int n = node.count();
    const int attrBytes = n * 16;
    const int capBytes = node.capacity * 16;

    pos = make(rhi, capBytes, "fixa2.pos");
    col = make(rhi, capBytes, "fixa2.col");
    extra = make(rhi, kSideSize, "fixa2.extra");
    vals = make(rhi, kSideSize, "fixa2.vals");

    std::vector<char> zero(kSideSize > capBytes ? kSideSize : capBytes, 0);
    res.uploadStaticBuffer(pos, 0, capBytes, zero.data());
    res.uploadStaticBuffer(col, 0, capBytes, zero.data());
    res.uploadStaticBuffer(pos, 0, attrBytes, node.positions.data());
    res.uploadStaticBuffer(col, 0, attrBytes, node.colors.data());

    std::vector<float> side(kSideSize / 4, 0.f);
    for(int i = 0; i < 5; i++)
      side[kExtraOffset / 4 + i] = 42.f + i;
    res.uploadStaticBuffer(extra, 0, kSideSize, side.data());

    std::fill(side.begin(), side.end(), 0.f);
    for(int i = 0; i < 7; i++)
      side[kValsOffset / 4 + i * 4] = 100.f + i;
    res.uploadStaticBuffer(vals, 0, kSideSize, side.data());

    ossia::geometry g;
    g.vertices = n;
    g.topology = ossia::geometry::triangles;
    g.cull_mode = ossia::geometry::none;
    g.front_face = ossia::geometry::counter_clockwise;
    g.buffers.push_back({.data = ossia::geometry::gpu_buffer{pos, attrBytes}});
    g.buffers.push_back({.data = ossia::geometry::gpu_buffer{col, attrBytes}});
    g.buffers.push_back({.data = ossia::geometry::gpu_buffer{extra, kSideSize}});
    g.bindings.push_back({.byte_stride = 16});
    g.bindings.push_back({.byte_stride = 16});
    ossia::geometry::attribute a_pos;
    a_pos.binding = 0;
    a_pos.location = 0;
    a_pos.format = ossia::geometry::attribute::float4;
    a_pos.semantic = ossia::attribute_semantic::position;
    ossia::geometry::attribute a_col;
    a_col.binding = 1;
    a_col.location = 1;
    a_col.format = ossia::geometry::attribute::float4;
    a_col.semantic = ossia::attribute_semantic::color0;
    g.attributes.push_back(a_pos);
    g.attributes.push_back(a_col);
    g.input.push_back({.buffer = 0, .byte_offset = 0});
    g.input.push_back({.buffer = 1, .byte_offset = 0});
    g.auxiliary.push_back(
        {.name = "extra", .buffer = 2, .byte_offset = kExtraOffset, .byte_size = 5 * 4});

    m_spec.meshes = std::make_shared<ossia::mesh_list>();
    m_spec.meshes->meshes.push_back(std::move(g));
    m_initialized = true;
  }

  score::gfx::BufferView bufferForOutput(const score::gfx::Port& output) override
  {
    if(&output == node.output[1])
      return {.handle = vals, .byte_offset = kValsOffset, .byte_size = 7 * 16, .owned = false};
    return {};
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
    for(auto** b : {&pos, &col, &extra, &vals})
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
GpuStaticMeshNode::createRenderer(score::gfx::RenderList&) const noexcept
{
  return new GpuStaticMeshRenderer{*this};
}

std::unique_ptr<GpuStaticMeshNode> probeProducer()
{
  return std::make_unique<GpuStaticMeshNode>(
      std::vector<float>{1.f, 0.f, 0.f, 1.f, 2.f, 0.f, 0.f, 1.f, 3.f, 0.f, 0.f, 1.f},
      std::vector<float>(12, 1.f), 64);
}

std::unique_ptr<GpuStaticMeshNode> triangleProducer()
{
  return std::make_unique<GpuStaticMeshNode>(
      std::vector<float>{
          -1.f, -1.f, 0.f, 1.f, -0.5f, -1.f, 0.f, 1.f, -1.f, 1.f, 0.f, 1.f},
      std::vector<float>{
          0.f, 1.f, 0.f, 1.f, 0.f, 1.f, 0.f, 1.f, 0.f, 1.f, 0.f, 1.f},
      3);
}

bool identical(const ReadbackImage& a, const ReadbackImage& b)
{
  if(!a.valid() || !b.valid() || a.width != b.width || a.height != b.height)
    return false;
  for(int y = 0; y < a.height; ++y)
    for(int x = 0; x < a.width; ++x)
      if(a.at(x, y) != b.at(x, y))
        return false;
  return true;
}

int lit_pixels(const ReadbackImage& img)
{
  int n = 0;
  for(int y = 0; y < img.height; ++y)
    for(int x = 0; x < img.width; ++x)
    {
      const auto p = img.at(x, y);
      if(p[0] > 20 || p[1] > 20 || p[2] > 20)
        ++n;
    }
  return n;
}
}

TEST_CASE(
    "N54: CSF storage bindings cover the published range, not the allocation",
    "[gfx][csf][geometry][storage][fixa2]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  const Shot s = render_pipeline(backend, 3, [](GfxPipeline& p) {
    const int prod = p.addNode(probeProducer());
    const int cons = p.addCsf(corpus("fixa2-range-probe.cs"));
    const int sink = p.addSink({64, 64});
    if(prod < 0 || cons < 0)
      return -1;
    p.wire(p.nodeGeometryOut(prod, 0), p.geometryIn(cons, 0));
    p.wire(p.nodeBufferOut(prod, 0), p.bufferIn(cons, 0));
    p.wire(p.imageOut(cons, 0), p.sinkInput(sink));
    return sink;
  });
  FIXA2_REQUIRE_LIVE(s, backend);
  const auto c = s.image.center();
  INFO("centre = " << int(c[0]) << "," << int(c[1]) << "," << int(c[2]));
  CHECK(int(c[0]) > 200);
  CHECK(int(c[1]) > 200);
  CHECK(int(c[2]) > 200);
}

TEST_CASE(
    "N59: a read_write modifier on a static GPU mesh does not accumulate",
    "[gfx][csf][geometry][fixa2]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  const auto build = [](GfxPipeline& p) {
    const int mesh = p.addNode(triangleProducer());
    const int mod = p.addCsf(corpus("fixc-rw-offset.cs"));
    const int raster
        = p.addRaster(corpus("raw-raster-basic.vs"), corpus("raw-raster-basic.fs"));
    const int sink = p.addSink({64, 64});
    if(mesh < 0 || mod < 0 || raster < 0)
      return -1;
    p.wire(p.nodeGeometryOut(mesh, 0), p.geometryIn(mod, 0));
    p.wire(p.geometryOut(mod, 0), p.geometryIn(raster, 0));
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));
    return sink;
  };
  const Shot early = render_pipeline(backend, 2, build);
  const Shot late = render_pipeline(backend, 8, build);
  FIXA2_REQUIRE_LIVE(early, backend);
  REQUIRE(late.error.empty());
  REQUIRE(late.image.valid());

  const int litEarly = lit_pixels(early.image);
  const int litLate = lit_pixels(late.image);
  CAPTURE(litEarly, litLate);
  REQUIRE(litEarly > 50);

  CHECK(litEarly == litLate);
  CHECK(identical(early.image, late.image));

  int once = 0;
  for(int y = 0; y < late.image.height; ++y)
    for(int x = 0; x < late.image.width; ++x)
      if(near(late.image.at(x, y), {64, 255, 0, 255}, 8))
        ++once;
  CAPTURE(once);
  CHECK(once * 10 > litLate * 9);
}

TEST_CASE(
    "N59: a read_write modifier leaves its GPU upstream's buffer untouched",
    "[gfx][csf][geometry][fixa2]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  // One render list: the upstream feeds the modifier and, directly, a second
  // raster shown in the left half. The modified copy goes to the right half,
  // which the triangle never reaches, so the left half shows only the upstream.
  const Shot s = render_pipeline(backend, 4, [](GfxPipeline& p) {
    const int mesh = p.addNode(triangleProducer());
    const int mod = p.addCsf(corpus("fixc-rw-offset.cs"));
    const int raster
        = p.addRaster(corpus("raw-raster-basic.vs"), corpus("raw-raster-basic.fs"));
    const int modRaster
        = p.addRaster(corpus("raw-raster-basic.vs"), corpus("raw-raster-basic.fs"));
    const int split = p.addIsf(corpus("isf-two-images.fs"));
    const int sink = p.addSink({64, 64});
    if(mesh < 0 || mod < 0 || raster < 0 || modRaster < 0 || split < 0)
      return -1;
    p.wire(p.nodeGeometryOut(mesh, 0), p.geometryIn(mod, 0));
    p.wire(p.nodeGeometryOut(mesh, 0), p.geometryIn(raster, 0));
    p.wire(p.geometryOut(mod, 0), p.geometryIn(modRaster, 0));
    p.wire(p.imageOut(raster, 0), p.imageIn(split, 0));
    p.wire(p.imageOut(modRaster, 0), p.imageIn(split, 1));
    p.wire(p.imageOut(split, 0), p.sinkInput(sink));
    return sink;
  });
  FIXA2_REQUIRE_LIVE(s, backend);

  int green = 0, other = 0;
  for(int y = 0; y < s.image.height; ++y)
    for(int x = 0; x < s.image.width * 3 / 8; ++x)
    {
      const auto px = s.image.at(x, y);
      if(near(px, {0, 255, 0, 255}, 8))
        ++green;
      else if(px[0] > 20 || px[1] > 20 || px[2] > 20)
        ++other;
    }
  CAPTURE(green, other);
  CHECK(green > 50);
  CHECK(other == 0);
}
