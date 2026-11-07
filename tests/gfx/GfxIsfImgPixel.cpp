// ISF image macros in pixel units sample in the IMAGE's pixels (ISF spec,
// VVISF): IMG_PIXEL(image, coord) is coord / size(image), and
// IMG_THIS_PIXEL(image) is IMG_PIXEL at the quad's position in output pixels,
// pixel-aligned and following the vertex shader's warp;
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

// The PS1 and N64 layers of the scene-rendering-pipelines example render at
// 320x240 into a mixer whose output is the window. A layer read with
// IMG_THIS_PIXEL is drawn at its own size in the frame's bottom-left corner;
// read with IMG_THIS_NORM_PIXEL it covers the frame.
TEST_CASE(
    "ISF IMG_THIS_NORM_PIXEL stretches an inlet rendered smaller than the output",
    "[gfx][isf][U30]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  ReadbackImage img;
  std::string error, backend;
  bool skipped = false;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int gradient = p.addIsf(QStringLiteral(GFX_TEST_CORPUS_DIR "/isf-gradient-x.fs"));
    const int reader
        = p.addIsf(QStringLiteral(GFX_TEST_CORPUS_DIR "/isf-this-pixel-small-inlet.fs"));
    const int sink = p.addSink({64, 64});
    p.wire(p.imageOut(gradient, 0), p.imageIn(reader, 0));
    p.wire(p.imageOut(reader, 0), p.sinkInput(sink));
    if(!p.create(be))
    {
      skipped = p.skipped();
      error = skipped ? p.skipReason() : p.error();
      backend = p.backend();
      return;
    }
    backend = p.backend();

    ossia::render_target_spec spec;
    spec.size = ossia::texture_size{16, 16};
    spec.address_u = ossia::texture_address_mode::CLAMP_TO_EDGE;
    spec.address_v = ossia::texture_address_mode::CLAMP_TO_EDGE;
    setRenderTargetSpec(*p.isf(reader), first_image_input(*p.isf(reader)), spec);
    p.render(4);
    error = p.error();
    img = p.readback(sink);
  });
  if(skipped)
    SKIP(backend << ": " << error);
  INFO("backend=" << backend << " error=" << error);
  REQUIRE(error.empty());
  REQUIRE(img.valid());
  REQUIRE(img.width == 64);

  // The input's red is its normalised x. Readback rows are top-down.
  const auto red = [&](int x, int y) { return int(img.at(x, y)[0]); };
  const int top = img.height / 4, bottom = 3 * img.height / 4;
  INFO("IMG_THIS_NORM_PIXEL at x = 8 and 48 of 64: red = x / 64");
  CHECK(red(8, top) < 60);
  CHECK(red(48, top) > 165);
  CHECK(red(48, top) < 220);
  INFO("IMG_THIS_PIXEL at x = 8 and 48 of 64: the 16 pixel input is not stretched");
  CHECK(red(8, bottom) > 100);
  CHECK(red(8, bottom) < 155);
  CHECK(red(48, bottom) > 230);
}

// The vertex shader mirrors the quad horizontally and halves it around the
// centre (3d Rotate and Vertex Manipulator do the same kind of warp). As in
// VVISF, IMG_THIS_PIXEL reads at the quad's position from isf_vertShaderInit,
// before the warp, so the gradient is drawn mirrored and squeezed into the
// centre half of the frame instead of staying where the fragments land.
TEST_CASE("ISF IMG_THIS_PIXEL follows the vertex shader's warp", "[gfx][isf][U30]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  ReadbackImage img;
  std::string error, backend;
  bool skipped = false;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int gradient = p.addIsf(QStringLiteral(GFX_TEST_CORPUS_DIR "/isf-gradient-x.fs"));
    const int reader
        = p.addIsf(QStringLiteral(GFX_TEST_CORPUS_DIR "/isf-this-pixel-warped-quad.fs"));
    const int sink = p.addSink({64, 64});
    p.wire(p.imageOut(gradient, 0), p.imageIn(reader, 0));
    p.wire(p.imageOut(reader, 0), p.sinkInput(sink));
    if(!p.create(be))
    {
      skipped = p.skipped();
      error = skipped ? p.skipReason() : p.error();
      backend = p.backend();
      return;
    }
    backend = p.backend();
    p.render(4);
    error = p.error();
    img = p.readback(sink);
  });
  if(skipped)
    SKIP(backend << ": " << error);
  INFO("backend=" << backend << " error=" << error);
  REQUIRE(error.empty());
  REQUIRE(img.valid());
  REQUIRE(img.width == 64);
  REQUIRE(img.height == 64);

  const auto px = [&](int x, int y) { return img.at(x, y); };
  const int mid = img.height / 2;
  // The quad covers x and y in [16, 48). Pixel 20 is at clip x -0.36, which
  // the mirror and halving bring from 0.72: input pixel 55 of 64.
  INFO("left of the warped quad: red " << int(px(20, mid)[0]));
  CHECK(int(px(20, mid)[0]) > 200);
  INFO("right of the warped quad: red " << int(px(44, mid)[0]));
  CHECK(int(px(44, mid)[0]) < 50);
  INFO("the gradient falls from left to right across the quad");
  for(int x = 17; x < 47; x++)
    CHECK(int(px(x, mid)[0]) >= int(px(x + 1, mid)[0]));
  INFO("outside the quad nothing is drawn: green " << int(px(4, mid)[1]) << ", "
                                                   << int(px(32, 4)[1]));
  CHECK(int(px(4, mid)[1]) < 25);
  CHECK(int(px(32, 4)[1]) < 25);
  CHECK(int(px(32, mid)[1]) > 40);
}

// A vertex shader that writes isf_FragNormCoord after isf_vertShaderInit
// leaves IMG_THIS_PIXEL alone: the read stays at the quad position init
// computed, as VVISF's per-image coordinates do, so the gradient comes through
// unchanged instead of the single texel at (0, 0).
TEST_CASE(
    "ISF IMG_THIS_PIXEL ignores later writes to isf_FragNormCoord", "[gfx][isf][U30]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  ReadbackImage img;
  std::string error, backend;
  bool skipped = false;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int gradient = p.addIsf(QStringLiteral(GFX_TEST_CORPUS_DIR "/isf-gradient-x.fs"));
    const int reader = p.addIsf(
        QStringLiteral(GFX_TEST_CORPUS_DIR "/isf-this-pixel-normcoord-moved.fs"));
    const int sink = p.addSink({64, 64});
    p.wire(p.imageOut(gradient, 0), p.imageIn(reader, 0));
    p.wire(p.imageOut(reader, 0), p.sinkInput(sink));
    if(!p.create(be))
    {
      skipped = p.skipped();
      error = skipped ? p.skipReason() : p.error();
      backend = p.backend();
      return;
    }
    backend = p.backend();
    p.render(4);
    error = p.error();
    img = p.readback(sink);
  });
  if(skipped)
    SKIP(backend << ": " << error);
  INFO("backend=" << backend << " error=" << error);
  REQUIRE(error.empty());
  REQUIRE(img.valid());

  const int mid = img.height / 2;
  const auto red = [&](int x) { return int(img.at(x, mid)[0]); };
  INFO("red at 4: " << red(4) << ", at 32: " << red(32) << ", at 60: " << red(60));
  // red = x / width at the pixel centre.
  CHECK(std::abs(red(4) - 18) <= 4);
  CHECK(std::abs(red(32) - 130) <= 4);
  CHECK(std::abs(red(60) - 243) <= 4);
}
