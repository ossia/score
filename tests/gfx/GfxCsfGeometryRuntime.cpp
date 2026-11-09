// A RUNTIME geometry input reads how its upstream is drawn: the elements,
// triangles, instances and vertices its draws reach, through uint16 and uint32
// index buffers at a byte offset, with negative and positive base vertices,
// several draws, triangle lists, strips and fans, primitive-restart indices,
// draw records and a live draw count written on the GPU, an index buffer the
// shader cannot bind, zero instances and no upstream at all; and which
// declared attributes the upstream provides.
//
// A native source publishes each layout; csf-runtime-probe.cs paints one
// accessor result per pixel and is compared with a reference of the
// documented semantics (isf.cpp runtime_accessors).
//
//   SCORE_TEST_API=vulkan ctest -R gfx_csf_geometry_runtime
#include "IsfTestCommon.hpp"

#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <algorithm>
#include <array>
#include <cstring>

using namespace score::test::gfx;
using namespace score::test::gfx::isf;

namespace
{
constexpr uint32_t invalid = 0xFFFFFFFFu;

struct Fixture
{
  int vertices{};
  int instances{1};
  decltype(ossia::geometry::topology) topology{ossia::geometry::triangles};
  int width{}; // index width in bytes, 0: not indexed
  std::vector<uint32_t> indices;
  int leading{}; // indices of garbage before the first one
  std::vector<ossia::geometry::draw_command> draws;
  bool gpu{};
  bool indexStorage{true};
  bool gpuDraws{};
  int gpuDrawCount{-1};
  bool weight{};
};

struct SourceNode final : score::gfx::ProcessNode
{
  Fixture f;
  explicit SourceNode(Fixture fx)
      : f{std::move(fx)}
  {
    output.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Geometry, {}});
  }
  score::gfx::NodeRenderer* createRenderer(score::gfx::RenderList&) const noexcept override;
};

struct SourceRenderer final : score::gfx::NodeRenderer
{
  const SourceNode& node;
  std::vector<QRhiBuffer*> owned;
  ossia::geometry_spec spec;

  explicit SourceRenderer(const SourceNode& n)
      : NodeRenderer{n}
      , node{n}
  {
  }

  ossia::geometry::buffer buffer(
      QRhi& rhi, QRhiResourceUpdateBatch& res, const std::vector<char>& bytes, bool gpu,
      QRhiBuffer::UsageFlags usage)
  {
    const int64_t size = bytes.size();
    if(!gpu)
    {
      std::shared_ptr<void> copy(new char[size], std::default_delete<char[]>());
      std::memcpy(copy.get(), bytes.data(), size);
      return {.data = ossia::geometry::cpu_buffer{std::move(copy), size}};
    }
    auto* b = rhi.newBuffer(QRhiBuffer::Static, usage, size);
    b->setName("RuntimeSource");
    b->create();
    res.uploadStaticBuffer(b, 0, size, bytes.data());
    owned.push_back(b);
    return {.data = ossia::geometry::gpu_buffer{b, size}};
  }

  template <typename T>
  static void put(std::vector<char>& out, T v)
  {
    const auto at = out.size();
    out.resize(at + sizeof(T));
    std::memcpy(out.data() + at, &v, sizeof(T));
  }

  void addAttribute(
      ossia::geometry& g, ossia::geometry::buffer b, uint32_t stride,
      decltype(ossia::geometry::attribute::format) format, ossia::attribute_semantic sem,
      const char* name)
  {
    const int idx = g.buffers.size();
    g.buffers.push_back(std::move(b));
    g.bindings.push_back({.byte_stride = stride});
    g.input.push_back({.buffer = idx, .byte_offset = 0});
    ossia::geometry::attribute a;
    a.binding = idx;
    a.location = idx;
    a.format = format;
    a.semantic = sem;
    if(sem == ossia::attribute_semantic::custom)
      a.name = name;
    g.attributes.push_back(a);
  }

