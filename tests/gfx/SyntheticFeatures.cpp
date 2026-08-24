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

TEST_CASE("COPY_FROM forwards an attribute declared read_only",
          "[gfx][syn][csf][geometry]")
{
  // The same forwarding written the other legal way. "ACCESS": "none" is what
  // RenderedCSFNode documents for a forwarded attribute, but read_only is the
  // natural way to say "this shader does not write it" and is what the shipped
  // csf-testers corpus uses -- so it has to reach the same pixel, not silently
  // allocate an unwritten buffer and render black.
  const auto api = GENERATE(from_range(platform_backends()));
  IsfResult r;
  run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = render_raster(api,
                      {corpus("syn-geo-producer.cs"), corpus("syn-copy-from-readonly.cs")},
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

TEST_CASE("REQUIRED false falls back to zeroes instead of failing the build",
          "[gfx][syn][csf][geometry]")
{
  // in_texcoord is declared REQUIRED false and the producer never supplies it.
  // The documented contract is a zero-filled buffer, so the shader can read it
  // unconditionally: green stays 1 (the shader ran) and blue stays 0 (the
  // fallback really was zeroes rather than uninitialised memory).
  const auto api = GENERATE(from_range(platform_backends()));
  IsfResult r;
  run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = render_raster(api,
                      {corpus("syn-geo-producer.cs"), corpus("syn-required-optional.cs")},
                      corpus("raw-raster-basic.vs"), corpus("raw-raster-basic.fs"));
  });
  if(r.skipped) SKIP(r.backend + ": " + r.skip_reason);
  INFO("backend=" << r.backend << " error: " << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() >= 1);
  const auto px = r.outputs[0].at(32, 32);
  INFO("centre rgba = " << int(px[0]) << "," << int(px[1]) << "," << int(px[2])
                        << "," << int(px[3]));
  CHECK(int(px[1]) > 128);
  CHECK(int(px[2]) < 32);
}

TEST_CASE("EXECUTION_MODEL PER_MIP loops the pass once per level",
          "[gfx][syn][raster][execution]")
{
  // Each level paints its own PASSINDEX. Level 0 must therefore read 0 in red
  // and 1 in green: green proves the pass ran at all, red proves the level
  // index reaching the shader is the base level rather than a stale or
  // arbitrary value.
  const auto api = GENERATE(from_range(platform_backends()));
  IsfResult r;
  run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = render_raster(api, {corpus("syn-geo-producer.cs")},
                      corpus("syn-raster-per-mip.vs"), corpus("syn-raster-per-mip.fs"));
  });
  if(r.skipped) SKIP(r.backend + ": " + r.skip_reason);
  INFO("backend=" << r.backend << " error: " << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() >= 1);
  const auto px = r.outputs[0].at(32, 32);
  INFO("centre rgba = " << int(px[0]) << "," << int(px[1]) << "," << int(px[2]));
  CHECK(int(px[1]) > 128);
  CHECK(int(px[0]) < 32);
}

TEST_CASE("EXECUTION_MODEL USER dispatches from its generated ports",
          "[gfx][syn][csf][execution]")
{
  // USER makes the runtime create three integer ports for the dispatch counts,
  // each defaulting to 1. With LOCAL_SIZE [4,1,1] the default dispatch is
  // exactly 4 invocations. A USER pass quietly treated as 2D_IMAGE would size
  // from an image and land elsewhere; one never dispatched reads 0.
  //
  // USER is parser-supported and was exercised by nothing in the tree -- not
  // one shader in packages/, not one test -- before this.
  const auto api = GENERATE(from_range(platform_backends()));
  IsfResult r;
  run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = render_csf_image(api, corpus("syn-exec-user.cs"), {}, {16, 16}, 3);
  });
  if(r.skipped) SKIP(r.backend + ": " + r.skip_reason);
  INFO("backend=" << r.backend << " error: " << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 1);
  REQUIRE(r.outputs[0].valid());
  const int hits = r.outputs[0].at(8, 8)[0];
  const int marker = r.outputs[0].at(8, 8)[2];
  // hits[56..63] record which PASSINDEX values reached the shader.
  INFO("invocations = " << hits << ", marker = " << marker
                        << " (marker 7 means the pass ran at all)");
  CHECK(marker == 7);
  CHECK(hits == 4);
}

