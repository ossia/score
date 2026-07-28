// =============================================================================
//   DISPLAY=:0 SCORE_TEST_API=opengl ctest -R gfx_depth_sampler
//   DISPLAY=:0 SCORE_TEST_API=vulkan ctest -R gfx_depth_sampler
// =============================================================================
#include "GfxIncrementalCommon.hpp"

using namespace score::test::gfx;
using namespace score::test::gfx::isf;
using namespace score::test::gfx::incremental;

namespace
{
// Raw index into node->input of the k-th (0-based) Image port. The render_target_
// spec is keyed by the RAW input index (RenderList uses cur_port over all inputs).
int raw_image_input_index(const score::gfx::Node& n, int k)
{
  int seen = 0;
  for(std::size_t i = 0; i < n.input.size(); ++i)
    if(n.input[i]->type == score::gfx::Types::Image)
      if(seen++ == k)
        return int(i);
  return -1;
}

// Mean red over the left column band (x in [w/16, w/8)), middle rows.
int left_red(const ReadbackImage& img)
{
  long sum = 0;
  int n = 0;
  const int x0 = std::max(1, img.width / 16), x1 = std::max(x0 + 1, img.width / 8);
  for(int y = img.height / 4; y < 3 * img.height / 4; y += 2)
    for(int x = x0; x < x1; ++x)
    {
      sum += img.at(x, y)[0];
      ++n;
    }
  return n ? int(sum / n) : 0;
}
}

TEST_CASE(
    "updateInputSamplerFilter edits the right sampler across SamplableDepth",
    "[gfx][l3][incremental][depth-sampler]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  Shot out;
  int imgBPort = -1;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int pa = p.addIsf(corpus("isf-solid-color.fs")); // imgA producer (color)
    const int pb = p.addIsf(corpus("isf-gradient-x.fs"));   // imgB producer (ramp)
    const int b = p.addIsf(corpus("isf-depth-sampler-drift.fs"));
    const int s0 = p.addSink({64, 64});
    p.wire(p.imageOut(pa, 0), p.imageIn(b, 0)); // -> imgA (SamplableDepth)
    p.wire(p.imageOut(pb, 0), p.imageIn(b, 1)); // -> imgB
    p.wire(p.imageOut(b, 0), p.sinkInput(s0));

    if(!p.create(backend))
    {
      out.skipped = p.skipped();
      out.skip_reason = p.skipReason();
      out.backend = p.backend();
      out.error = p.error();
      return;
    }
    out.backend = p.backend();

    p.render(3);
    out.a = p.readback(s0); // baseline: imgB REPEAT -> left red near 0 (ramp)

    // Switch imgB to CLAMP_TO_EDGE. Same size + format as its current input RT
    // (state.renderSize == the 64x64 sink) so NO RT recreation happens and only
    // the sampler-filter path (updateInputSamplerFilter) runs.
    imgBPort = raw_image_input_index(*p.isf(b), 1);
    ossia::render_target_spec spec;
    spec.size = ossia::texture_size{64, 64};
    spec.address_u = ossia::texture_address_mode::CLAMP_TO_EDGE;
    spec.address_v = ossia::texture_address_mode::CLAMP_TO_EDGE;
    setRenderTargetSpec(*p.isf(b), imgBPort, spec);

    p.render(3);
    out.b = p.readback(s0); // imgB CLAMP (fix) -> left red high / REPEAT (bug) -> low
  });

  if(out.skipped)
    SKIP(out.backend + ": " + out.skip_reason);
  INFO("backend=" << out.backend << " imgBPort=" << imgBPort);
  REQUIRE(out.error.empty());

  REQUIRE(out.a.valid());
  REQUIRE(out.b.valid());
  const int baseLeft = left_red(out.a);
  const int afterLeft = left_red(out.b);
  INFO("left red: baseline=" << baseLeft << " after CLAMP switch=" << afterLeft);

  // Baseline REPEAT: the out-of-range sample wraps to the low end of the ramp.
  CHECK(baseLeft < 90);
  // After switching imgB to CLAMP, imgB's OWN sampler must change so the left
  // pins to the right edge (red ~ 1.0).
  CHECK(afterLeft > 180);
}
