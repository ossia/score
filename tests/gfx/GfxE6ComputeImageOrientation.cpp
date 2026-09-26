// How a consumer reads a compute-written image, and an ISF-rendered one, once
// it has crossed a texture outlet. Sources: csf-orient-store.cs (a green ramp
// written through IMG_STORE, 0 at the top) and isf-gradient-y.fs (255 at the
// top).
//
//   * sampled reads (an ISF's IMG_NORM_PIXEL, a raw raster's IMG_PIXEL at
//     gl_FragCoord) keep the picture the right way up on every backend;
//   * IMG_TEXEL, the integer read, does too: at gl_FragCoord in an ISF and in
//     a raw raster, and at the invocation index in a CSF. Like IMG_PIXEL it
//     returns straight colour from a cabled image (a3alpha-straight-red.fs,
//     red at alpha 0.5, read back through fixc-opaque-view.fs);
//   * a raw raster's bare texelFetch at gl_FragCoord indexes memory rows. The
//     engine rewrites gl_FragCoord to a bottom-left origin on Vulkan only, so
//     that read is mirrored on Vulkan and upright elsewhere; pinned as is.
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

enum class Expect
{
  Upright,
  MirroredOnVulkan
};

void check_consumer(const char* consumer, Expect expect = Expect::Upright)
{
  const auto api = GENERATE(from_range(platform_backends()));
  const char* source = GENERATE("csf-orient-store.cs", "isf-gradient-y.fs");
  const bool mirrored = expect == Expect::MirroredOnVulkan && api == score::gfx::Vulkan;
  const bool topIsZero = QString{source}.endsWith(".cs") != mirrored;
  CAPTURE(backend_name(api), source, consumer);

  IsfResult r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = QString{consumer}.startsWith("e6-orient-")
            ? render_through_raster(api, source, consumer)
            : render_isf_chain(api, {corpus(source), corpus(consumer)}, {64, 64}, 3);
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
    "E6: IMG_TEXEL reads with IMG_PIXEL's row convention in ISF, raw raster and CSF",
    "[gfx][csf][orientation][e6]")
{
  check_consumer(GENERATE(
      "isf-e6-orient-imgtexel.fs", "e6-orient-imgtexel.fs", "csf-e6-orient-imgtexel.cs"));
}

TEST_CASE(
    "E6: a raw raster texelFetch at gl_FragCoord reads memory rows",
    "[gfx][csf][orientation][e6]")
{
  check_consumer("e6-orient-texelfetch.fs", Expect::MirroredOnVulkan);
}

TEST_CASE("E6: IMG_TEXEL returns straight colour like IMG_PIXEL", "[gfx][isf][alpha][e6]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  IsfResult r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = render_isf_chain(
        api,
        {corpus("a3alpha-straight-red.fs"), corpus("isf-e6-orient-imgtexel.fs"),
         corpus("fixc-opaque-view.fs")},
        {64, 64}, 3);
  });
  if(r.skipped)
    SKIP("backend unavailable: " << r.skip_reason);
  INFO("error=" << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 1);
  REQUIRE(r.outputs[0].valid());
  const auto rgb = r.outputs[0].at(16, 32);
  const auto alpha = r.outputs[0].at(48, 32);
  INFO("rgb=" << int(rgb[0]) << "," << int(rgb[1]) << "," << int(rgb[2])
              << " alpha=" << int(alpha[0]));
  CHECK(std::abs(int(rgb[0]) - 128) <= 3);
  CHECK(int(rgb[1]) <= 3);
  CHECK(int(rgb[2]) <= 3);
  CHECK(std::abs(int(alpha[0]) - 128) <= 3);
}
