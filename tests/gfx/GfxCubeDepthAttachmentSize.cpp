// A raw-raster pass with a cube output is square in every attachment.
//
// Cube faces must be square, so the cube colour output is allocated at
// min(w, h), and the depth attachment of the same pass must match: not every
// API accepts a pass whose attachments disagree in size. A declared depth output exposes that
// attachment, so its size is observable here.
#include "IsfTestCommon.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <array>
#include <cstdlib>

using namespace score::test::gfx;
using score::test::gfx::isf::corpus;


TEST_CASE(
    "a cube pass allocates its depth attachment as square as the cube",
    "[gfx][cubemap][depth]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  bool skipped = false;
  std::string err;
  QSize cube, depth;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int raster
        = p.addRaster(corpus("rr-cube-depth.vs"), corpus("rr-cube-depth.fs"));
    if(raster < 0)
    {
      err = p.error();
      return;
    }
    const int sink = p.addSink({64, 32});
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      skipped = p.skipped();
      err = skipped ? std::string{} : p.error();
      return;
    }
    p.render(2);
    auto* node = p.isf(raster);
    if(node->renderedNodes.empty())
    {
      err = "no renderer";
      return;
    }
    auto* r = node->renderedNodes.begin()->second;
    if(auto* t = r->textureForOutput(*p.imageOut(raster, 0)))
      cube = t->pixelSize();
    if(auto* t = r->textureForOutput(*p.imageOut(raster, 1)))
      depth = t->pixelSize();
  });
  if(skipped)
    SKIP("backend unavailable");

  INFO("error=" << err);
  INFO(
      "cube " << cube.width() << "x" << cube.height() << " depth " << depth.width()
              << "x" << depth.height());
  REQUIRE(err.empty());
  CHECK(cube == QSize(32, 32));
  CHECK(depth == cube);
}

// A cube output wired straight to a window is shown through the multi-target
// blit's samplerCube variant, showing the +Z face: Metal's draw validation
// aborts on a cube texture in a 2D slot, and Vulkan's
// VUID-vkCmdDraw-viewType-07752 forbids it.
TEST_CASE(
    "a cube output wired to a window shows its +Z face",
    "[gfx][cubemap]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  bool skipped = false;
  std::string err;
  std::array<uint8_t, 4> centre{};
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int raster = p.addRaster(
        corpus("rr-cube-faces-procedural.vs"), corpus("rr-cube-faces-procedural.fs"));
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
    p.render(3);
    const auto img = p.readback(sink);
    if(!img.valid())
    {
      err = "readback failed";
      return;
    }
    centre = img.at(img.width / 2, img.height / 2);
  });
  if(skipped)
    SKIP("backend unavailable");

  INFO("error=" << err);
  INFO(
      "centre=(" << int(centre[0]) << "," << int(centre[1]) << ","
                 << int(centre[2]) << ")");
  REQUIRE(err.empty());
  CHECK(std::abs(int(centre[0]) - 160) <= 2);
  CHECK(centre[1] > 250);
}
