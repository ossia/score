// A graphics ISF's storage image was allocated at the render size whatever
// its WIDTH / HEIGHT (shader board U45). Here the fragment shader paints the
// imageSize() of an image declared WIDTH "$size", HEIGHT "12": (24, 12), not
// the 64x64 render size.
#include "IsfTestCommon.hpp"

#include <QFile>
#include <QTemporaryDir>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

using namespace score::test::gfx;

namespace
{
constexpr const char* kSized = R"(/*{
  "ISFVSN": "2.0",
  "INPUTS": [
    { "NAME": "size", "TYPE": "long", "DEFAULT": 24, "MIN": 1, "MAX": 255 },
    { "NAME": "scratch", "TYPE": "image", "ACCESS": "read_write", "FORMAT": "rgba8",
      "WIDTH": "$size", "HEIGHT": "12" }
  ]
}*/
void main()
{
  ivec2 s = imageSize(scratch);
  gl_FragColor = vec4(float(s.x) / 255.0, float(s.y) / 255.0, 0.0, 1.0);
}
)";
}

TEST_CASE("a graphics ISF storage image has its declared size", "[gfx][isf][storage][U45]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString fs = dir.filePath("sized_image.fs");
  {
    QFile f(fs);
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write(kSized);
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
  const auto px = r.outputs.back().center();
  CAPTURE(int(px[0]), int(px[1]));
  CHECK(int(px[0]) == 24);
  CHECK(int(px[1]) == 12);
}
