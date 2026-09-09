// =============================================================================
// The soft-edge blend ramp of the multi-window output.
//
// The ramp coordinate leaves [0;1] whenever the fragment centre of an edge
// pixel falls outside the triangle, which multisampling makes routine. The
// expression called pow() on the resulting negative base -- undefined in GLSL
// -- and the drivers that answer NaN write a fully lit pixel into the UNORM
// attachment: the one-pixel bright fringe that reappeared along a blended
// border once the rest of the edge had faded to black.
//
// The shader lives in MultiWindowNode.cpp, inside a renderer that needs one
// real platform surface per output and presents rather than reads back, so the
// expression itself is what is pinned here -- rendered on every backend, from
// a corpus shader that carries the same lines.
//
//   DISPLAY=:0 ctest -R gfx_soft_edge_blend
// =============================================================================

#include "IsfTestCommon.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

using namespace score::test::gfx;
using namespace score::test::gfx::isf;
using Catch::Approx;

TEST_CASE("the soft-edge ramp never lights the pixels past its own edge",
          "[gfx][window][blend]")
{
  for(auto& shot : render_all({corpus("isf-soft-edge-blend.fs")}, {128, 32}))
  {
    const auto& r = shot.result;
    INFO("backend " << int(shot.api));
    if(r.skipped)
      continue;
    REQUIRE(r.error.empty());
    REQUIRE(!r.outputs.empty());

    const auto& img = r.outputs[0];
    REQUIRE(img.width > 16);
    const int y = img.height / 2;

    // The ramp spans t in [-0.125; 1.125] with a width of 0.25, so the first
    // eighth of the image is outside the quad and the ramp reaches 1 at 3/8.
    const int outside = img.width / 8;

    for(int x = 0; x < outside; ++x)
    {
      INFO("column " << x << " of " << img.width);
      // Black, not white: this is the fringe.
      CHECK(int(img.at(x, y)[1]) <= 4);
    }

    // Not merely all-black: the ramp is there.
    CHECK(int(img.at(img.width - 2, y)[1]) >= 250);
    CHECK(int(img.at(img.width / 4, y)[1]) > 4);
    CHECK(int(img.at(img.width / 4, y)[1]) < 250);

    // Monotonic, and never above full.
    int prev = -1;
    for(int x = 0; x < img.width; ++x)
    {
      const int g = int(img.at(x, y)[1]);
      INFO("column " << x << " green " << g);
      CHECK(g <= 255);
      CHECK(g >= prev - 2);
      prev = g;
    }

    // The garbage detectors.
    CHECK(int(img.at(img.width / 2, y)[0]) == Approx(64).margin(3));
    CHECK(int(img.at(img.width / 2, y)[2]) == Approx(191).margin(3));
  }
}
