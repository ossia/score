// CSF / ISF engine fixes from agent C (N55, N56, N57, N59, N60, N62).
//
//   * N55: a CSF attribute declared with SEMANTIC "custom" is published under
//     its NAME, so a downstream CSF reading it by name sees the data.
//   * N60: a non-standard SEMANTIC ("face_mask") matches the upstream attribute
//     published under that semantic, whatever the two NAMEs are.
//   * N56: a COPY_FROM attribute on a geometry that also declares an AUXILIARY
//     buffer still carries the forwarded data downstream.
//   * N57: a CSF storage-image outlet reaches its consumer unblended.
//   * N59: a read_write CSF modifier on a static CPU mesh gives the same result
//     every frame instead of accumulating.
//   * N62: FRAMEINDEX is 0 on the first rendered frame (ISF and CSF).
//   * N67: a CSF audio input is bound and uploaded.
//   * N53: ISF pass sizes can use $WIDTH_<input> / $HEIGHT_<input>.
//   * N96 (part): a flexible-array storage AUXILIARY with no producer gets a
//     fallback buffer of at least one element.
//   * N69: a delayed self-cable on a 3D CSF output reads a snapshot, not the
//     storage image the same pass writes.
//
//   DISPLAY=:0 SCORE_GPU_VALIDATION=0 ctest -R gfx_csf_isf_fixc
#include "IsfTestCommon.hpp"

#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderedCSFNode.hpp>
#include <Gfx/Graph/RenderList.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <cstring>

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

Shot render_producer_consumer(
    score::gfx::GraphicsApi be, const char* producer, const char* middle,
    const char* consumer)
{
  return render_pipeline(be, 3, [&](GfxPipeline& p) {
    const int prod = p.addCsf(corpus(producer));
    const int mid = middle ? p.addCsf(corpus(middle)) : -1;
    const int cons = p.addCsf(corpus(consumer));
    const int sink = p.addSink({64, 64});
    if(prod < 0 || cons < 0 || (middle && mid < 0))
      return -1;
    if(middle)
    {
      p.wire(p.geometryOut(prod, 0), p.geometryIn(mid, 0));
      p.wire(p.geometryOut(mid, 0), p.geometryIn(cons, 0));
    }
    else
    {
      p.wire(p.geometryOut(prod, 0), p.geometryIn(cons, 0));
    }
    p.wire(p.imageOut(cons, 0), p.sinkInput(sink));
    return sink;
  });
}

#define FIXC_REQUIRE_LIVE(s, backend)                                         \
  if((s).skipped)                                                            \
    SKIP((s).backend + ": " + (s).skip_reason);                              \
  if(const char* why = compute_shader_skip_reason(backend))                  \
    SKIP(std::string{backend_name(backend)} + ": " + why);                   \
  CAPTURE((s).backend);                                                      \
  REQUIRE((s).error.empty());                                                \
  REQUIRE((s).image.valid())

// A static CPU mesh: one triangle, position float3 tightly packed (stride 12,
// the GPU-scatter path) and color float4 (stride 16, the direct-upload path).
struct CpuTriangleNode final : score::gfx::ProcessNode
{
  CpuTriangleNode()
  {
    output.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Geometry, {}});
  }
  score::gfx::NodeRenderer* createRenderer(score::gfx::RenderList&) const
      noexcept override;
};

struct CpuTriangleRenderer final : score::gfx::NodeRenderer
{
  ossia::geometry_spec m_spec;

  explicit CpuTriangleRenderer(const CpuTriangleNode& n)
      : NodeRenderer{n}
  {
  }

