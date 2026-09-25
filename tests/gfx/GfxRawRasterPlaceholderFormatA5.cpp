// An unbound raw raster storage image AUXILIARY gets a placeholder in its
// declared FORMAT (agent A5).
//
// The placeholder was the renderer's RGBA8 empty texture of the right shape.
// classic_pbr_openpbr declares `voxel_grid` as a readonly r32ui uimage3D; with
// nothing wired, Metal's validation layer aborts the first draw on the
// RGBA8Unorm texture bound to a UInt image, and Vulkan's forbids a view whose
// format differs from the image's format qualifier. The placeholder is now a
// 1x1 zero texture of the declared format and shape, usable with image
// load/store.
//
// Pinned here: the placeholder format and flags for float, uint and sint
// formats in 2D, 3D, array and cube shapes on every backend this machine
// brings up, and a raw raster reading an unbound r32ui 3D image and an r32i
// 2D array image renders (both read 0).
//
// Registration:
//   score_add_gfx_test(raw_raster_placeholder_format_a5 GfxRawRasterPlaceholderFormatA5.cpp)
#include <score_test/Gfx.hpp>

#include <Gfx/Graph/Utils.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <array>

using namespace score::test::gfx;

namespace
{
QString corpus(const char* file)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR) + QStringLiteral("/") + file;
}
}

TEST_CASE("image format qualifiers map to their QRhi format", "[gfx][raster][placeholder]")
{
  using score::gfx::imageFormatFromQualifier;
  CHECK(imageFormatFromQualifier("rgba8") == QRhiTexture::RGBA8);
  CHECK(imageFormatFromQualifier("R32F") == QRhiTexture::R32F);
  CHECK(imageFormatFromQualifier("rgba16f") == QRhiTexture::RGBA16F);
  CHECK(imageFormatFromQualifier("r32ui") == QRhiTexture::R32UI);
  CHECK(imageFormatFromQualifier("r32i") == QRhiTexture::R32SI);
  CHECK(imageFormatFromQualifier("rgba32ui") == QRhiTexture::RGBA32UI);
}

TEST_CASE("storage image placeholders take the declared format and shape", "[gfx][raster][placeholder]")
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
  const std::array<Case, 4> cases{{
      {"r32ui", 3, false, false, QRhiTexture::R32UI, QRhiTexture::ThreeDimensional},
      {"r32i", 2, true, false, QRhiTexture::R32SI, QRhiTexture::TextureArray},
      {"rgba16f", 2, false, true, QRhiTexture::RGBA16F, QRhiTexture::CubeMap},
      {"rgba8", 2, false, false, QRhiTexture::RGBA8, {}},
  }};

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

TEST_CASE("a raw raster reads an unbound integer storage image as zero", "[gfx][raster][placeholder]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  bool skipped = false;
  std::string err;
  ReadbackImage img;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int raster = p.addRaster(
        corpus("rr-a5-uint-placeholder.vs"), corpus("rr-a5-uint-placeholder.fs"));
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
