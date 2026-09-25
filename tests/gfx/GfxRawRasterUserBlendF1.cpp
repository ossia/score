// Agent F1: a raw raster's "Enable blend" factors mean the same thing whatever
// its COMPOSITE is.
//
// ALPHA straight with COMPOSITE multiply or screen makes libisf wrap the
// fragment shader so it writes rgb * a: those composites need premultiplied
// colour. The wrapper is decided from the descriptor alone, so it still runs
// when the user switches the node's own blend on, and the default user blend
// (SrcAlpha, OneMinusSrcAlpha) then multiplied the colour by alpha a second
// time. The engine now turns a SrcAlpha colour source into One on such an
// output, which is exactly the user's straight-alpha blend applied to the
// colour the wrapper already premultiplied.
//
// Each shader writes (0.8, 0.4, 0.2, 0.5); the COMPOSITE over variant, which
// the wrapper leaves alone, is the reference.
#include <score_test/Gfx.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <array>
#include <cstdlib>

using namespace score::test::gfx;

namespace
{
QString corpus(const std::string& file)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR) + QStringLiteral("/")
         + QString::fromStdString(file);
}

struct Shot
{
  bool skipped{};
  std::string error;
  std::array<uint8_t, 4> centre{};
};

Shot draw(score::gfx::GraphicsApi be, const std::string& base)
{
  Shot s;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int node = p.addRaster(corpus(base + ".vs"), corpus(base + ".fs"));
    if(node < 0)
    {
      s.error = "node build failed: " + p.error();
      return;
    }
    const int sink = p.addSink({32, 32});
    p.wire(p.imageOut(node, 0), p.sinkInput(sink));
    if(!p.create(be))
    {
      s.skipped = p.skipped();
      s.error = s.skipped ? std::string{} : p.error();
      return;
    }
    auto& isf = *p.isf(node);
    setControl(isf, nth_control_input(isf, 1), true);
    p.render(3);
    const auto img = p.readback(sink);
    if(!img.valid())
    {
      s.error = "readback failed";
      return;
    }
    s.centre = img.at(img.width / 2, img.height / 2);
  });
  return s;
}
}

TEST_CASE(
    "F1: a raw raster's user blend is not applied to engine-premultiplied colour "
    "twice",
    "[gfx][raw_raster][alpha][blend][f1]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  const std::string composite = GENERATE("multiply", "screen");
  CAPTURE(backend_name(be), composite);

  const Shot ref = draw(be, "f1-rr-blend-over");
  if(ref.skipped)
    SKIP("backend unavailable");
  const Shot got = draw(be, "f1-rr-blend-" + composite);

  INFO("ref error=" << ref.error << " error=" << got.error);
  REQUIRE(ref.error.empty());
  REQUIRE(got.error.empty());
  INFO(
      "reference=(" << int(ref.centre[0]) << "," << int(ref.centre[1]) << ","
                    << int(ref.centre[2]) << "," << int(ref.centre[3]) << ")");
  INFO(
      "centre=(" << int(got.centre[0]) << "," << int(got.centre[1]) << ","
                 << int(got.centre[2]) << "," << int(got.centre[3]) << ")");
  CHECK(ref.centre[0] > 60);
  for(int c = 0; c < 4; c++)
    CHECK(std::abs(int(got.centre[c]) - int(ref.centre[c])) <= 2);
}