  void init(score::gfx::RenderList&, QRhiResourceUpdateBatch&) override
  {
    static const float pos[9] = {-1.f, -1.f, 0.f, -0.5f, -1.f, 0.f, -1.f, 1.f, 0.f};
    static const float col[12]
        = {0.f, 1.f, 0.f, 1.f, 0.f, 1.f, 0.f, 1.f, 0.f, 1.f, 0.f, 1.f};

    auto posData = std::shared_ptr<void>(new char[sizeof(pos)], [](void* p) {
      delete[] static_cast<char*>(p);
    });
    std::memcpy(posData.get(), pos, sizeof(pos));
    auto colData = std::shared_ptr<void>(new char[sizeof(col)], [](void* p) {
      delete[] static_cast<char*>(p);
    });
    std::memcpy(colData.get(), col, sizeof(col));

    ossia::geometry g;
    g.vertices = 3;
    g.topology = ossia::geometry::triangles;
    g.cull_mode = ossia::geometry::none;
    g.front_face = ossia::geometry::counter_clockwise;
    g.buffers.push_back({.data = ossia::geometry::cpu_buffer{posData, sizeof(pos)}});
    g.buffers.push_back({.data = ossia::geometry::cpu_buffer{colData, sizeof(col)}});
    g.bindings.push_back({.byte_stride = 12});
    g.bindings.push_back({.byte_stride = 16});
    ossia::geometry::attribute a_pos;
    a_pos.binding = 0;
    a_pos.location = 0;
    a_pos.format = ossia::geometry::attribute::float3;
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
    if(!m_spec.meshes || !sink || !sink->node)
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
  void release(score::gfx::RenderList&) override
  {
    m_spec = {};
    m_initialized = false;
  }
};

score::gfx::NodeRenderer*
CpuTriangleNode::createRenderer(score::gfx::RenderList&) const noexcept
{
  return new CpuTriangleRenderer{*this};
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
    "N55: a SEMANTIC custom attribute is published under its NAME",
    "[gfx][csf][geometry][fixc]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  const Shot s = render_producer_consumer(
      backend, "fixc-attr-custom-producer.cs", nullptr, "fixc-attr-foo-consumer.cs");
  FIXC_REQUIRE_LIVE(s, backend);
  const auto c = s.image.center();
  INFO("centre = " << int(c[0]) << "," << int(c[1]) << "," << int(c[2]));
  CHECK(near(c, {0, 255, 0, 255}, 8));
}

TEST_CASE(
    "N60: a non-standard SEMANTIC matches the upstream attribute of that name",
    "[gfx][csf][geometry][fixc]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  const Shot s = render_producer_consumer(
      backend, "fixc-attr-mask-producer.cs", nullptr, "fixc-attr-mask-consumer.cs");
  FIXC_REQUIRE_LIVE(s, backend);
  const auto c = s.image.center();
  INFO("centre = " << int(c[0]) << "," << int(c[1]) << "," << int(c[2]));
  CHECK(near(c, {0, 255, 0, 255}, 8));
}

TEST_CASE(
    "N56: COPY_FROM next to an AUXILIARY keeps the forwarded attribute",
    "[gfx][csf][geometry][fixc]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  const Shot s = render_producer_consumer(
      backend, "fixc-attr-foo-producer.cs", "fixc-copyfrom-aux-middle.cs",
      "fixc-attr-foo-consumer.cs");
  FIXC_REQUIRE_LIVE(s, backend);
  const auto c = s.image.center();
  INFO("centre = " << int(c[0]) << "," << int(c[1]) << "," << int(c[2]));
  CHECK(near(c, {0, 255, 0, 255}, 8));
}

TEST_CASE(
    "N57: a CSF storage-image outlet reaches the consumer unblended",
    "[gfx][csf][image][fixc]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  const Shot s = render_pipeline(backend, 3, [](GfxPipeline& p) {
    const int n = p.addCsf(corpus("fixc-image-alpha.cs"));
    const int view = p.addIsf(corpus("fixc-opaque-view.fs"));
    const int sink = p.addSink({64, 64});
    if(n < 0 || view < 0)
      return -1;
    p.wire(p.imageOut(n, 0), p.imageIn(view, 0));
    p.wire(p.imageOut(view, 0), p.sinkInput(sink));
    return sink;
  });
  FIXC_REQUIRE_LIVE(s, backend);
  const auto rgb = s.image.at(16, 32);
  const auto alpha = s.image.at(48, 32);
  INFO("rgb = " << int(rgb[0]) << "," << int(rgb[1]) << "," << int(rgb[2]));
  INFO("alpha = " << int(alpha[0]));
  CHECK(near(rgb, {255, 128, 64, 255}, 3));
  CHECK(near(alpha, {128, 128, 128, 255}, 3));
}

