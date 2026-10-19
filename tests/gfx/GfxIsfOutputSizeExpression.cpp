// A classic ISF's OUTPUTS.WIDTH / HEIGHT `$` expressions size its targets, and
// follow the inputs they read. The simple renderer took only a literal pair
// and allocated at the render size otherwise (shader board F13), like the
// raw raster renderer before it (GfxRasterOutputSizeExpression.cpp).
#include "IsfTestCommon.hpp"

#include <Gfx/Graph/ISFNode.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>

#include <QFile>
#include <QTemporaryDir>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <string>

using namespace score::test::gfx;

namespace
{
constexpr const char* kSizedIsf = R"(/*{
  "ISFVSN": "2.0",
  "OUTPUTS": [
    { "NAME": "first", "WIDTH": "$size", "HEIGHT": "$size / 2" },
    { "NAME": "second", "WIDTH": "$size", "HEIGHT": "$size / 2" }
  ],
  "INPUTS": [
    { "NAME": "gain", "TYPE": "float", "DEFAULT": 0.5 },
    { "NAME": "size", "TYPE": "long", "DEFAULT": 32, "MIN": 2, "MAX": 512 }
  ]
}*/
void main()
{
  first = vec4(gain, 0.0, 0.0, 1.0);
  second = vec4(0.0, gain, 0.0, 1.0);
}
)";

QSize outputSize(GfxPipeline& p, int node)
{
  auto& isf = *p.isf(node);
  for(auto& [rl, renderer] : isf.renderedNodes)
    if(renderer)
      if(auto* tex = renderer->textureForOutput(*p.imageOut(node, 0)))
        return tex->pixelSize();
  return {};
}
}

TEST_CASE(
    "a classic ISF OUTPUTS size expression follows the input it reads",
    "[gfx][isf][outputs][F13]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString fs = dir.filePath("sized.fs");
  {
    QFile f(fs);
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write(kSizedIsf);
  }

  bool skipped = false;
  std::string err;
  QSize initial, resized;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int node = p.addIsf(fs);
    if(node < 0)
    {
      err = "build failed: " + p.error();
      return;
    }
    const int sink = p.addSink({64, 64});
    p.wire(p.imageOut(node, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      skipped = p.skipped();
      err = skipped ? std::string{} : p.error();
      return;
    }
    p.render(2);
    initial = outputSize(p, node);
    setControl(*p.isf(node), nth_control_input(*p.isf(node), 1), ossia::value{48});
    p.render(2);
    resized = outputSize(p, node);
    if(err.empty())
      err = p.error();
  });
  if(skipped)
    SKIP("backend unavailable");
  INFO("error=" << err);
  REQUIRE(err.empty());
  CAPTURE(initial.width(), initial.height(), resized.width(), resized.height());
  CHECK(initial == QSize(32, 16));
  CHECK(resized == QSize(48, 24));
}

// The size follows its inputs every frame, but the expression is compiled
// once: an exprtk compile per frame per consumer, as each frame rebuilt it.
TEST_CASE(
    "a classic ISF OUTPUTS size expression is compiled once, not every frame",
    "[gfx][isf][outputs][F13][perf]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString fs = dir.filePath("sized.fs");
  {
    QFile f(fs);
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write(kSizedIsf);
  }

  bool skipped = false;
  std::string err;
  uint64_t afterFew{}, afterMany{};
  QSize resized;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int node = p.addIsf(fs);
    if(node < 0)
    {
      err = "build failed: " + p.error();
      return;
    }
    const int sink = p.addSink({64, 64});
    p.wire(p.imageOut(node, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      skipped = p.skipped();
      err = skipped ? std::string{} : p.error();
      return;
    }
    p.render(3);
    afterFew = score::gfx::sizeExpressionCompilations().load();
    p.render(30);
    afterMany = score::gfx::sizeExpressionCompilations().load();
    // Still followed.
    setControl(*p.isf(node), nth_control_input(*p.isf(node), 1), ossia::value{48});
    p.render(2);
    resized = outputSize(p, node);
  });
  if(skipped)
    SKIP("backend unavailable");
  INFO("error=" << err);
  REQUIRE(err.empty());
  INFO("compilations after 3 frames " << afterFew << ", after 33 " << afterMany);
  CHECK(afterMany == afterFew);
  CHECK(resized == QSize(48, 24));
}
