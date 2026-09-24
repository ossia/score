// A texture wired into ModelDisplay is read the same way up on every backend.
//
// The source image is red over blue in ISF's own coordinates, and ModelDisplay
// maps it onto a cube by its texture coordinates, so the colour at the top of
// the cube's front face names which way the texture was read.
#include "GfxHalpNodes.hpp"
#include "IsfTestCommon.hpp"

#include <score_test/Document.hpp>

#include <Threedim/ModelDisplay/ModelDisplayNode.hpp>
#include <Threedim/Primitive.hpp>

#include <Crousti/CpuAnalysisNode.hpp>
#include <Crousti/CpuFilterNode.hpp>
#include <Crousti/GfxNode.hpp>
#include <Crousti/ProcessModel.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <algorithm>

using namespace score::test;
using namespace score::test::gfx;
using score::test::gfx::isf::corpus;

namespace
{
enum class Hue
{
  None,
  Red,
  Blue
};

Hue hue(const std::array<uint8_t, 4>& px)
{
  if(px[0] > 128 && px[2] < 64)
    return Hue::Red;
  if(px[2] > 128 && px[0] < 64)
    return Hue::Blue;
  return Hue::None;
}

struct Shot
{
  bool skipped{};
  std::string error;
  ReadbackImage img;
};
}

TEST_CASE(
    "ModelDisplay reads its texture the same way up on every backend",
    "[gfx][threedim][modeldisplay][texture]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  Shot s;
  run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto* doc = new_document(app);
    if(!doc)
    {
      s.error = "no document";
      return;
    }
    HalpProcesses procs;
    GfxPipeline p;
    const int image = p.addIsf(corpus("syn-red-over-blue.fs"));
    const int cube = p.addNode(procs.make<Threedim::Cube>(doc->context()));
    auto md = std::make_unique<score::gfx::ModelDisplayNode>();
    md->texture_projection = 0;
    md->position = {0.f, 0.f, 2.5f};
    md->center = {0.f, 0.f, 0.f};
    md->fov = 60.f;
    auto* mdNode = md.get();
    const int display = p.addNode(std::move(md));
    if(image < 0 || cube < 0 || display < 0)
    {
      s.error = "node build failed: " + p.error();
      return;
    }
    p.wire(p.imageOut(image, 0), mdNode->input[0]);
    p.wire(p.nodeSceneOut(cube, 0), mdNode->input[1]);
    const int sink = p.addSink({96, 96});
    p.wire(p.nodeImageOut(display, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      s.skipped = p.skipped();
      s.error = s.skipped ? std::string{} : p.error();
      return;
    }
    p.render(4);
    s.img = p.readback(sink);
    if(!s.img.valid())
      s.error = "empty readback";
  });
  if(s.skipped)
    SKIP("backend unavailable");
  INFO("error=" << s.error);
  REQUIRE(s.error.empty());

  int top = -1, bottom = -1, left = s.img.width, right = -1;
  for(int y = 0; y < s.img.height; y++)
    for(int x = 0; x < s.img.width; x++)
      if(hue(s.img.at(x, y)) != Hue::None)
      {
        if(top < 0)
          top = y;
        bottom = y;
        left = std::min(left, x);
        right = std::max(right, x);
      }
  INFO("face rows " << top << ".." << bottom << " cols " << left << ".." << right);
  REQUIRE(top >= 0);
  REQUIRE(bottom - top > 8);
  const int cx = (left + right) / 2;

  const int span = bottom - top;
  const auto upper = hue(s.img.at(cx, top + span / 4));
  const auto lower = hue(s.img.at(cx, bottom - span / 4));
  INFO("upper " << int(upper) << " lower " << int(lower));
  CHECK(upper == Hue::Red);
  CHECK(lower == Hue::Blue);
}
