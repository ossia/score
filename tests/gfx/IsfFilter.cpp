// =============================================================================
// L3 ISF feature surface — SAMPLER FILTERING.
//
// isf-nearest-filter.fs: pass 0 writes a 4x4 checkerboard into a small 32x32
// TARGET declared FILTER:"NEAREST"; pass 1 samples it up to the 64x64 output.
// With NEAREST the up-sampled checker has HARD edges — every output pixel is
// (almost) pure red (255,51,0) or pure blue (0,51,255). With LINEAR it would
// blur to purple (R and B both mid) along the cell boundaries. So we assert the
// image is a hard 2-colour checker: (almost) no "purple" (both R>64 and B>64)
// pixels, on every RHI backend, and the readback agrees across backends.
//
// Run: DISPLAY=:0 ctest -R gfx --output-on-failure
// =============================================================================
#include "IsfTestCommon.hpp"

using namespace score::test::gfx;
using namespace score::test::gfx::isf;

TEST_CASE("isf-nearest-filter produces a hard-edged checker", "[gfx][l3][isf][filter]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  const IsfResult r = render(backend, {corpus("isf-nearest-filter.fs")});
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  INFO("backend=" << r.backend);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 1);
  const auto& img = r.outputs[0];
  REQUIRE(img.valid());

  int red = 0, blue = 0, purple = 0, other = 0;
  for(int y = 0; y < img.height; ++y)
    for(int x = 0; x < img.width; ++x)
    {
      const auto p = img.at(x, y);
      const bool rHi = p[0] > 180, rLo = p[0] < 75;
      const bool bHi = p[2] > 180, bLo = p[2] < 75;
      if(rHi && bLo)
        ++red;
      else if(rLo && bHi)
        ++blue;
      else if(p[0] > 64 && p[2] > 64)
        ++purple; // a blended (LINEAR-like) boundary pixel
      else
        ++other;
    }
  const int total = img.width * img.height;
  INFO("red=" << red << " blue=" << blue << " purple=" << purple << " other="
              << other << " / " << total);

  // Both checker colours are well represented (real checkerboard, not a flat
  // fill), and blended/purple pixels are rare — that is the NEAREST signature.
  CHECK(red > total / 8);
  CHECK(blue > total / 8);
  CHECK(purple <= total / 20); // <=5% blended => hard edges, not LINEAR
}

TEST_CASE("isf-nearest-filter agrees across backends", "[gfx][l3][isf][filter]")
{
  const auto shots = render_all({corpus("isf-nearest-filter.fs")});
  std::vector<ReadbackImage> got;
  for(const auto& s : shots)
    if(!s.result.skipped && s.result.error.empty() && !s.result.outputs.empty()
       && s.result.outputs[0].valid())
      got.push_back(s.result.outputs[0]);
  if(got.size() < 2)
    SKIP("need >=2 live backends to compare");
  for(std::size_t i = 1; i < got.size(); ++i)
  {
    const int d = max_channel_diff(got[0], got[i]);
    INFO("max channel diff vs backend0 = " << d);
    CHECK(d <= 4);
  }
}
