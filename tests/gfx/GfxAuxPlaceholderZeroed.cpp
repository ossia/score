// An unbound top-level AUXILIARY block must read back as zeros -- across a
// RenderList rebuild, not just on the first frame.
//
// A RAW_RASTER_PIPELINE shader can declare AUXILIARY blocks that the user's
// graph has no producer for. RenderedRawRasterPipelineNode then allocates a
// placeholder so the descriptor set is valid (initPass / initMRTPass, the
// `if(!aux.buffer)` branches). Shaders read those placeholders as SENTINELS:
// classic_pbr_openpbr gates its clustered-lighting and volumetric paths on
//     bool clustered = (cluster_config.cluster_x > 0u);
// and then indexes cluster_light_counts / cluster_light_lists /
// vol_integrated with an id derived from that grid -- so a nonzero sentinel in
// a 16-byte placeholder is a multi-gigabyte out-of-bounds read.
//
// The placeholders were created and never written. Vulkan does not initialise
// VkBuffer memory, so on a rebuild the fresh placeholder lands on whatever the
// previous owner of that suballocation left there. Measured directly on an RTX
// 4090 (Qt 6.9.1 QRhi, no score involved): allocate a 256-byte Dynamic UBO,
// fill it with 0x5C, destroy it, then create a new one and read it back --
// 14 of 64 fresh, never-uploaded placeholders came back 0x5C5C5C5C.
//
// IsfBindingsBuilder::ensureStorageResources already zero-fills the
// INPUTS-side placeholders for exactly this reason (its comment names
// cluster_light_counts / cluster_light_lists). The top-level AUXILIARY path
// never got the same treatment.
//
// This test builds a raster node with two producerless AUXILIARY blocks (one
// std430 SSBO, one std140 UBO), then resizes the sink repeatedly.
// GfxPipeline::resizeSink drives Graph::recreateOutputRenderList -- the same
// teardown+rebuild a live window resize takes -- so each round frees every
// buffer in the list and allocates a fresh placeholder over the recycled
// memory. The shader paints green while every sentinel reads 0 and red as soon
// as one does not.
//
// Measured on this box (NVIDIA RTX 4090, Vulkan), with the zero-fill reverted:
// the readback goes red. See the commit message for both directions.

#include <score_test/Gfx.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <cstdio>

using namespace score::test::gfx;

namespace
{
QString corpus(const char* file)
{
  return QString{GFX_TEST_CORPUS_DIR "/"} + file;
}

// Largest per-channel value anywhere in the image.
std::array<int, 4> channel_max(const ReadbackImage& img)
{
  std::array<int, 4> m{0, 0, 0, 0};
  for(int y = 0; y < img.height; ++y)
    for(int x = 0; x < img.width; ++x)
    {
      const auto p = img.at(x, y);
      for(int c = 0; c < 4; ++c)
        m[c] = std::max(m[c], int(p[c]));
    }
  return m;
}
}

TEST_CASE(
    "an unbound AUXILIARY placeholder stays zero across RenderList rebuilds",
    "[gfx][l3][raster][binding][regression]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(be));

  constexpr int rounds = 12;

  bool built = false;
  bool skipped = false;
  std::string err;
  // Positive control: how many rounds actually produced a readback with
  // geometry in it. A round that drew nothing cannot observe the sentinel, so
  // the test must not pass on an all-blank run.
  int rounds_with_geometry = 0;
  int rounds_red = 0;
  bool initial_verdict = false;
  bool initial_red = false;
  std::vector<std::array<int, 4>> maxima;

  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int geo = p.addIsf(corpus("csf-vertex-count-expr.cs"));
    const int node
        = p.addRaster(corpus("raw-raster-aux-unbound.vs"),
                      corpus("raw-raster-aux-unbound.fs"));
    if(geo < 0 || node < 0)
    {
      err = "node build failed: " + p.error();
      return;
    }

    const int sink = p.addSink({64, 64});
    p.wire(p.geometryOut(geo, 0), p.geometryIn(node, 0));
    p.wire(p.imageOut(node, 0), p.sinkInput(sink));

    if(!p.create(be))
    {
      if(p.skipped())
      {
        skipped = true;
        return;
      }
      err = p.error();
      return;
    }
    built = true;

    // Round -1: the very first RenderList build, before any rebuild. Recorded
    // separately because it is the one allocation that can still land on
    // fresh (driver-zeroed) device memory.
    p.render(3);
    if(const auto img0 = p.readback(sink); img0.valid())
    {
      const auto m0 = channel_max(img0);
      initial_verdict = (m0[1] > 40 || m0[0] > 40);
      initial_red = (m0[0] > 40);
    }

    for(int r = 0; r < rounds; ++r)
    {
      // Alternate the size so every round is a real rebuild, not a no-op.
      p.resizeSink(sink, (r % 2 == 0) ? QSize{80, 80} : QSize{64, 64});
      p.render(3);

      const auto img = p.readback(sink);
      if(!img.valid())
        continue;
      const auto m = channel_max(img);
      maxima.push_back(m);
      // Green OR red anywhere == the shader ran on real geometry this round
      // and reached a verdict. A blank round observed nothing.
      if(m[1] > 40 || m[0] > 40)
        ++rounds_with_geometry;
      if(m[0] > 40)
        ++rounds_red;
    }
  });

  if(skipped || (!built && err.empty()))
    SKIP("backend unavailable");

  INFO("backend=" << backend_name(be) << " error=" << err);
  REQUIRE(err.empty());
  REQUIRE(built);

  for(std::size_t i = 0; i < maxima.size(); ++i)
    INFO(
        "round " << i << " channel max = (" << maxima[i][0] << "," << maxima[i][1]
                 << "," << maxima[i][2] << "," << maxima[i][3] << ")");

  // The verdict, then the positive control. Both are reported even when the
  // first one fails, so a red run is never confused with a blank one.
  INFO(
      "rounds where a sentinel came back nonzero (RED): " << rounds_red << " / "
                                                          << rounds);
  INFO(
      "rounds that drew a verdict at all (positive control): "
      << rounds_with_geometry << " / " << rounds);
  INFO(
      "first build (no rebuild yet): verdict drawn=" << initial_verdict
                                                   << " red=" << initial_red);
  CHECK(rounds_red == 0);
  // Without geometry on screen nothing read the sentinel and a green verdict
  // would be vacuous.
  CHECK(rounds_with_geometry == rounds);
}
