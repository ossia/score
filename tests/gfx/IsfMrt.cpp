// =============================================================================
// L3 ISF feature surface — MULTIPLE RENDER TARGETS (MRT).
//
// Reads back EACH declared OUTPUTS attachment port separately and asserts each
// is correct + distinct, on every RHI backend. (mrt-single-color and
// isf-mrt-four-outputs are already asserted in Gfx.cpp; here we add the
// color+depth and G-buffer variants.)
//
// Corpus covered here:
//   * isf-mrt-depth-color.fs — OUTPUTS {color, depth}: two ports; color is a
//                              radial gradient (analytic centre/edge).
//   * mrt-gbuffer.fs         — OUTPUTS {color, normals, edges}: three ports off
//                              one image input.
//
// The MRT+PERSISTENT-SSBO shader (isf-mrt-persistent-ssbo.fs) is a KNOWN broken
// case (Vulkan segfault, OpenGL 2nd attachment blank) and lives in its own
// isolated target test_gfx_isf_mrt_persistent so its crash can't take this file
// down — see IsfMrtPersistent.cpp.
//
// Run: DISPLAY=:0 ctest -R gfx --output-on-failure
// =============================================================================
#include "IsfTestCommon.hpp"

using namespace score::test::gfx;
using namespace score::test::gfx::isf;

// -----------------------------------------------------------------------------
// isf-mrt-depth-color: two OUTPUTS ports (color + depth).
//   colorOut = radial gradient mix((1,.8,.2),(.1,.2,.8), 2*len(uv-.5)):
//     centre (uv=.5) -> (255,204,51);  corners -> ~(0,0,255) (clamped).
//   depthOut is a real depth attachment; reading it back as RGBA8 yields black
//   on both backends, so we assert the PORT EXISTS and note the depth-content
//   readback is not meaningful in RGBA8 (would need an R32F/depth readback).
// -----------------------------------------------------------------------------
TEST_CASE(
    "isf-mrt-depth-color exposes color+depth ports", "[gfx][l3][isf][mrt]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  const IsfResult r = render(backend, {corpus("isf-mrt-depth-color.fs")});
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  INFO("backend=" << r.backend);
  REQUIRE(r.error.empty());

  // color + depth => two image output ports.
  REQUIRE(r.outputs.size() == 2);
  REQUIRE(r.outputs[0].valid());
  REQUIRE(r.outputs[1].valid());

  // Attachment 0 = color gradient.
  const auto c = r.outputs[0].center();
  INFO("color centre = (" << (int)c[0] << "," << (int)c[1] << "," << (int)c[2] << ")");
  CHECK(near(c, {255, 204, 51, 255}, 10));               // analytic centre
  const auto corner = r.outputs[0].at(2, 2);
  CHECK(corner[2] > 220);                                // blue high at the edge
  CHECK(corner[0] < 30);                                 // red low at the edge

  // Attachment 1 = depth. Documented: reads back as black in RGBA8 on both
  // backends (depth attachment, not a colour). We only assert the port exists.
  const auto d = r.outputs[1].center();
  INFO("depth centre (RGBA8, expected ~black) = ("
       << (int)d[0] << "," << (int)d[1] << "," << (int)d[2] << ")");
}

TEST_CASE(
    "isf-mrt-depth-color color attachment agrees across backends",
    "[gfx][l3][isf][mrt]")
{
  const auto shots = render_all({corpus("isf-mrt-depth-color.fs")});
  std::vector<ReadbackImage> got;
  for(const auto& s : shots)
    if(!s.result.skipped && s.result.error.empty() && s.result.outputs.size() == 2)
      got.push_back(s.result.outputs[0]);
  if(got.size() < 2)
    SKIP("need >=2 live backends to compare");
  for(std::size_t i = 1; i < got.size(); ++i)
  {
    const int diff = max_channel_diff(got[0], got[i]);
    INFO("color attachment max channel diff = " << diff);
    CHECK(diff <= 4);
  }
}

// -----------------------------------------------------------------------------
// mrt-gbuffer: three OUTPUTS ports off one image input (a uv gradient producer).
//   colorOut   = passthrough of the input  -> RED tracks uv.x (X gradient).
//   normalsOut = screen-space normal of a smooth gradient -> ~flat (128,128,255).
//   edgesOut   = Sobel edge mask of a smooth gradient -> ~black (below threshold).
//
// NB the passthrough's GREEN channel (uv.y) shows a vertical-orientation
// difference between OpenGL and Vulkan through the linear-chain path (a
// cross-node intermediate-texture Y convention interaction — reported as a
// finding), so we assert only the Y-flip-invariant RED (X) gradient here.
// -----------------------------------------------------------------------------
TEST_CASE("mrt-gbuffer writes color/normals/edges ports", "[gfx][l3][isf][mrt]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  const IsfResult r
      = render(backend, {corpus("mrt-single-color.fs"), corpus("mrt-gbuffer.fs")});
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  INFO("backend=" << r.backend);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 3);
  for(const auto& o : r.outputs)
    REQUIRE(o.valid());

  const auto& color = r.outputs[0];
  const auto& normals = r.outputs[1];
  const auto& edges = r.outputs[2];

  // colorOut: passthrough of uv gradient -> RED rises left->right (X axis is
  // consistent across backends; see file note re: the GREEN/Y divergence).
  const int rL = color.at(color.width / 8, color.height / 2)[0];
  const int rR = color.at(7 * color.width / 8, color.height / 2)[0];
  INFO("color RED left=" << rL << " right=" << rR);
  CHECK(rL < 60);
  CHECK(rR > 200);

  // normalsOut: smooth gradient -> nearly-flat normal ~(128,128,255).
  const auto n = normals.center();
  INFO("normal centre = (" << (int)n[0] << "," << (int)n[1] << "," << (int)n[2] << ")");
  CHECK(near(n, {128, 130, 255, 255}, 14));

  // edgesOut: a smooth gradient has no edges above threshold -> black interior.
  const auto e = edges.center();
  INFO("edge centre = (" << (int)e[0] << "," << (int)e[1] << "," << (int)e[2] << ")");
  CHECK(near(e, {0, 0, 0, 255}, 8));

  // The three attachments are genuinely distinct renders.
  CHECK(!near(color.center(), normals.center(), 12));
  CHECK(!near(normals.center(), edges.center(), 12));
}

TEST_CASE(
    "mrt-gbuffer normals+edges attachments agree across backends",
    "[gfx][l3][isf][mrt]")
{
  const auto shots
      = render_all({corpus("mrt-single-color.fs"), corpus("mrt-gbuffer.fs")});
  std::vector<std::pair<ReadbackImage, ReadbackImage>> got; // {normals, edges}
  for(const auto& s : shots)
    if(!s.result.skipped && s.result.error.empty() && s.result.outputs.size() == 3)
      got.push_back({s.result.outputs[1], s.result.outputs[2]});
  if(got.size() < 2)
    SKIP("need >=2 live backends to compare");
  for(std::size_t i = 1; i < got.size(); ++i)
  {
    const int dn = max_channel_diff(got[0].first, got[i].first);
    const int de = max_channel_diff(got[0].second, got[i].second);
    INFO("normals diff=" << dn << " edges diff=" << de);
    CHECK(dn <= 4);
    CHECK(de <= 4);
  }
}
