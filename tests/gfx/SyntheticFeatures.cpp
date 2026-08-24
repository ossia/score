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

namespace
{
//! Mean of one channel over the readback, so an oracle can name an expected
//! value rather than "not blank".
int channel_at(const ReadbackImage& img, int x, int y, int c)
{
  return img.at(x, y)[c];
}
}

TEST_CASE("EXECUTION_MODEL 1D_BUFFER dispatches over the whole buffer",
          "[gfx][syn][csf][execution]")
{
  // Invocation i writes i*2 into a 64-entry buffer; the image pass paints
  // entry 32, which is 64 only if the 1D dispatch actually covered the range.
  const auto api = GENERATE(from_range(platform_backends()));
  IsfResult r;
  run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = render_csf_image(api, corpus("syn-exec-1d-buffer.cs"), {}, {16, 16}, 3);
  });
  if(r.skipped) SKIP(r.backend + ": " + r.skip_reason);
  INFO("backend=" << r.backend << " error: " << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 1);
  REQUIRE(r.outputs[0].valid());
  const int red = channel_at(r.outputs[0], 8, 8, 0);
  INFO("red channel = " << red << ", expected 64 (buf.v[32] == 32*2)");
  CHECK(std::abs(red - 64) <= 2);
}

TEST_CASE("EXECUTION_MODEL MANUAL dispatches exactly WORKGROUPS x LOCAL_SIZE",
          "[gfx][syn][csf][execution]")
{
  // WORKGROUPS [2,1,1] x LOCAL_SIZE 8 = 16 invocations, no more and no fewer.
  // An oracle of "non-zero" would accept a dispatch of any size; this one does
  // not.
  const auto api = GENERATE(from_range(platform_backends()));
  IsfResult r;
  run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = render_csf_image(api, corpus("syn-exec-manual.cs"), {}, {16, 16}, 3);
  });
  if(r.skipped) SKIP(r.backend + ": " + r.skip_reason);
  INFO("backend=" << r.backend << " error: " << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 1);
  REQUIRE(r.outputs[0].valid());
  const int hits = r.outputs[0].at(8, 8)[0];
  INFO("invocations counted = " << hits << ", expected exactly 16");
  CHECK(hits == 16);
}

TEST_CASE("a PERSISTENT buffer carries state across frames",
          "[gfx][syn][csf][feedback]")
{
  // The counter survives frame to frame, so five frames read 5. A buffer that
  // is cleared or reallocated per frame reads 1 -- which is the failure this
  // distinguishes, and which "not blank" would miss entirely.
  const auto api = GENERATE(from_range(platform_backends()));
  IsfResult r;
  run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = render_csf_image(api, corpus("syn-feedback-persistent.cs"), {}, {16, 16}, 5);
  });
  if(r.skipped) SKIP(r.backend + ": " + r.skip_reason);
  INFO("backend=" << r.backend << " error: " << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 1);
  REQUIRE(r.outputs[0].valid());
  const int frames = r.outputs[0].at(8, 8)[0];
  INFO("accumulated frames = " << frames << " (1 would mean no persistence)");
  CHECK(frames >= 2);
}