  void init(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    auto& rhi = *renderer.state.rhi;
    const auto& f = node.f;
    const auto storage = QRhiBuffer::StorageBuffer | QRhiBuffer::VertexBuffer;
    ossia::geometry g;
    g.vertices = f.vertices;
    g.instances = f.instances;
    g.topology = f.topology;
    g.cull_mode = ossia::geometry::none;
    g.front_face = ossia::geometry::counter_clockwise;

    std::vector<char> pos;
    for(int i = 0; i < std::max(1, f.vertices); i++)
      for(float c : {float(i), 0.f, 0.f, 1.f})
        put(pos, c);
    addAttribute(
        g, buffer(rhi, res, pos, f.gpu, storage), 16, ossia::geometry::attribute::float4,
        ossia::attribute_semantic::position, "");
    if(f.weight)
    {
      std::vector<char> w;
      for(int i = 0; i < std::max(1, f.vertices); i++)
        put(w, 0.5f);
      addAttribute(
          g, buffer(rhi, res, w, f.gpu, storage), 4, ossia::geometry::attribute::float1,
          ossia::attribute_semantic::custom, "weight");
    }

    if(f.width)
    {
      std::vector<char> idx;
      for(int i = 0; i < f.leading; i++)
        f.width == 2 ? put(idx, uint16_t(0x7777)) : put(idx, uint32_t(0x77777777));
      for(uint32_t v : f.indices)
        f.width == 2 ? put(idx, uint16_t(v)) : put(idx, v);
      while(idx.size() % 4)
        idx.push_back(0);
      const auto usage = f.indexStorage
                             ? QRhiBuffer::UsageFlags{QRhiBuffer::IndexBuffer | QRhiBuffer::StorageBuffer}
                             : QRhiBuffer::UsageFlags{QRhiBuffer::IndexBuffer};
      g.index.buffer = g.buffers.size();
      g.buffers.push_back(buffer(rhi, res, idx, f.gpu, usage));
      g.index.byte_offset = int64_t(f.leading) * f.width;
      g.index.format = f.width == 2 ? decltype(g.index)::uint16 : decltype(g.index)::uint32;
      g.indices = f.indices.size();
    }

    if(f.gpuDraws)
    {
      std::vector<char> rec;
      for(const auto& c : f.draws)
      {
        put(rec, c.index_or_vertex_count);
        put(rec, c.instance_count);
        put(rec, c.first_index_or_vertex);
        if(f.width)
        {
          put(rec, c.base_vertex);
          put(rec, c.first_instance);
        }
        else
        {
          put(rec, c.first_instance);
          put(rec, uint32_t(0));
        }
      }
      // Records past the live count: never read. Without a count, every
      // record is a draw.
      if(f.gpuDrawCount >= 0)
        for(int i = 0; i < 2 * 5; i++)
          put(rec, uint32_t(99));
      auto b = buffer(rhi, res, rec, true, QRhiBuffer::StorageBuffer);
      g.indirect_count = *ossia::get_if<ossia::geometry::gpu_buffer>(&b.data);
      g.buffers.push_back(std::move(b));
      if(f.gpuDrawCount >= 0)
      {
        std::vector<char> count;
        put(count, uint32_t(0xAAAA));
        put(count, uint32_t(f.gpuDrawCount));
        put(count, uint32_t(0));
        put(count, uint32_t(0));
        g.auxiliary.push_back(
            {.name = "_indirect_draw_count", .buffer = (int)g.buffers.size(),
             .byte_offset = 4, .byte_size = 4});
        g.buffers.push_back(buffer(rhi, res, count, true, QRhiBuffer::StorageBuffer));
      }
    }
    else
    {
      g.cpu_draw_commands.assign(f.draws.begin(), f.draws.end());
    }

    spec.meshes = std::make_shared<ossia::mesh_list>();
    spec.meshes->meshes.push_back(std::move(g));
    spec.meshes->dirty_index = 1;
    spec.filters = std::make_shared<ossia::geometry_filter_list>();
    m_initialized = true;
  }

  void update(score::gfx::RenderList&, QRhiResourceUpdateBatch&, score::gfx::Edge*) override { }

  void runInitialPasses(
      score::gfx::RenderList& renderer, QRhiCommandBuffer&, QRhiResourceUpdateBatch*&,
      score::gfx::Edge& edge) override
  {
    auto* sink = edge.sink;
    if(!spec.meshes || !sink || !sink->node)
      return;
    auto rn_it = sink->node->renderedNodes.find(&renderer);
    if(rn_it == sink->node->renderedNodes.end())
      return;
    auto it = std::find(sink->node->input.begin(), sink->node->input.end(), sink);
    if(it == sink->node->input.end())
      return;
    rn_it->second->process(int(it - sink->node->input.begin()), spec, edge.source);
  }

  void runRenderPass(score::gfx::RenderList&, QRhiCommandBuffer&, score::gfx::Edge&) override { }
  void removeOutputPass(score::gfx::RenderList&, score::gfx::Edge&) override { }
  void release(score::gfx::RenderList& r) override
  {
    for(auto* b : owned)
      r.releaseBuffer(b);
    owned.clear();
    spec = {};
    m_initialized = false;
  }
};

score::gfx::NodeRenderer* SourceNode::createRenderer(score::gfx::RenderList&) const noexcept
{
  return new SourceRenderer{*this};
}

using Grid = std::array<std::array<int, 16>, 5>;

