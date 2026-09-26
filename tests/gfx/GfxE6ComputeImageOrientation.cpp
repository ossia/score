// How a consumer reads a compute-written image, and an ISF-rendered one, once
// it has crossed a texture outlet. Sources: csf-orient-store.cs (a green ramp
// written through IMG_STORE, 0 at the top) and isf-gradient-y.fs (255 at the
// top).
//
//   * sampled reads (an ISF's IMG_NORM_PIXEL, a raw raster's IMG_PIXEL at
//     gl_FragCoord) keep the picture the right way up on every backend;
//   * [.e6-texelfetch], hidden: a raw raster's texelFetch at gl_FragCoord, the
//     read 3dgs.tile's 08_Composite makes of 07_TileRender's image. The engine
//     rewrites gl_FragCoord to a bottom-left origin on Vulkan while texelFetch
//     keeps the texture's memory rows, so the fetched picture is mirrored on
//     Vulkan only.
//
// Registration:
//   score_add_gfx_test(e6_compute_image_orientation GfxE6ComputeImageOrientation.cpp)
#include "IsfTestCommon.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>

using namespace score::test::gfx;
using namespace score::test::gfx::isf;

namespace
{
constexpr int kTol = 8;

int worst_ramp_error(const ReadbackImage& img, bool topIsZero, int& worstRow, int& got)
{
  int worst = 0;
  for(int y = 0; y < img.height; ++y)
  {
    const int row = topIsZero ? y : img.height - 1 - y;
    const int e = int(std::lround(255.0 * double(row) / double(img.height - 1)));
    for(int x : {1, img.width / 2, img.width - 2})
    {
      const int g = img.at(x, y)[1];
      if(std::abs(g - e) > worst)
      {
        worst = std::abs(g - e);
        worstRow = y;
        got = g;
      }
    }
  }
  return worst;
}

IsfResult render_through_raster(
    score::gfx::GraphicsApi api, const char* source, const char* consumerFs)
{
  IsfResult r;
  GfxPipeline p;
  const int src = p.addIsf(corpus(source));
  QString fs = corpus(consumerFs);
  QString vs = fs;
  vs.replace(vs.size() - 3, 3, ".vs");
  const int raster = p.addRaster(vs, fs);
  if(src < 0 || raster < 0)
  {
    r.error = p.error();
    return r;
  }
  const int sink = p.addSink({64, 64});
  p.wire(p.imageOut(src, 0), p.imageIn(raster, 0));
  p.wire(p.imageOut(raster, 0), p.sinkInput(sink));
  if(!p.create(api))
  {
    r.skipped = p.skipped();
    r.skip_reason = p.skipReason();
    r.error = r.skipped ? std::string{} : p.error();
    return r;
  }
  r.backend = p.backend();
  p.render(3);
  r.outputs.push_back(p.readback(sink));
  if(!r.outputs.back().valid())
    r.error = "empty readback";
  return r;
}

void check_consumer(const char* consumer)
{
  const auto api = GENERATE(from_range(platform_backends()));
  const char* source = GENERATE("csf-orient-store.cs", "isf-gradient-y.fs");
  const bool topIsZero = QString{source}.endsWith(".cs");
  CAPTURE(backend_name(api), source, consumer);

  IsfResult r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = QString{consumer}.endsWith(".fs") && QString{consumer}.startsWith("isf-")
            ? render_isf_chain(api, {corpus(source), corpus(consumer)}, {64, 64}, 3)
            : render_through_raster(api, source, consumer);
  });
  if(r.skipped)
    SKIP("backend unavailable: " << r.skip_reason);
  if(const char* why = compute_shader_skip_reason(api))
    SKIP(why);
  INFO("error=" << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 1);
  REQUIRE(r.outputs[0].valid());

  int row = 0, got = 0;
  const int worst = worst_ramp_error(r.outputs[0], topIsZero, row, got);
  INFO("worst row " << row << ": green=" << got);
  CHECK(worst <= kTol);
}
}

TEST_CASE(
    "E6: a sampled read of a compute-written image is the right way up",
    "[gfx][csf][orientation][e6]")
{
  check_consumer(GENERATE("isf-passthrough-plain.fs", "e6-orient-imgpixel.fs"));
}

TEST_CASE(
    "E6: a raw raster texelFetch at gl_FragCoord is the right way up",
    "[gfx][csf][orientation][.e6-texelfetch]")
{
  check_consumer("e6-orient-texelfetch.fs");
}
