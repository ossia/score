// =============================================================================
//   DISPLAY=:0 SCORE_TEST_API=opengl ctest -R gfx_incremental_findings
//   DISPLAY=:0 SCORE_TEST_API=vulkan ctest -R gfx_incremental_findings
// =============================================================================
#include "GfxIncrementalCommon.hpp"

using namespace score::test::gfx;
using namespace score::test::gfx::isf;
using namespace score::test::gfx::incremental;

TEST_CASE("FINDING add-new-output incremental (Vulkan leaks a render pass)", "[gfx][l3][incremental][finding]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  Shot out;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int a = p.addIsf(corpus("isf-solid-color.fs"));
    const int s0 = p.addSink({64, 64});
    const int s1 = p.addSink({64, 64});
    p.wire(p.imageOut(a, 0), p.sinkInput(s0));

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
    const int b = p.addIsf(corpus("isf-solid-color.fs"));
    p.addEdgeIncremental(p.imageOut(b, 0), p.sinkInput(s1));
    p.render(3);
    out.c = p.readback(s1);
  });

  if(out.skipped)
    SKIP(out.backend + ": " + out.skip_reason);
  INFO("backend=" << out.backend);
  REQUIRE(out.error.empty());
  REQUIRE(out.c.valid());
  CHECK(solid(out.c, {255, 0, 255, 255}, 2)); // the added output appears
}

TEST_CASE("FINDING resize after an incremental add crashes on Vulkan", "[gfx][l3][incremental][finding]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  Shot out;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int a = p.addIsf(corpus("isf-solid-color.fs"));
    const int s0 = p.addSink({64, 64});
    p.wire(p.imageOut(a, 0), p.sinkInput(s0));

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
    p.resizeSink(s0, {96, 48}); // <- Vulkan: SIGSEGV on the stale render pass
    p.render(3);
    p.resizeSink(s0, {40, 40});
    p.render(3);
    out.c = p.readback(s0);
  });

  if(out.skipped)
    SKIP(out.backend + ": " + out.skip_reason);
  INFO("backend=" << out.backend);
  REQUIRE(out.error.empty());
  REQUIRE(out.c.valid());
  CHECK(out.c.width == 40);
  CHECK(out.c.height == 40);
  CHECK(solid(out.c, {255, 0, 255, 255}, 2));
}
