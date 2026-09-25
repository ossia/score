// A read_only storage image INPUT in a raw raster has an image port, so
// initInputSamplers() gives it a sampler, but libisf declares no sampler for
// it, so the raw raster must not bind that stray sampler: it would shift every
// later sampler one binding late and t0 would read the storage image's
// placeholder instead of the cabled texture.
//
// On OpenGL a storage image is bound to the image unit of its binding, and
// NVIDIA exposes 8 units. A shader whose storage images reach the context's
// GL_MAX_IMAGE_UNITS logs one warning per shader (ISF, CSF, raw raster) on
// OpenGL, and none on the other backends or on a context with more units
// (Mesa's llvmpipe exposes 192).
#include <score_test/Gfx.hpp>

#include "GfxLogCapture.hpp"

#include <Gfx/Graph/Utils.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <array>
#include <string>
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
    "a raw raster read_only storage image takes no sampler binding",
    "[gfx][raster][binding]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  bool skipped = false;
  std::string err;
  ReadbackImage img;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int green = p.addIsf(corpus("isf-solid-green.fs"));
    const int raster = p.addRaster(
        corpus("rr-readonly-image-then-sampler.vs"),
        corpus("rr-readonly-image-then-sampler.fs"));
    if(green < 0 || raster < 0)
    {
      err = p.error();
      return;
    }
    const int sink = p.addSink({32, 32});
    p.wire(p.imageOut(green, 0), p.imageIn(raster, 1));
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      skipped = p.skipped();
      err = skipped ? std::string{} : p.error();
      return;
    }
    p.render(3);
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
  for(auto [x, y] : std::array<std::array<int, 2>, 3>{{{4, 4}, {16, 16}, {27, 27}}})
  {
    const auto px = img.at(x, y);
    INFO("pixel " << x << "," << y << " = " << int(px[0]) << " " << int(px[1]) << " "
                  << int(px[2]));
    CHECK(px[0] < 40);
    CHECK(px[1] > 200);
    CHECK(px[2] < 40);
  }
}

TEST_CASE("storage image bindings past the GL image units warn once", "[gfx][binding]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const auto file = GENERATE(
      "isf-six-storage-images.fs", "csf-six-storage-images.cs", "rr-six-storage-images");
  CAPTURE(backend_name(api), file);

  const QString f = QString::fromUtf8(file);
  constexpr int highestImageBinding = 8;

  const QString kind = f.startsWith("rr-")  ? QStringLiteral("raw raster shader")
                       : f.endsWith(".cs") ? QStringLiteral("CSF shader")
                                           : QStringLiteral("ISF shader");
  bool skipped = false;
  std::string backend;
  std::string err;
  int units = 0;
  int warnings = 0;
  int kindWarnings = 0;
  for(int run = 0; run < 2; run++)
  {
    score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
      // Inside the application: its Messages panel installs its own handler.
      // The tally is read once the pipeline is gone, on every way out.
      LogCapture log;
      struct Tally
      {
        const LogCapture& log;
        const QString& kind;
        int& warnings;
        int& kindWarnings;
        ~Tally()
        {
          warnings += log.count({u"image units"});
          kindWarnings += log.count({u"image units", kind});
        }
      } tally{log, kind, warnings, kindWarnings};
      GfxPipeline p;
      const int node = f.startsWith("rr-") ? p.addRaster(
                                                 corpus("rr-six-storage-images.vs"),
                                                 corpus("rr-six-storage-images.fs"))
                       : f.endsWith(".cs") ? p.addCsf(corpus(file))
                                           : p.addIsf(corpus(file));
      if(node < 0)
      {
        err = p.error();
        return;
      }
      const int sink = p.addSink({16, 16});
      p.wire(p.imageOut(node, 0), p.sinkInput(sink));
      if(!p.create(api))
      {
        skipped = p.skipped();
        err = skipped ? std::string{} : p.error();
        return;
      }
      backend = p.backend();
      p.render(2);
      if(const auto st = p.sink(sink)->renderState(); st && st->rhi)
        units = score::gfx::storageImageUnitLimit(*st->rhi);
    });
    if(skipped)
      SKIP("backend unavailable");
  }
  if(f.endsWith(".cs"))
    if(const char* why = compute_shader_skip_reason(api))
      SKIP(why);

  INFO("error=" << err);
  REQUIRE(err.empty());
  REQUIRE(units > 0);

  INFO("backend " << backend << ", image units " << units);
  if(api == score::gfx::OpenGL && highestImageBinding >= units)
  {
    CHECK(warnings == 1);
    CHECK(kindWarnings == 1);
  }
  else
  {
    CHECK(warnings == 0);
  }
}
