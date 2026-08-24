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

TEST_CASE("PER_INSTANCE dispatches once per instance, not per vertex",
          "[gfx][syn][csf][instancing]")
{
  // VERTEX_COUNT 3, INSTANCE_COUNT 4. A PER_INSTANCE pass sized from the
  // instance count marks 4 slots; sized from the vertex count it marks 3, and a
  // pass that never ran marks none. All three are distinguishable here.
  const auto api = GENERATE(from_range(platform_backends()));
  IsfResult r;
  run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = render_csf_image(api, corpus("syn-instancing.cs"), {}, {16, 16}, 3);
  });
  if(r.skipped) SKIP(r.backend + ": " + r.skip_reason);
  INFO("backend=" << r.backend << " error: " << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 1);
  REQUIRE(r.outputs[0].valid());
  const int hits = r.outputs[0].at(8, 8)[0];
  INFO("instance invocations = " << hits << ", expected 4 (3 would mean it "
                                    "dispatched per vertex)");
  CHECK(hits == 4);
}

TEST_CASE("COPY_FROM forwards an attribute buffer between geometries",
          "[gfx][syn][csf][geometry]")
{
  // The filter writes positions only; colour is declared COPY_FROM the upstream
  // geometry and never assigned in the shader. Drawn with raw-raster-basic the
  // triangle is the producer's green if the runtime forwarded the buffer, and
  // black if it did not -- a difference a "not blank" oracle would miss, since
  // the geometry draws either way.
  const auto api = GENERATE(from_range(platform_backends()));
  IsfResult r;
  run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = render_raster(api, {corpus("syn-geo-producer.cs"), corpus("syn-copy-from.cs")},
                      corpus("raw-raster-basic.vs"), corpus("raw-raster-basic.fs"));
  });
  if(r.skipped) SKIP(r.backend + ": " + r.skip_reason);
  INFO("backend=" << r.backend << " error: " << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() >= 1);
  REQUIRE(r.outputs[0].valid());
  const auto px = r.outputs[0].at(32, 32);
  INFO("centre pixel rgba = " << int(px[0]) << "," << int(px[1]) << ","
                              << int(px[2]) << "," << int(px[3]));
  CHECK(int(px[1]) > 128);
  CHECK(int(px[0]) < 96);
}

TEST_CASE("the synthetic producer alone draws its colour",
          "[gfx][syn][raster][control]")
{
  // Control for the COPY_FROM case above: same consumer, no filter in between.
  // If this is not green the chain is at fault, not attribute forwarding.
  const auto api = GENERATE(from_range(platform_backends()));
  IsfResult r;
  run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = render_raster(api, {corpus("syn-geo-producer.cs")},
                      corpus("raw-raster-basic.vs"), corpus("raw-raster-basic.fs"));
  });
  if(r.skipped) SKIP(r.backend + ": " + r.skip_reason);
  INFO("backend=" << r.backend << " error: " << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() >= 1);
  const auto px = r.outputs[0].at(32, 32);
  INFO("centre pixel rgba = " << int(px[0]) << "," << int(px[1]) << ","
                              << int(px[2]) << "," << int(px[3]));
  CHECK(int(px[1]) > 128);
}

TEST_CASE("a geometry filter copying attributes by hand keeps the colour",
          "[gfx][syn][csf][geometry][control]")
{
  // Same shape as the COPY_FROM case but with the copy written out. Green here
  // and black there isolates the declared forwarding as the difference.
  const auto api = GENERATE(from_range(platform_backends()));
  IsfResult r;
  run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = render_raster(api,
                      {corpus("syn-geo-producer.cs"), corpus("syn-passthrough-filter.cs")},
                      corpus("raw-raster-basic.vs"), corpus("raw-raster-basic.fs"));
  });
  if(r.skipped) SKIP(r.backend + ": " + r.skip_reason);
  INFO("backend=" << r.backend << " error: " << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() >= 1);
  const auto px = r.outputs[0].at(32, 32);
  INFO("centre pixel rgba = " << int(px[0]) << "," << int(px[1]) << ","
                              << int(px[2]) << "," << int(px[3]));
  CHECK(int(px[1]) > 128);
}
