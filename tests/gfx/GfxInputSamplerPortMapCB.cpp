// An IS_ARRAY image input gets the sampler its INPUTS entry declares when an
// earlier INPUTS entry creates no input port.
//
// cb-isf-storage-then-array declares a write-only storage buffer (an output
// port, no input port) and then an IS_ARRAY image with WRAP repeat, and samples
// layer 0 of cb-rr-array-left-right at u = 1.25. initInputSamplers matched desc.inputs[i]
// to input port i, so the image port read the storage entry's (absent) sampler
// config and got the default ClampToEdge sampler: green. With the declared
// repeat sampler it reads u = 0.25: red.
//
// Registration: score_add_gfx_test(input_sampler_port_map_cb GfxInputSamplerPortMapCB.cpp)
#include <score_test/Gfx.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <string>

using namespace score::test;
using namespace score::test::gfx;

namespace
{
QString corpus(const char* f)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR "/") + QString::fromUtf8(f);
}
}

TEST_CASE(
    "an image input after a port-less storage input gets its declared sampler",
    "[gfx][isf][sampler][storage]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  bool skipped = false;
  std::string skip_reason, error;
  ReadbackImage view;
  run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int src = p.addRaster(
        corpus("cb-rr-array-left-right.vs"), corpus("cb-rr-array-left-right.fs"));
    const int probe = p.addIsf(corpus("cb-isf-storage-then-array.fs"));
    if(src < 0 || probe < 0)
    {
      error = "chain build failed: " + p.error();
      return;
    }
    const int sink = p.addSink({32, 32});
    p.wire(p.imageOut(src, 0), p.imageIn(probe, 0));
    p.wire(p.imageOut(probe, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      skipped = p.skipped();
      skip_reason = p.skipReason();
      error = skipped ? std::string{} : p.error();
      return;
    }
    p.render(4);
    view = p.readback(sink);
    if(!view.valid())
      error = "empty readback";
  });
  if(skipped)
    SKIP(skip_reason);
  REQUIRE(error.empty());

  const auto px = view.center();
  CAPTURE(int(px[0]), int(px[1]), int(px[2]));
  CHECK(px[0] > 200);
  CHECK(px[1] < 50);
}
