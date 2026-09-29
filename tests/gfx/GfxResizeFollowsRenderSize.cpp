// A resize of the output rebuilds only what follows its size, but all of it:
// the output uniforms carry the new render size, and a generator whose own
// targets are allocated at the render size (ISF and CSF storage images, the
// storage images of a CSF geometry input, MRT attachments without an OUTPUT
// size) reallocates them, while one whose OUTPUTs give their size keeps it.
#include "IsfTestCommon.hpp"

#include <Gfx/Graph/ISFNode.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>

#include <cstdlib>

using namespace score::test::gfx;
using score::test::gfx::isf::corpus;

namespace
{
struct Sizes
{
  bool skipped{};
  std::string skip, error;
  QSize before, after;
  ReadbackImage a, b;
};

QSize outputTextureSize(score::gfx::Node& node)
{
  for(auto& [rl, r] : node.renderedNodes)
    for(auto* out : node.output)
      if(out->type == score::gfx::Types::Image)
        if(auto* tex = r->textureForOutput(*out))
          return tex->pixelSize();
  return {};
}

// Builds `producer -> sink (64x64)`, renders, resizes the sink to 96x80 and
// renders again. `add` returns the producer's index in the pipeline.
template <typename Add>
Sizes run(score::gfx::GraphicsApi api, Add add)
{
  Sizes s;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int src = add(p);
    if(src < 0)
    {
      s.error = p.error().empty() ? "node build failed" : p.error();
      return;
    }
    const int sink = p.addSink({64, 64});
    p.wire(p.imageOut(src, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      s.skipped = p.skipped();
      s.skip = p.skipReason();
      s.error = s.skipped ? std::string{} : p.error();
      return;
    }
    p.render(3);
    s.a = p.readback(sink);
    s.before = outputTextureSize(*p.isf(src));

    p.resizeSink(sink, {96, 80});
    p.render(3);
    s.b = p.readback(sink);
    s.after = outputTextureSize(*p.isf(src));
    if(s.error.empty())
      s.error = p.error();
  });
  return s;
}

int addGeometryDrivenRaster(GfxPipeline& p, const char* vs, const char* fs)
{
  const int geo = p.addIsf(corpus("syn-geo-producer.cs"));
  const int raster = p.addRaster(corpus(vs), corpus(fs));
  if(geo < 0 || raster < 0)
    return -1;
  p.wire(p.geometryOut(geo, 0), p.geometryIn(raster, 0));
  return raster;
}

// The picture paints a size in R and G, in pixels out of 255.
void checkPaintedSize(const Sizes& s)
{
  INFO("error=" << s.error);
  REQUIRE(s.error.empty());
  REQUIRE(s.a.valid());
  REQUIRE(s.b.valid());
  REQUIRE(s.b.width == 96);
  REQUIRE(s.b.height == 80);

  const auto before = s.a.center();
  const auto after = s.b.center();
  INFO("before " << int(before[0]) << "x" << int(before[1]) << ", after "
                 << int(after[0]) << "x" << int(after[1]));
  CHECK(std::abs(int(before[0]) - 64) <= 1);
  CHECK(std::abs(int(before[1]) - 64) <= 1);
  CHECK(std::abs(int(after[0]) - 96) <= 1);
  CHECK(std::abs(int(after[1]) - 80) <= 1);
}
}

TEST_CASE(
    "a resize of the output hands its new size to the output uniforms",
    "[gfx][resize]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const auto s
      = run(api, [](GfxPipeline& p) { return p.addIsf(corpus("isf-output-rendersize.fs")); });
  if(s.skipped)
    SKIP(s.skip);
  checkPaintedSize(s);
}

TEST_CASE(
    "a resize of the output reallocates the storage images at the render size",
    "[gfx][resize][storage]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const auto s
      = run(api, [](GfxPipeline& p) { return p.addIsf(corpus("isf-storage-image-size.fs")); });
  if(s.skipped)
    SKIP(s.skip);
  if(const char* why = storage_buffer_skip_reason(api))
    SKIP(why);
  checkPaintedSize(s);
}

TEST_CASE(
    "a resize of the output reallocates the CSF images at the render size",
    "[gfx][resize][csf]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  // A storage image, then the storage-image auxiliary of a geometry input.
  const auto* file
      = GENERATE("csf-image-rendersize-paint.cs", "csf-geo-aux-image-rendersize.cs");
  CAPTURE(backend_name(api), file);

  const auto s = run(api, [&](GfxPipeline& p) { return p.addCsf(corpus(file)); });
  if(s.skipped)
    SKIP(s.skip);
  if(const char* why = compute_shader_skip_reason(api))
    SKIP(why);
  checkPaintedSize(s);
}

TEST_CASE(
    "a resize of the output reallocates the MRT attachments that follow it",
    "[gfx][resize][mrt]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  SECTION("ISF")
  {
    const auto s
        = run(api, [](GfxPipeline& p) { return p.addIsf(corpus("isf-mrt-pattern.fs")); });
    if(s.skipped)
      SKIP(s.skip);
    INFO("error=" << s.error);
    REQUIRE(s.error.empty());
    CHECK(s.before == QSize(64, 64));
    CHECK(s.after == QSize(96, 80));
  }

  SECTION("raw raster")
  {
    const auto s = run(api, [](GfxPipeline& p) {
      return addGeometryDrivenRaster(p, "syn-rr-mrt-pattern.vs", "syn-rr-mrt-pattern.fs");
    });
    if(s.skipped)
      SKIP(s.skip);
    if(const char* why = compute_shader_skip_reason(api))
      SKIP(why);
    INFO("error=" << s.error);
    REQUIRE(s.error.empty());
    CHECK(s.before == QSize(64, 64));
    CHECK(s.after == QSize(96, 80));
  }

  SECTION("OUTPUTS with a size")
  {
    const auto s = run(
        api, [](GfxPipeline& p) { return p.addIsf(corpus("RenderTargets-sized.fs")); });
    if(s.skipped)
      SKIP(s.skip);
    INFO("error=" << s.error);
    REQUIRE(s.error.empty());
    CHECK(s.before == QSize(17, 9));
    CHECK(s.after == QSize(17, 9));
  }
}
