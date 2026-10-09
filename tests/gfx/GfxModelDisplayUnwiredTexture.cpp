// A ModelDisplay with nothing wired into its texture inlet draws its mesh white.
//
// The inlet still has a render target of its own, which nothing draws into, so
// sampling it painted the mesh black on a black background in every projection
// that reads the texture.
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

namespace
{
struct Shot
{
  bool skipped{};
  std::string error;
  ReadbackImage img;
};

// Tex. Proj. values that sample the texture and nothing else.
enum TexProj
{
  TexCoord = 0,
  ViewSpace = 4,
  Barycentric = 5
};
}

TEST_CASE(
    "ModelDisplay draws a mesh white when no texture is wired",
    "[gfx][threedim][modeldisplay][texture]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const int proj = GENERATE(TexCoord, ViewSpace, Barycentric);
  CAPTURE(backend_name(api), proj);

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
    const int cube = p.addNode(procs.make<Threedim::Cube>(doc->context()));
    auto md = std::make_unique<score::gfx::ModelDisplayNode>();
    md->texture_projection = proj;
    md->position = {0.f, 0.f, 2.5f};
    md->center = {0.f, 0.f, 0.f};
    md->fov = 60.f;
    auto* mdNode = md.get();
    const int display = p.addNode(std::move(md));
    if(cube < 0 || display < 0)
    {
      s.error = "node build failed: " + p.error();
      return;
    }
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

  // The cube spans [0, 1] on each axis, so its front face is off the frame's
  // centre: sample the middle of what was drawn. Black drawn on black leaves
  // nothing to find.
  int left = s.img.width, right = -1, top = s.img.height, bottom = -1;
  for(int y = 0; y < s.img.height; y++)
    for(int x = 0; x < s.img.width; x++)
    {
      const auto p = s.img.at(x, y);
      if(p[0] + p[1] + p[2] > 60)
      {
        left = std::min(left, x);
        right = std::max(right, x);
        top = std::min(top, y);
        bottom = std::max(bottom, y);
      }
    }
  INFO("drawn cols " << left << ".." << right << " rows " << top << ".." << bottom);
  REQUIRE(right - left > 8);
  REQUIRE(bottom - top > 8);

  const auto px = s.img.at((left + right) / 2, (top + bottom) / 2);
  INFO("face centre " << int(px[0]) << " " << int(px[1]) << " " << int(px[2]));
  CHECK(px[0] > 230);
  CHECK(px[1] > 230);
  CHECK(px[2] > 230);
}
