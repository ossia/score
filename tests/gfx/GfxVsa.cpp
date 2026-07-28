// =============================================================================
// L3 — VSA (vertex-shader-art) node construction + render, GREEN coverage.
//
// A VSA node is a score::gfx::ISFNode built with a VSA descriptor
// (SimpleRenderedVSANode) from a single .vs that emits POINT_COUNT procedural
// vertices from `vertexId`. This target proves the VSA path works end to end:
// a grid of colored POINTS renders offscreen, reads back non-degenerate, and
// AGREES across every backend. Points are not affected by the triangle cull
// path, so this stays green regardless of the cull-mode divergence tracked
// separately in test_gfx_vsa_cull (GfxVsaCull.cpp).
// =============================================================================

#include <score_test/Gfx.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <cstdio>
#include <vector>

using namespace score::test::gfx;

namespace
{
QString corpus(const char* file)
{
  return QString{GFX_TEST_CORPUS_DIR "/"} + file;
}

int drawn_pixels(const ReadbackImage& img, int thresh = 12)
{
  int n = 0;
  for(int y = 0; y < img.height; ++y)
    for(int x = 0; x < img.width; ++x)
    {
      const auto p = img.at(x, y);
      if(int(p[0]) + int(p[1]) + int(p[2]) > thresh)
        ++n;
    }
  return n;
}

int max_channel_diff(const ReadbackImage& a, const ReadbackImage& b)
{
  if(a.width != b.width || a.height != b.height)
    return 256;
  int worst = 0;
  for(int y = 2; y < a.height - 2; y += 3)
    for(int x = 2; x < a.width - 2; x += 3)
    {
      const auto pa = a.at(x, y);
      const auto pb = b.at(x, y);
      for(int c = 0; c < 4; ++c)
        worst = std::max(worst, std::abs(int(pa[c]) - int(pb[c])));
    }
  return worst;
}

IsfResult run_vsa(score::gfx::GraphicsApi be, const char* vs)
{
  IsfResult r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = render_vsa(be, corpus(vs));
  });
  return r;
}
}

TEST_CASE("VSA points grid: renders non-degenerate", "[gfx][l3][vsa]")
{
  const auto be = GENERATE(from_range(platform_backends()));

  const IsfResult r = run_vsa(be, "vsa-points.vs");
  if(qEnvironmentVariableIsSet("GFX_DUMP") && !r.outputs.empty())
  {
    const auto& o = r.outputs[0];
    std::fprintf(
        stderr, "[vsa-points] backend=%s skipped=%d err='%s' drawn=%d\n",
        r.backend.c_str(), int(r.skipped), r.error.c_str(), drawn_pixels(o));
    std::fflush(stderr);
  }

  if(r.skipped)
    SKIP(r.backend << ": " << r.skip_reason);
  INFO("backend=" << r.backend << " error=" << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 1);
  REQUIRE(r.outputs[0].valid());

  // A grid of colored points must cover a good chunk of the frame.
  CHECK(drawn_pixels(r.outputs[0]) > (r.outputs[0].width * r.outputs[0].height) / 8);
}

TEST_CASE("VSA points grid: OpenGL and Vulkan agree", "[gfx][l3][vsa]")
{
  std::vector<IsfResult> shots;
  for(auto be : platform_backends())
    shots.push_back(run_vsa(be, "vsa-points.vs"));

  int compared = 0;
  const ReadbackImage* ref = nullptr;
  std::string refBackend;
  for(const auto& r : shots)
  {
    if(r.skipped)
      continue;
    INFO("backend=" << r.backend << " error=" << r.error);
    REQUIRE(r.error.empty());
    REQUIRE(r.outputs.size() == 1);
    REQUIRE(r.outputs[0].valid());
    if(!ref)
    {
      ref = &r.outputs[0];
      refBackend = r.backend;
      continue;
    }
    const int d = max_channel_diff(*ref, r.outputs[0]);
    INFO(refBackend << " vs " << r.backend << " max channel diff = " << d);
    CHECK(d <= 8);
    ++compared;
  }
  if(compared == 0)
    SKIP("fewer than two backends available to compare");
}
