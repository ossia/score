// A procedural raw raster (no VERTEX_INPUTS) adopts what its cabled geometry
// publishes: a read-only storage image INPUT and a top-level AUXILIARY buffer
// resolve by name against the upstream CSF's image and geometry auxiliary,
// instead of staying on their zero placeholders.
#include "IsfTestCommon.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace score::test::gfx;
using namespace score::test::gfx::isf;

TEST_CASE(
    "procedural raw raster reads the storage image and auxiliary buffer of its "
    "geometry",
    "[gfx][raster][auxiliary]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  IsfResult r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = render_raster(
        api, {corpus("csf-procgeo-producer.cs")}, corpus("rr-procgeo-consumer.vs"),
        corpus("rr-procgeo-consumer.fs"), {64, 64}, 4);
  });
  if(r.skipped)
    SKIP("backend unavailable: " << r.skip_reason);
  if(const char* why = compute_shader_skip_reason(api))
    SKIP(why);
  INFO("error=" << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 1);
  const auto& img = r.outputs[0];
  REQUIRE(img.valid());

  for(int y : {8, 32, 56})
  {
    const auto l = img.at(12, y);
    INFO("image half (12," << y << ") = " << int(l[0]) << " " << int(l[1]) << " "
                          << int(l[2]));
    CHECK(l[0] < 30);
    CHECK(l[1] > 220);
    CHECK(l[2] < 30);

    const auto rgt = img.at(52, y);
    INFO("buffer half (52," << y << ") = " << int(rgt[0]) << " " << int(rgt[1]) << " "
                           << int(rgt[2]));
    CHECK(rgt[0] < 30);
    CHECK(rgt[1] < 30);
    CHECK(rgt[2] > 220);
  }
}
