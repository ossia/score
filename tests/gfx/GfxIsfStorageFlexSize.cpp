// A graphics storage buffer whose LAYOUT ends in a flexible array is sized by
// its synthesized "<name> size" control: fixed part + count x element stride
// (shader board U44). It was allocated as one element whatever the control
// said, so `data.v.length()` read 1 and every write past it went nowhere.
//
// isf-storage-flex-size.fs paints the length it sees: red for the default
// 1024, blue for 10, green for 1.
#include <score_test/Gfx.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

using namespace score::test::gfx;

namespace
{
// The size control is the node's first control input.
int sizePort(score::gfx::ISFNode& n)
{
  for(std::size_t i = 0; i < n.input.size(); i++)
    if(n.input[i]->type == score::gfx::Types::Int)
      return int(i);
  return -1;
}

IsfResult render(score::gfx::GraphicsApi be, std::optional<int> count)
{
  IsfResult r;
  r.backend = backend_name(be);
  GfxPipeline p;
  const int n = p.addIsf(QStringLiteral(GFX_TEST_CORPUS_DIR "/isf-storage-flex-size.fs"));
  if(n < 0)
  {
    r.error = p.error();
    return r;
  }
  const int s = p.addSink({16, 16});
  p.wire(p.imageOut(n, 0), p.sinkInput(s));
  if(!p.create(be))
  {
    r.skipped = p.skipped();
    r.skip_reason = p.skipReason();
    r.error = p.error();
    return r;
  }
  p.render(2);
  if(count)
  {
    const int port = sizePort(*p.isf(n));
    if(port < 0)
    {
      r.error = "no size control";
      return r;
    }
    setControl(*p.isf(n), port, *count);
    p.render(3);
  }
  r.outputs.push_back(p.readback(s));
  if(!r.outputs.back().valid())
    r.error = "empty readback";
  return r;
}
}

TEST_CASE(
    "a flexible-array graphics storage buffer follows its size control",
    "[gfx][isf][storage][U44]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  IsfResult def, ten;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    def = render(be, std::nullopt);
    ten = render(be, 10);
  });
  if(def.skipped)
    SKIP(def.backend << ": " << def.skip_reason);
  INFO("backend=" << def.backend << " error=" << def.error << " / " << ten.error);
  REQUIRE(def.error.empty());
  REQUIRE(ten.error.empty());
  const auto d = def.outputs.back().at(8, 8);
  const auto t = ten.outputs.back().at(8, 8);
  INFO("default=" << int(d[0]) << "," << int(d[1]) << "," << int(d[2]) << " resized="
                  << int(t[0]) << "," << int(t[1]) << "," << int(t[2]));
  // Default: 1024 elements.
  CHECK(near(def.outputs.back().at(8, 8), {255, 0, 0, 255}, 2));
  // Resized to 10 elements while running.
  CHECK(near(ten.outputs.back().at(8, 8), {0, 0, 255, 255}, 2));
}
