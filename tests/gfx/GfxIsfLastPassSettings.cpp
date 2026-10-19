// A multipass ISF's last pass, when not persistent, rendered straight into the
// destination whatever its own WIDTH / HEIGHT / FORMAT / FILTER (shader board
// F05). A last pass that differs from the destination renders into its own
// target, which is then blitted: here an 8x8 NEAREST last pass, read back
// through a 64x64 sink, shows 8x8-pixel blocks.
#include "IsfTestCommon.hpp"

#include <QFile>
#include <QTemporaryDir>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

using namespace score::test::gfx;

namespace
{
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
    "a multipass ISF's last pass keeps its own size and filter",
    "[gfx][isf][passes][F05]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString fs = dir.filePath("lastpass.fs");
  {
    QFile f(fs);
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write(kTwoPass);
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
