// A multipass ISF's last pass, when not persistent, rendered straight into the
// destination whatever its own WIDTH / HEIGHT / FORMAT / FILTER (shader board
// F05), and a single-pass ISF ignored them altogether (F04). A last pass that
// differs from the destination renders into its own target, which is then
// blitted: here an 8x8 NEAREST pass, read back through a 64x64 sink, shows
// 8x8-pixel blocks.
#include "IsfTestCommon.hpp"

#include <Gfx/Graph/RenderedISFNode.hpp>
#include <Gfx/Graph/SimpleRenderedISFNode.hpp>

#include <QFile>
#include <QTemporaryDir>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

using namespace score::test::gfx;

namespace
{
constexpr const char* kOnePass = R"(/*{
  "ISFVSN": "2.0",
  "INPUTS": [],
  "PASSES": [ { "WIDTH": "8", "HEIGHT": "8", "FILTER": "NEAREST" } ]
}*/
void main()
{
  gl_FragColor = vec4(isf_FragNormCoord.x, 0.0, 0.0, 1.0);
}
)";

constexpr const char* kTwoPass = R"(/*{
  "ISFVSN": "2.0",
  "INPUTS": [],
  "PASSES": [
    { "TARGET": "first" },
    { "WIDTH": "8", "HEIGHT": "8", "FILTER": "NEAREST" }
  ]
}*/
void main()
{
  if(PASSINDEX == 0)
    gl_FragColor = vec4(1.0);
  else
    gl_FragColor = vec4(isf_FragNormCoord.x, 0.0, 0.0, 1.0);
}
)";
}

TEST_CASE(
    "an ISF's last pass keeps its own size and filter",
    "[gfx][isf][passes][F05][F04]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const bool single = GENERATE(false, true);
  CAPTURE(backend_name(api), single);
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString fs = dir.filePath("lastpass.fs");
  {
    QFile f(fs);
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write(single ? kOnePass : kTwoPass);
  }
  IsfResult r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = render_isf_chain(api, {fs}, {64, 64}, 3);
  });
  if(r.skipped)
    SKIP(r.backend << ": " << r.skip_reason);
  INFO("backend=" << r.backend << " error=" << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(!r.outputs.empty());
  const auto& img = r.outputs.back();
  REQUIRE(img.valid());
  REQUIRE(img.width == 64);

  // Every 8-pixel run of a row holds one value; 8 distinct values across.
  int blocks_uniform = 0, distinct = 0;
  int previous = -1;
  for(int bx = 0; bx < 8; bx++)
  {
    const int v0 = img.at(bx * 8, 32)[0];
    bool uniform = true;
    for(int x = bx * 8; x < bx * 8 + 8; x++)
      uniform &= std::abs(int(img.at(x, 32)[0]) - v0) <= 1;
    blocks_uniform += uniform;
    distinct += v0 != previous;
    previous = v0;
  }
  CHECK(blocks_uniform == 8);
  CHECK(distinct == 8);
}

// FLOAT and FILTER change nothing in the pixels of a last pass drawn at the
// destination's size: one draw quantised into the destination, or into a
// float target then blitted at 1:1, is the same. Such a pass draws straight
// into the destination: a single pass on the simple renderer, a multipass
// shader's last pass without the blit. Both took an own target and a blit
// (16 bytes a pixel for FLOAT), which 61 library shaders declaring FLOAT paid.
TEST_CASE(
    "a last pass whose FLOAT or FILTER changes nothing draws into the destination",
    "[gfx][isf][passes][F05][F04][perf]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const std::string setting = GENERATE(
      std::string{R"("FLOAT": true)"}, std::string{R"("FILTER": "NEAREST")"});
  const bool single = GENERATE(false, true);
  CAPTURE(backend_name(api), setting, single);
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString fs = dir.filePath("lastpass-direct.fs");
  {
    QFile f(fs);
    REQUIRE(f.open(QIODevice::WriteOnly));
    const std::string passes = single ? "[ { " + setting + " } ]"
                                      : R"([ { "TARGET": "first" }, { )" + setting + " } ]";
    f.write(QByteArray::fromStdString(R"(/*{
  "ISFVSN": "2.0",
  "INPUTS": [],
  "PASSES": )" + passes + R"(
}*/
void main() { gl_FragColor = vec4(isf_FragNormCoord.x, 0.0, 0.0, 1.0); }
)"));
  }
  bool skipped = false;
  bool simple = false;
  int blitted = -1;
  std::string err;
  ReadbackImage img;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int node = p.addIsf(fs);
    const int sink = p.addSink({64, 64});
    if(node < 0)
    {
      err = "build failed: " + p.error();
      return;
    }
    p.wire(p.imageOut(node, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      skipped = p.skipped();
      err = skipped ? std::string{} : p.error();
      return;
    }
    p.render(3);
    img = p.readback(sink);
    auto* n = p.isf(node);
    if(n->renderedNodes.empty())
    {
      err = "no renderer";
      return;
    }
    auto* r = n->renderedNodes.begin()->second;
    simple = dynamic_cast<score::gfx::SimpleRenderedISFNode*>(r) != nullptr;
    if(auto* multi = dynamic_cast<score::gfx::RenderedISFNode*>(r))
      blitted = multi->blittedEdges();
  });
  if(skipped)
    SKIP("backend unavailable");
  INFO(err);
  REQUIRE(err.empty());
  REQUIRE(img.valid());
  // The gradient is drawn.
  CHECK(int(img.at(8, 32)[0]) < int(img.at(56, 32)[0]));
  if(single)
    CHECK(simple);
  else
    CHECK(blitted == 0);
}