// The documented accessor semantics, on the CPU.
Grid reference(const Fixture* f)
{
  struct Rec
  {
    uint32_t count, instances, first;
    int32_t base;
    uint32_t firstInstance;
  };
  std::vector<Rec> recs;
  const bool indexed = f && f->width;
  if(f)
  {
    if(!f->draws.empty())
      for(const auto& c : f->draws)
        recs.push_back(
            {c.index_or_vertex_count, c.instance_count, c.first_index_or_vertex,
             indexed ? c.base_vertex : 0, c.first_instance});
    else
      recs.push_back(
          {uint32_t(indexed ? f->indices.size() : f->vertices), uint32_t(f->instances), 0,
           0, 0});
    if(f->gpuDrawCount >= 0)
      recs.resize(std::min<std::size_t>(recs.size(), f->gpuDrawCount));
  }
  const auto stored = [&](const Rec& r, uint32_t k) {
    const uint32_t i = r.first + k;
    return i < f->indices.size() ? f->indices[i] : invalid;
  };
  const auto element = [&](const Rec& r, uint32_t k) -> uint32_t {
    if(k >= r.count)
      return invalid;
    if(!indexed)
      return r.first + k;
    const uint32_t v = stored(r, k);
    if(v == invalid)
      return invalid;
    const int64_t vertex = int64_t(v) + r.base;
    return vertex < 0 ? invalid : uint32_t(vertex);
  };
  const auto triangles = [&](const Rec& r) -> uint32_t {
    if(!f || f->topology == ossia::geometry::triangles)
      return r.count / 3;
    if(f->topology == ossia::geometry::triangle_strip
       || f->topology == ossia::geometry::triangle_fan)
      return r.count >= 3 ? r.count - 2 : 0;
    return 0;
  };
  const auto indexOf = [&](uint32_t e) {
    for(const auto& r : recs)
    {
      if(e < r.count)
        return element(r, e);
      e -= r.count;
    }
    return invalid;
  };
  const auto triangle = [&](uint32_t t, uint32_t c) -> uint32_t {
    for(const auto& r : recs)
    {
      const uint32_t n = triangles(r);
      if(t >= n)
      {
        t -= n;
        continue;
      }
      if(f->topology == ossia::geometry::triangles)
        return element(r, t * 3 + c);
      const bool fan = f->topology == ossia::geometry::triangle_fan;
      if(indexed)
      {
        const uint32_t restart = f->width == 2 ? 0xFFFFu : 0xFFFFFFFFu;
        if(stored(r, fan ? 0 : t) == restart || stored(r, t + 1) == restart
           || stored(r, t + 2) == restart)
          return invalid;
      }
      if(fan)
        return element(r, c == 0 ? 0 : t + c);
      return element(r, t + ((t & 1) && c < 2 ? 1 - c : c));
    }
    return invalid;
  };
  uint32_t elements = 0, tris = 0, insts = 0;
  for(const auto& r : recs)
  {
    elements += r.count;
    tris += triangles(r);
    insts += r.instances;
  }
  const auto instance = [&](uint32_t i) {
    for(const auto& r : recs)
    {
      if(i < r.instances)
        return r.firstInstance + i;
      i -= r.instances;
    }
    return invalid;
  };
  const uint32_t storedVertices = f ? f->vertices : 0;
  const auto vertex = [&](uint32_t i) {
    if(indexed)
      return i < storedVertices ? i : invalid;
    return indexOf(i);
  };

  const auto enc = [](uint32_t v) { return v == invalid ? 0 : int(std::min(v + 1u, 255u)); };
  Grid g{};
  for(uint32_t x = 0; x < 16; x++)
  {
    g[0][x] = enc(indexOf(x));
    g[1][x] = enc(f ? triangle(x / 3, x % 3) : invalid);
    g[2][x] = enc(instance(x));
    g[3][x] = enc(vertex(x));
  }
  const uint32_t counts[]{
      elements, tris, insts, indexed ? storedVertices : elements, f ? 1u : 0u,
      f && f->weight ? 1u : 0u, 0u};
  for(int x = 0; x < 16; x++)
    g[4][x] = enc(x < 7 ? counts[x] : invalid);
  return g;
}

struct Shot
{
  bool skipped{};
  std::string error;
  ReadbackImage img;
};

Shot run(score::gfx::GraphicsApi api, const Fixture* f)
{
  Shot r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int probe = p.addCsf(corpus("csf-runtime-probe.cs"));
    const int sink = p.addSink({16, 8});
    if(probe < 0)
    {
      r.error = "build failed: " + p.error();
      return;
    }
    if(f)
    {
      const int src = p.addNode(std::make_unique<SourceNode>(*f));
      p.wire(p.nodeGeometryOut(src, 0), p.geometryIn(probe, 0));
    }
    p.wire(p.imageOut(probe, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      r.skipped = p.skipped();
      r.error = r.skipped ? std::string{} : p.error();
      return;
    }
    p.render(3);
    r.img = p.readback(sink);
  });
  return r;
}