TEST_CASE("a compute filter can ADD an attribute the input does not have",
          "[gfx][syn][csf][geometry]")
{
  // Upstream carries position only. The blue the consumer draws exists nowhere
  // in the input, so forwarding cannot produce it -- only the filter writing a
  // new attribute can.
  const auto api = GENERATE(from_range(platform_backends()));
  IsfResult r;
  run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = render_raster(api,
                      {corpus("syn-geo-position-only.cs"), corpus("syn-geo-add-attribute.cs")},
                      corpus("raw-raster-basic.vs"), corpus("raw-raster-basic.fs"));
  });
  if(r.skipped) SKIP(r.backend + ": " + r.skip_reason);
  INFO("backend=" << r.backend << " error: " << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() >= 1);
  const auto px = r.outputs[0].at(32, 32);
  INFO("centre rgba = " << int(px[0]) << "," << int(px[1]) << "," << int(px[2]));
  CHECK(int(px[2]) > 128);
  CHECK(int(px[1]) < 96);
}

TEST_CASE("a compute filter can MODIFY an attribute in flight",
          "[gfx][syn][csf][geometry]")
{
  // Upstream is green; the filter rewrites the colour to red. Red proves the
  // write landed, and green would prove the upstream buffer was forwarded past
  // it -- the two outcomes are opposite channels, not shades of one.
  const auto api = GENERATE(from_range(platform_backends()));
  IsfResult r;
  run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = render_raster(api,
                      {corpus("syn-geo-producer.cs"), corpus("syn-geo-modify-attribute.cs")},
                      corpus("raw-raster-basic.vs"), corpus("raw-raster-basic.fs"));
  });
  if(r.skipped) SKIP(r.backend + ": " + r.skip_reason);
  INFO("backend=" << r.backend << " error: " << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() >= 1);
  const auto px = r.outputs[0].at(32, 32);
  INFO("centre rgba = " << int(px[0]) << "," << int(px[1]) << "," << int(px[2]));
  CHECK(int(px[0]) > 128);
  CHECK(int(px[1]) < 96);
}

TEST_CASE("EXECUTION_MODEL SINGLE runs the pass exactly once",
          "[gfx][syn][raster][execution]")
{
  // The explicit form of the default. One pass means PASSINDEX 0, so red must
  // be 0 while green proves it drew: a looped pass would leave a higher index
  // behind, and no pass at all leaves both channels at 0.
  const auto api = GENERATE(from_range(platform_backends()));
  IsfResult r;
  run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = render_raster(api, {corpus("syn-geo-producer.cs")},
                      corpus("syn-raster-single.vs"), corpus("syn-raster-single.fs"));
  });
  if(r.skipped) SKIP(r.backend + ": " + r.skip_reason);
  INFO("backend=" << r.backend << " error: " << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() >= 1);
  const auto px = r.outputs[0].at(32, 32);
  INFO("centre rgba = " << int(px[0]) << "," << int(px[1]) << "," << int(px[2]));
  CHECK(int(px[1]) > 128);
  CHECK(int(px[0]) < 32);
}

// PER_CUBE_FACE is NOT tested here on purpose. The case was written and its
// shader is committed (syn-raster-per-cube-face.*), but render_raster cannot
// read back a cubemap attachment: the same output declared CUBEMAP reads black
// under EXECUTION_MODEL SINGLE too, so a failure here would accuse the wrong
// thing. Testing it needs either cubemap readback in the rig or a second pass
// sampling the cube into a 2D target.
