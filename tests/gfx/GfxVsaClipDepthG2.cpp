// VSA clip-space depth (N39). A VERTEX_SHADER_ART shader writes gl_Position
// in WebGL clip space, where the visible depth range is z in [-w, w]. A
// triangle at z = -0.75 w must be drawn on every backend, and the backends
// must agree; Vulkan, Metal and D3D clip z < 0 unless the VSA entry point
// remaps z the way clipSpaceCorrMatrix does.

#include <score_test/Gfx.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <vector>

using namespace score::test::gfx;

namespace
{
QString corpus(const char* file)
{
  return QString{GFX_TEST_CORPUS_DIR "/"} + file;
}

int drawn_pixels(const ReadbackImage& img)
{
  int n = 0;
  for(int y = 0; y < img.height; ++y)
    for(int x = 0; x < img.width; ++x)
    {
      const auto p = img.at(x, y);
      if(int(p[0]) + int(p[1]) + int(p[2]) > 12)
        ++n;
    }
  return n;
}

IsfResult run_vsa(score::gfx::GraphicsApi be)
{
  IsfResult r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = render_vsa(be, corpus("vsa-clip-depth-g2.vs"));
  });
  return r;
}
}

TEST_CASE("VSA: negative clip z inside [-w, w] is drawn (N39)", "[gfx][vsa][n39]")
{
  const auto be = GENERATE(from_range(platform_backends()));

  const IsfResult r = run_vsa(be);
  if(r.skipped)
    SKIP(r.backend << ": " << r.skip_reason);
  INFO("backend=" << r.backend << " error=" << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 1);
  const auto& img = r.outputs[0];
  REQUIRE(img.valid());

  const auto center = img.at(img.width / 2, img.height / 2);
  INFO("center=(" << int(center[0]) << "," << int(center[1]) << ","
                  << int(center[2]) << "," << int(center[3]) << ")");
  CHECK(int(center[0]) > 150);
  CHECK(drawn_pixels(img) > (img.width * img.height) / 8);
}
