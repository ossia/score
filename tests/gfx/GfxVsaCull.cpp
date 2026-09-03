// =============================================================================
// L3 -- VSA triangle cull-mode. Isolated in its own target, because a
// cull-mode regression is the kind that would otherwise take an unrelated
// group down with it.
//
// A single solid triangle (vsa-triangle.vs) rendered through the VSA node: one
// procedurally-emitted front-facing triangle must be visible on every backend,
// and the backends must agree.
//
// The hazard: for a given winding in vsa-triangle.vs, OpenGL draws the
// triangle while Vulkan culls it entirely, and reversing the winding flips
// which backend culls. No winding renders on both on its own; the per-backend
// cull mode, FrontFace and the SPIRV-only Y-flip have to compose for the
// visible face to be consistent across GL and Vulkan.
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

TEST_CASE("VSA triangle: front face visible on every backend (R3-N4, fixed)", "[gfx][l3][vsa][cull][finding]")
{
  const auto be = GENERATE(from_range(platform_backends()));

  const IsfResult r = run_vsa(be, "vsa-triangle.vs");
  if(qEnvironmentVariableIsSet("GFX_DUMP") && !r.outputs.empty())
  {
    const auto& o = r.outputs[0];
    const auto c = o.at(o.width / 2, o.height / 2);
    std::fprintf(
        stderr, "[vsa-tri] backend=%s drawn=%d center=(%d,%d,%d,%d)\n",
        r.backend.c_str(), drawn_pixels(o), c[0], c[1], c[2], c[3]);
    std::fflush(stderr);
  }

  if(r.skipped)
    SKIP(r.backend << ": " << r.skip_reason);
  INFO("backend=" << r.backend << " error=" << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 1);
  const auto& img = r.outputs[0];
  REQUIRE(img.valid());

  // The front-facing triangle covers the center on every backend.
  const auto center = img.at(img.width / 2, img.height / 2);
  INFO("center=(" << int(center[0]) << "," << int(center[1]) << ","
                  << int(center[2]) << "," << int(center[3]) << ")");
  CHECK(int(center[0]) > 150);
  CHECK(drawn_pixels(img) > (img.width * img.height) / 8);
}

TEST_CASE("VSA triangle: backends agree (R3-N4, fixed)", "[gfx][l3][vsa][cull][finding]")
{
  std::vector<IsfResult> shots;
  for(auto be : platform_backends())
    shots.push_back(run_vsa(be, "vsa-triangle.vs"));

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
    // Cross-backend agreement: a large divergence here means one backend
    // draws the triangle and the other culls it entirely.
    const int d = max_channel_diff(*ref, r.outputs[0]);
    INFO(refBackend << " vs " << r.backend << " max channel diff = " << d);
    CHECK(d <= 8);
    ++compared;
  }
  if(compared == 0)
    SKIP("fewer than two backends available to compare");
}
