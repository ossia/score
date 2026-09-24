// Names reserved in the Metal Shading Language but legal in GLSL compile.
//
// SPIRV-Cross renames most MSL keywords, but not a function called `quad`
// (a Metal type) nor a uniform member called `operator` (a C++ keyword): the
// MSL fails to compile and the node never draws on Metal. The fixture uses
// both and renders red.
#include "IsfTestCommon.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

using namespace score::test::gfx;
using score::test::gfx::isf::corpus;


TEST_CASE("MSL-reserved names in a shader still compile", "[gfx][isf][metal]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  bool skipped = false;
  std::string err;
  ReadbackImage img;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int isf = p.addIsf(corpus("isf-msl-reserved-names.fs"));
    if(isf < 0)
    {
      err = p.error();
      return;
    }
    const int sink = p.addSink({16, 16});
    p.wire(p.imageOut(isf, 0), p.sinkInput(sink));
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
  INFO("error=" << err);
  REQUIRE(err.empty());
  const auto px = img.center();
  INFO("centre " << int(px[0]) << " " << int(px[1]) << " " << int(px[2]));
  CHECK(px[0] > 200);
  CHECK(px[1] < 40);
  CHECK(px[2] < 40);
}
