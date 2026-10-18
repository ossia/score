// ISF image macros in pixel units sample in the IMAGE's pixels (ISF spec,
// VVISF): IMG_PIXEL(image, coord) is coord / size(image), and
// IMG_THIS_PIXEL(image) is IMG_PIXEL(image, gl_FragCoord.xy), pixel-aligned;
// only IMG_THIS_NORM_PIXEL stretches the image over the frame. Graphics
// shaders divided by RENDERSIZE and made IMG_THIS_PIXEL the normalised form
// (shader board U29, U30); compute shaders already used the texture size.
#include <score_test/Gfx.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

using namespace score::test::gfx;

TEST_CASE("ISF IMG_PIXEL and IMG_THIS_PIXEL use the image's pixels", "[gfx][isf][U29][U30]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  IsfResult r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = render_isf_chain(be, {QStringLiteral(GFX_TEST_CORPUS_DIR "/isf-img-pixel-size.fs")});
  });
  if(r.skipped)
    SKIP(r.backend << ": " << r.skip_reason);
  INFO("backend=" << r.backend << " error=" << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(!r.outputs.empty());
  const auto& img = r.outputs.back();
  REQUIRE(img.valid());
  const auto red = [&](int x, int y) { return img.at(x, y)[0] > 128; };
  // Readback rows are top-down; isf_FragNormCoord.y > 0.5 is the top half.
  const int top = img.height / 4, bottom = 3 * img.height / 4;
  INFO("IMG_PIXEL(small, (3.5, 2.5))");
  CHECK_FALSE(red(img.width / 2, top));
  INFO("IMG_THIS_PIXEL(small) at x = 1, 3: pixel-aligned, not stretched");
  CHECK(red(1, bottom));
  CHECK_FALSE(red(3, bottom));
}
