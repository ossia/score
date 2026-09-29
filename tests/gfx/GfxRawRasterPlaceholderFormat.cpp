// An unbound raw raster storage image AUXILIARY gets a placeholder in its
// declared FORMAT.
//
// classic_pbr_openpbr declares `voxel_grid` as a readonly r32ui uimage3D; with
// nothing wired, Metal's validation layer aborts a draw with an RGBA8Unorm
// texture bound to a UInt image, and Vulkan's forbids a view whose format
// differs from the image's format qualifier. The placeholder is a 1x1 zero
// texture of the declared format and shape, usable with image load/store.
//
// Checks the placeholder format and flags for float, uint and sint formats in
// 2D, 3D, array and cube shapes on every available backend, and that a raw
// raster reading an unbound r32ui 3D image and an r32i 2D array image renders
// (both read 0).
#include <score_test/Gfx.hpp>

#include <Gfx/Graph/Utils.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <array>
#include <vector>

using namespace score::test::gfx;

namespace
{
QString corpus(const char* file)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR) + QStringLiteral("/") + file;
}
}

TEST_CASE(
    "storage image placeholders take the declared format and shape",
    "[gfx][raster][placeholder]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  struct Case
  {
    const char* format;
    int dimensions;
    bool array;
    bool cube;
    QRhiTexture::Format expected;
    QRhiTexture::Flags shape;
  };
  // Integer formats exist from Qt 6.10.
  const std::vector<Case> cases{
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
      {"r32ui", 3, false, false, QRhiTexture::R32UI, QRhiTexture::ThreeDimensional},
      {"r32i", 2, true, false, QRhiTexture::R32SI, QRhiTexture::TextureArray},
#endif
      {"rgba16f", 2, false, true, QRhiTexture::RGBA16F, QRhiTexture::CubeMap},
      {"rgba8", 2, false, false, QRhiTexture::RGBA8, {}},
  };

  bool skipped = false;
  std::vector<std::string> failures;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    auto st = score::gfx::createRenderState(api, QSize{16, 16}, nullptr);
    if(!st || !st->rhi || st->rhi->backend() == QRhi::Null)
    {
      skipped = true;
      if(st)
        st->destroy();
      return;
    }
    auto& rhi = *st->rhi;
    auto* res = rhi.nextResourceUpdateBatch();
    for(const auto& c : cases)
    {
      auto* tex = score::gfx::createStorageImagePlaceholder(
          rhi, *res, c.format, c.dimensions, c.array, c.cube);
      if(!tex)
      {
        failures.push_back(std::string{c.format} + ": not created");
        continue;
      }
      if(tex->format() != c.expected)
        failures.push_back(std::string{c.format} + ": format " + std::to_string(tex->format()));
      if(!tex->flags().testFlag(QRhiTexture::UsedWithLoadStore))
        failures.push_back(std::string{c.format} + ": not usable with load/store");
      if(c.shape && !(tex->flags() & c.shape))
        failures.push_back(std::string{c.format} + ": wrong shape");
      delete tex;
    }
    res->release();
    st->destroy();
  });
  if(skipped)
    SKIP("backend unavailable");
  for(const auto& f : failures)
    FAIL_CHECK(f);
}

TEST_CASE(
    "a raw raster reads an unbound integer storage image as zero",
    "[gfx][raster][placeholder]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  bool skipped = false;
  std::string err;
  ReadbackImage img;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int raster = p.addRaster(
        corpus("rr-uint-image-placeholder.vs"), corpus("rr-uint-image-placeholder.fs"));
    if(raster < 0)
    {
      err = p.error();
      return;
    }
    const int sink = p.addSink({32, 32});
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      skipped = p.skipped();
      err = skipped ? std::string{} : p.error();
      return;
    }
    p.render(2);
    img = p.readback(sink);
    if(!img.valid())
      err = "empty readback";
  });
  if(skipped)
    SKIP("backend unavailable");
  if(const char* why = storage_buffer_skip_reason(api))
    SKIP(why);

  INFO("error=" << err);
  REQUIRE(err.empty());
  const auto px = img.center();
  INFO("centre " << int(px[0]) << " " << int(px[1]) << " " << int(px[2]));
  CHECK(px[0] < 40);
  CHECK(px[1] > 200);
}
