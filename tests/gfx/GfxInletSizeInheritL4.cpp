// An unsized texture inlet takes the size of the inlets it renders into
// (agent L4).
//
// RenderList::resolveDownstreamSize resolves recursively from the inlet
// settings: the output node counts as the render size, a sized inlet as its
// size, an unsized inlet as what it resolves to in turn. With several
// downstream inlets the largest width and the largest height win. Pinned at
// creation (one level, three unsized levels, fan-out) and after a runtime
// size change of the downstream inlet, which the rt_changed path propagates
// upstream.
//
// Registration: see score_add_gfx_test(inlet_size_inherit_l4 ...).
#include <score_test/Gfx.hpp>

#include <Gfx/Graph/RenderList.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <string>
#include <vector>

using namespace score::test;
using namespace score::test::gfx;

namespace
{
QString corpus(const char* f)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR "/") + QString::fromUtf8(f);
}

ossia::render_target_spec sized(int w, int h)
{
  ossia::render_target_spec s;
  s.size = ossia::texture_size{w, h};
  return s;
}

struct Run
{
  bool skipped{};
  std::string skipReason;
  std::string error;
  std::vector<QSize> sizes;
  std::vector<QSize> sizesAfter;
  std::array<uint8_t, 4> center{};
  std::array<uint8_t, 4> centerAfter{};
};

QSize inletSize(GfxPipeline& p, int node)
{
  const auto& rls = p.graph().renderLists();
  if(rls.empty() || !rls.front())
    return {};
  if(auto* tex = rls.front()->renderTargetForInputPort(*p.imageIn(node, 0)).texture)
    return tex->pixelSize();
  return {};
}

void setSize(GfxPipeline& p, int node, int w, int h)
{
  setRenderTargetSpec(*p.isf(node), first_image_input(*p.isf(node)), sized(w, h));
}

// solid -> X1 .. Xn (unsized) -> Y (w x h) -> sink 64x64
Run runChain(score::gfx::GraphicsApi api, int unsized, QSize y, QSize yAfter = {})
{
  Run out;
  run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int solid = p.addIsf(corpus("isf-solid-color.fs"));
    std::vector<int> xs;
    for(int i = 0; i < unsized; i++)
      xs.push_back(p.addIsf(corpus("isf-passthrough-plain.fs")));
    const int yn = p.addIsf(corpus("isf-passthrough-plain.fs"));
    if(!p.error().empty())
    {
      out.error = p.error();
      return;
    }
    setSize(p, yn, y.width(), y.height());
    const int sink = p.addSink({64, 64});
    int prev = solid;
    for(int x : xs)
    {
      p.wire(p.imageOut(prev, 0), p.imageIn(x, 0));
      prev = x;
    }
    p.wire(p.imageOut(prev, 0), p.imageIn(yn, 0));
    p.wire(p.imageOut(yn, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      out.skipped = p.skipped();
      out.skipReason = p.skipReason();
      out.error = out.skipped ? std::string{} : p.error();
      return;
    }
    p.render(3);
    for(int x : xs)
      out.sizes.push_back(inletSize(p, x));
    out.sizes.push_back(inletSize(p, yn));
    if(auto img = p.readback(sink); img.valid())
      out.center = img.center();

    if(yAfter.isValid())
    {
      setSize(p, yn, yAfter.width(), yAfter.height());
      p.render(3);
      for(int x : xs)
        out.sizesAfter.push_back(inletSize(p, x));
      out.sizesAfter.push_back(inletSize(p, yn));
      if(auto img = p.readback(sink); img.valid())
        out.centerAfter = img.center();
    }
  });
  return out;
}

