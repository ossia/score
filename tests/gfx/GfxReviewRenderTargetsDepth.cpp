// Depth-clear polarity, from the 2026-09 graphics review (section 7 / B2).
//
// The forward-Z (less) and reverse-Z (greater) shaders must both light the
// probe: a fragment at .5 has to beat whatever the pass cleared depth to. The
// reverse-Z case is the control -- it is the path the engine already takes.
#include <score_test/Gfx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>
using namespace score::test::gfx;
TEST_CASE("RenderTargets-10 simple MRT forward depth clears to the far plane", "[RenderTargetsDepthClear]") {
  const auto be=GENERATE(from_range(platform_backends()));
  IsfResult less,greater;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    const QString dir{GFX_TEST_CORPUS_DIR};
    less=render_isf_chain(be,{dir+"/RenderTargets-depth-less.fs"},{64,64},3);
    greater=render_isf_chain(be,{dir+"/RenderTargets-depth-greater.fs"},{64,64},3);
  });
  if(less.skipped||greater.skipped) SKIP(less.skip_reason+greater.skip_reason);
  INFO(less.error); INFO(greater.error);
  REQUIRE(less.error.empty()); REQUIRE(greater.error.empty());
  REQUIRE(!less.outputs.empty()); REQUIRE(!greater.outputs.empty());
  REQUIRE(less.outputs[0].valid()); REQUIRE(greater.outputs[0].valid());
  const auto a=less.outputs[0].center(),b=greater.outputs[0].center(); CAPTURE(a,b);
  CHECK(near(b,{255,0,0,255},3)); // Existing reverse-Z path is the control.
  CHECK(near(a,{255,0,0,255},3)); // .5 must beat a forward-Z clear of 1.
}
