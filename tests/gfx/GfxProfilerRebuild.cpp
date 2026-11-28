#include <score_test/Gfx.hpp>

#include <Gfx/Graph/NodeProfiler.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_range.hpp>

using namespace score::test::gfx;

TEST_CASE("profiling survives an output rebuilt and a profiler turned off and on", "[gfx][profiler]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  bool skipped{};
  std::string error;
  std::size_t before{}, afterResize{}, whileOff{}, afterOn{};
  bool cpuTimes{};
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    score::gfx::NodeProfiler prof;
    prof.setEnabled(true);
    GfxPipeline p;
    p.graph().setNodeProfiler(&prof);
    const int isf = p.addIsf(QStringLiteral(GFX_TEST_CORPUS_DIR "/isf-gradient-x.fs"));
    const int sink = p.addSink({64, 64});
    if(isf < 0)
    {
      error = p.error();
      return;
    }
    p.wire(p.imageOut(isf, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      skipped = p.skipped();
      error = skipped ? std::string{} : p.error();
      return;
    }

    p.render(8);
    before = prof.take().size();

    p.resizeSink(sink, {96, 48});
    p.render(8);
    const auto frames = prof.take();
    afterResize = frames.size();
    for(const auto& f : frames)
      for(const auto& n : f.nodes)
        cpuTimes |= n.cpu_ns > 0;

    prof.setEnabled(false);
    p.render(8);
    whileOff = prof.take().size();
    prof.setEnabled(true);
    p.render(8);
    afterOn = prof.take().size();
  });

  if(skipped)
    SKIP("backend unavailable");
  REQUIRE(error.empty());
  CHECK(before > 0);
  CHECK(afterResize > 0);
  CHECK(cpuTimes);
  CHECK(whileOff == 0);
  CHECK(afterOn > 0);
}