// solid -> X (unsized) -> Y1 (32x16) -> sink A, X -> Y2 (8x48) -> sink B
Run runFanOut(score::gfx::GraphicsApi api)
{
  Run out;
  run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int solid = p.addIsf(corpus("isf-solid-color.fs"));
    const int x = p.addIsf(corpus("isf-passthrough-plain.fs"));
    const int y1 = p.addIsf(corpus("isf-passthrough-plain.fs"));
    const int y2 = p.addIsf(corpus("isf-passthrough-plain.fs"));
    if(!p.error().empty())
    {
      out.error = p.error();
      return;
    }
    setSize(p, y1, 32, 16);
    setSize(p, y2, 8, 48);
    const int sink = p.addSink({64, 64});
    p.wire(p.imageOut(solid, 0), p.imageIn(x, 0));
    p.wire(p.imageOut(x, 0), p.imageIn(y1, 0));
    p.wire(p.imageOut(x, 0), p.imageIn(y2, 0));
    p.wire(p.imageOut(y1, 0), p.sinkInput(sink));
    p.wire(p.imageOut(y2, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      out.skipped = p.skipped();
      out.skipReason = p.skipReason();
      out.error = out.skipped ? std::string{} : p.error();
      return;
    }
    p.render(3);
    out.sizes = {inletSize(p, x), inletSize(p, y1), inletSize(p, y2)};
    if(auto img = p.readback(sink); img.valid())
      out.center = img.center();
  });
  return out;
}

std::string str(const std::vector<QSize>& v)
{
  std::string s;
  for(auto sz : v)
    s += std::to_string(sz.width()) + "x" + std::to_string(sz.height()) + " ";
  return s;
}

bool magenta(std::array<uint8_t, 4> c)
{
  return c[0] > 200 && c[1] < 40 && c[2] > 200;
}
}

TEST_CASE(
    "An unsized inlet inherits the size of the inlet it feeds",
    "[gfx][texture][inlet-settings][size][l4]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const Run r = runChain(api, 1, {32, 16});
  if(r.skipped)
    SKIP(r.skipReason);
  INFO("error=" << r.error << " sizes " << str(r.sizes));
  REQUIRE(r.error.empty());
  REQUIRE(r.sizes.size() == 2);
  CHECK(r.sizes[0] == QSize(32, 16));
  CHECK(r.sizes[1] == QSize(32, 16));
  if(api != score::gfx::Null)
    CHECK(magenta(r.center));
}

TEST_CASE(
    "Three unsized inlets in a row inherit the size at the end of the chain",
    "[gfx][texture][inlet-settings][size][l4]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const Run r = runChain(api, 3, {24, 40});
  if(r.skipped)
    SKIP(r.skipReason);
  INFO("error=" << r.error << " sizes " << str(r.sizes));
  REQUIRE(r.error.empty());
  REQUIRE(r.sizes.size() == 4);
  for(auto sz : r.sizes)
    CHECK(sz == QSize(24, 40));
  if(api != score::gfx::Null)
    CHECK(magenta(r.center));
}

TEST_CASE(
    "An unsized inlet feeding several inlets takes the largest width and height",
    "[gfx][texture][inlet-settings][size][l4]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const Run r = runFanOut(api);
  if(r.skipped)
    SKIP(r.skipReason);
  INFO("error=" << r.error << " sizes " << str(r.sizes));
  REQUIRE(r.error.empty());
  REQUIRE(r.sizes.size() == 3);
  CHECK(r.sizes[0] == QSize(32, 48));
  CHECK(r.sizes[1] == QSize(32, 16));
  CHECK(r.sizes[2] == QSize(8, 48));
  if(api != score::gfx::Null)
    CHECK(magenta(r.center));
}

TEST_CASE(
    "A runtime size change downstream resizes the unsized inlets feeding it",
    "[gfx][texture][inlet-settings][size][rtchanged][l4]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const Run r = runChain(api, 2, {32, 16}, {48, 24});
  if(r.skipped)
    SKIP(r.skipReason);
  INFO(
      "error=" << r.error << " sizes " << str(r.sizes) << " after "
               << str(r.sizesAfter));
  REQUIRE(r.error.empty());
  REQUIRE(r.sizes.size() == 3);
  REQUIRE(r.sizesAfter.size() == 3);
  for(auto sz : r.sizes)
    CHECK(sz == QSize(32, 16));
  for(auto sz : r.sizesAfter)
    CHECK(sz == QSize(48, 24));
  if(api != score::gfx::Null)
  {
    CHECK(magenta(r.center));
    CHECK(magenta(r.centerAfter));
  }
}