TEST_CASE(
    "N59: a read_write modifier on a static CPU mesh does not accumulate",
    "[gfx][csf][geometry][fixc]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  const auto build = [](GfxPipeline& p) {
    const int mesh = p.addNode(std::make_unique<CpuTriangleNode>());
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
  FIXC_REQUIRE_LIVE(early, backend);
  REQUIRE(late.error.empty());
  REQUIRE(late.image.valid());

  const int litEarly = lit_pixels(early.image);
  const int litLate = lit_pixels(late.image);
  CAPTURE(litEarly, litLate);
  REQUIRE(litEarly > 50);

  CHECK(litEarly == litLate);
  CHECK(identical(early.image, late.image));

  // One application of the offset: colour (0.25, 1, 0).
  int once = 0;
  for(int y = 0; y < late.image.height; ++y)
    for(int x = 0; x < late.image.width; ++x)
      if(near(late.image.at(x, y), {64, 255, 0, 255}, 8))
        ++once;
  CAPTURE(once);
  CHECK(once * 10 > litLate * 9);
}

TEST_CASE("N62: ISF FRAMEINDEX is 0 on the first frame", "[gfx][isf][uniforms][fixc]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  for(const int frames : {1, 4})
  {
    CAPTURE(frames);
    IsfResult r;
    score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
      r = score::test::gfx::render_isf_chain(
          backend, {corpus("fixc-frameindex.fs")}, {64, 64}, frames);
    });
    if(r.skipped)
      SKIP(r.backend + ": " + r.skip_reason);
    REQUIRE(r.error.empty());
    REQUIRE(!r.outputs.empty());
    REQUIRE(r.outputs[0].valid());
    const auto c = r.outputs[0].center();
    CAPTURE(int(c[0]));
    CHECK(int(c[0]) == frames - 1);
  }
}

TEST_CASE("N62: CSF FRAMEINDEX is 0 on the first frame", "[gfx][csf][uniforms][fixc]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  for(const int frames : {1, 4})
  {
    CAPTURE(frames);
    const Shot s = render_pipeline(backend, frames, [](GfxPipeline& p) {
      const int n = p.addCsf(corpus("fixc-frameindex.cs"));
      const int sink = p.addSink({64, 64});
      if(n < 0)
        return -1;
      p.wire(p.imageOut(n, 0), p.sinkInput(sink));
      return sink;
    });
    FIXC_REQUIRE_LIVE(s, backend);
    const auto c = s.image.center();
    CAPTURE(int(c[0]));
    CHECK(int(c[0]) == frames - 1);
  }
}

TEST_CASE("N67: a CSF samples its audio input", "[gfx][csf][audio][fixc]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  Shot s;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int n = p.addCsf(corpus("fixc-csf-audio.cs"));
    const int sink = p.addSink({64, 64});
    if(n < 0)
    {
      s.error = p.error();
      return;
    }
    p.wire(p.imageOut(n, 0), p.sinkInput(sink));
    if(!p.create(backend))
    {
      s.skipped = p.skipped();
      s.skip_reason = p.skipReason();
      s.backend = p.backend();
      s.error = p.error();
      return;
    }
    s.backend = p.backend();
    for(int f = 0; f < 3; ++f)
    {
      setAudio(*p.isf(n), 0, const_audio(0.5, 256));
      p.render(1);
    }
    s.image = p.readback(sink);
    if(s.error.empty())
      s.error = p.error();
  });
  FIXC_REQUIRE_LIVE(s, backend);
  const auto c = s.image.center();
  INFO("centre = " << int(c[0]) << "," << int(c[1]) << "," << int(c[2]));
  CHECK(near(c, {191, 0, 255, 255}, 4));
}

TEST_CASE(
    "N53: ISF pass sizes resolve $WIDTH_<input> and $HEIGHT_<input>",
    "[gfx][isf][passes][fixc]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  IsfResult r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = score::test::gfx::render_isf_chain(
        backend, {corpus("isf-solid-color.fs"), corpus("fixc-pass-size.fs")}, {64, 64},
        3);
  });
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  REQUIRE(r.error.empty());
  REQUIRE(!r.outputs.empty());
  REQUIRE(r.outputs[0].valid());
  const auto c = r.outputs[0].center();
  INFO("ratio = " << int(c[0]) << "," << int(c[1]));
  CHECK(near(c, {128, 128, 0, 255}, 2));
}

