#include "IsfTestCommon.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_range.hpp>
using namespace score::test;
using namespace score::test::gfx;
using namespace score::test::gfx::isf;

TEST_CASE("a raw-raster shader can sample an image input", "[gfx][syn][raster]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  IsfResult r;
  run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = render_raster(api, {corpus("syn-geo-producer.cs")},
                      corpus("syn-rrp-image-input.vs"), corpus("syn-rrp-image-input.fs"));
  });
  if(r.skipped) SKIP(r.backend + ": " + r.skip_reason);
  INFO("backend=" << r.backend);
  INFO("error: " << r.error);
  REQUIRE(r.error.empty());
}