void check(const Fixture* f)
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  const Shot r = run(api, f);
  if(r.skipped)
    SKIP("backend unavailable");
  INFO(r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.img.valid());
  REQUIRE(r.img.width == 16);
  REQUIRE(r.img.height == 8);
  const Grid want = reference(f);
  int wrong = 0;
  for(int y = 0; y < 5; y++)
    for(int x = 0; x < 16; x++)
    {
      const int got = r.img.at(x, y)[0];
      if(got != want[y][x])
      {
        if(wrong++ < 12)
          UNSCOPED_INFO("row " << y << " x " << x << ": " << got << ", expected " << want[y][x]);
      }
    }
  CHECK(wrong == 0);
}

Fixture indexedList(int width)
{
  Fixture f;
  f.vertices = 6;
  f.width = width;
  f.indices = {5, 4, 3, 2, 1, 0, 0, 2, 4};
  f.leading = 1;
  f.draws = {
      {.index_or_vertex_count = 3, .instance_count = 2, .first_index_or_vertex = 0,
       .base_vertex = 1, .first_instance = 3},
      {.index_or_vertex_count = 6, .instance_count = 1, .first_index_or_vertex = 3,
       .base_vertex = -1, .first_instance = 0}};
  return f;
}
}

TEST_CASE("RUNTIME: a non-indexed triangle list", "[gfx][csf][geometry][runtime]")
{
  Fixture f;
  f.vertices = 5;
  f.weight = true;
  check(&f);
}

TEST_CASE("RUNTIME: uint16 indices at an odd offset, two draws", "[gfx][csf][geometry][runtime]")
{
  const Fixture f = indexedList(2);
  check(&f);
}

TEST_CASE("RUNTIME: uint32 indices at an offset, two draws", "[gfx][csf][geometry][runtime]")
{
  const Fixture f = indexedList(4);
  check(&f);
}

TEST_CASE("RUNTIME: indices on the GPU, bound in place", "[gfx][csf][geometry][runtime]")
{
  Fixture f = indexedList(2);
  f.gpu = true;
  check(&f);
}

TEST_CASE(
    "RUNTIME: GPU indices the shader cannot bind are copied",
    "[gfx][csf][geometry][runtime]")
{
  Fixture f = indexedList(4);
  f.gpu = true;
  f.indexStorage = false;
  check(&f);
}

TEST_CASE("RUNTIME: a uint16 strip with a restart index", "[gfx][csf][geometry][runtime]")
{
  Fixture f;
  f.vertices = 6;
  f.topology = ossia::geometry::triangle_strip;
  f.width = 2;
  f.indices = {0, 1, 2, 0xFFFF, 3, 4, 5};
  check(&f);
}

TEST_CASE("RUNTIME: a uint32 fan", "[gfx][csf][geometry][runtime]")
{
  Fixture f;
  f.vertices = 5;
  f.topology = ossia::geometry::triangle_fan;
  f.width = 4;
  f.indices = {4, 0, 1, 2, 3};
  check(&f);
}

TEST_CASE(
    "RUNTIME: GPU draw records cut by a GPU draw count", "[gfx][csf][geometry][runtime]")
{
  Fixture f;
  f.vertices = 9;
  f.gpu = true;
  f.gpuDraws = true;
  f.gpuDrawCount = 2;
  f.draws = {
      {.index_or_vertex_count = 3, .instance_count = 1, .first_index_or_vertex = 0,
       .first_instance = 0},
      {.index_or_vertex_count = 3, .instance_count = 2, .first_index_or_vertex = 4,
       .first_instance = 5},
      {.index_or_vertex_count = 2, .instance_count = 1, .first_index_or_vertex = 7,
       .first_instance = 1}};
  check(&f);
}

TEST_CASE("RUNTIME: indexed GPU draw records", "[gfx][csf][geometry][runtime]")
{
  Fixture f = indexedList(2);
  f.gpu = true;
  f.gpuDraws = true;
  check(&f);
}

TEST_CASE("RUNTIME: zero instances", "[gfx][csf][geometry][runtime]")
{
  Fixture f;
  f.vertices = 3;
  f.instances = 0;
  check(&f);
}

TEST_CASE("RUNTIME: points have no triangles", "[gfx][csf][geometry][runtime]")
{
  Fixture f;
  f.vertices = 7;
  f.topology = ossia::geometry::points;
  check(&f);
}

TEST_CASE("RUNTIME: no upstream", "[gfx][csf][geometry][runtime]")
{
  check(nullptr);
}