TEST_CASE(
    "N69: a delayed self-cable reads a snapshot of the written storage image",
    "[gfx][csf][feedback][fixc]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  Shot s;
  bool distinct = false;
  bool checked = false;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int n = p.addCsf(corpus("fixc-self-feedback3d.cs"));
    const int sink = p.addSink({64, 64});
    if(n < 0)
    {
      s.error = p.error();
      return;
    }
    p.wire(p.imageOut(n, 0), p.sinkInput(sink));
    p.wireFeedback(p.imageOut(n, 0), p.imageIn(n, 0));
    if(!p.create(backend))
    {
      s.skipped = p.skipped();
      s.skip_reason = p.skipReason();
      s.backend = p.backend();
      s.error = p.error();
      return;
    }
    s.backend = p.backend();
    p.render(4);
    s.image = p.readback(sink);
    if(s.error.empty())
      s.error = p.error();

    for(auto& [rl, rn] : p.isf(n)->renderedNodes)
    {
      auto* csf = dynamic_cast<score::gfx::RenderedCSFNode*>(rn);
      if(!csf)
        continue;
      auto* written = csf->textureForOutput(*p.imageOut(n, 0));
      const auto samplers = csf->allSamplers();
      if(written && !samplers.empty())
      {
        checked = true;
        distinct = samplers[0].texture && samplers[0].texture != written;
      }
    }
  });
  FIXC_REQUIRE_LIVE(s, backend);
  REQUIRE(checked);
  CHECK(distinct);
  const auto c = s.image.center();
  INFO("centre = " << int(c[0]));
  CHECK(near(c, {64, 0, 0, 255}, 2));
}

TEST_CASE(
    "N96: an unprovided flexible-array storage auxiliary holds one element",
    "[gfx][csf][geometry][fixc]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  const bool withProducer = GENERATE(false, true);
  CAPTURE(backend_name(backend), withProducer);
  const Shot s = render_pipeline(backend, 3, [&](GfxPipeline& p) {
    const int prod = withProducer ? p.addCsf(corpus("fixc-attr-foo-producer.cs")) : -1;
    const int n = p.addCsf(corpus("fixc-aux-fallback.cs"));
    const int sink = p.addSink({64, 64});
    if(n < 0 || (withProducer && prod < 0))
      return -1;
    if(withProducer)
      p.wire(p.geometryOut(prod, 0), p.geometryIn(n, 0));
    p.wire(p.imageOut(n, 0), p.sinkInput(sink));
    return sink;
  });
  FIXC_REQUIRE_LIVE(s, backend);
  const auto c = s.image.center();
  INFO("centre = " << int(c[0]) << "," << int(c[1]) << "," << int(c[2]));
  CHECK(near(c, {0, 255, 0, 255}, 8));
}

TEST_CASE(
    "N62: single-pass ISF, VSA and raw raster FRAMEINDEX start at 0",
    "[gfx][isf][vsa][raster][uniforms][fixc]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  const int kind = GENERATE(0, 1, 2);
  CAPTURE(backend_name(backend), kind);
  for(const int frames : {1, 4})
  {
    CAPTURE(frames);
    IsfResult r;
    score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
      switch(kind)
      {
        case 0:
          r = score::test::gfx::render_isf_chain(
              backend, {corpus("fixc-frameindex-simple.fs")}, {64, 64}, frames);
          break;
        case 1:
          r = score::test::gfx::render_vsa(
              backend, corpus("fixc-frameindex-vsa.vs"), {64, 64}, frames);
          break;
        default:
          r = score::test::gfx::render_raster(
              backend, {}, corpus("fixc-frameindex-rr.vs"),
              corpus("fixc-frameindex-rr.fs"), {64, 64}, frames);
          break;
      }
    });
    if(r.skipped)
      SKIP(r.backend + ": " + r.skip_reason);
    REQUIRE(r.error.empty());
    REQUIRE(!r.outputs.empty());
    REQUIRE(r.outputs[0].valid());
    const auto c = r.outputs[0].center();
    CAPTURE(int(c[0]));
    CHECK(int(c[0]) == frames - 1);
  }
}

TEST_CASE(
    "N60: a raw raster VERTEX_INPUT with a custom SEMANTIC finds that attribute",
    "[gfx][raster][geometry][fixc]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  IsfResult r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = score::test::gfx::render_raster(
        backend, {corpus("fixc-raster-mask-producer.cs")}, corpus("fixc-raster-mask.vs"),
        corpus("fixc-raster-mask.fs"));
  });
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  if(const char* why = compute_shader_skip_reason(backend))
    SKIP(std::string{backend_name(backend)} + ": " + why);
  REQUIRE(r.error.empty());
  REQUIRE(!r.outputs.empty());
  REQUIRE(r.outputs[0].valid());
  const auto c = r.outputs[0].center();
  INFO("centre = " << int(c[0]) << "," << int(c[1]) << "," << int(c[2]));
  CHECK(near(c, {0, 255, 0, 255}, 8));
}
