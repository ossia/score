// Merge Geometries applies an input's transform to GPU-resident geometry.
//
// A CSF generator writes a viewport-covering triangle into GPU storage buffers
// (vec3 positions and +X normals, std430 16-byte stride). It feeds Merge
// Geometries port 0, whose renderer is handed a non-identity transform
// (rotate 90 degrees about Z, scale 0.5, translate x by 0.25), and a raw raster
// paints the merged normals as colour. The transformed triangle leaves the right
// and left edges of row 32 uncovered and turns the normal to +Y, so the painted
// colour goes from (255,128,128) to (128,255,128). Without a GPU-side bake the
// merged geometry still points at the untransformed buffers: full coverage, +X.
//
// The identity case is the positive control for the chain itself.

#include "IsfTestCommon.hpp"

#include <Gfx/Graph/MergeGeometriesNode.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>

#include <algorithm>
#include <cstdlib>

using namespace score::test::gfx;
using namespace score::test::gfx::isf;

namespace
{
ossia::transform3d rotatedScaledShifted()
{
  ossia::transform3d t;
  const float m[16] = {0, 0.5f, 0, 0, -0.5f, 0, 0, 0, 0, 0, 0.5f, 0, 0.25f, 0, 0, 1};
  std::copy_n(m, 16, t.matrix);
  return t;
}

struct Shot
{
  bool skipped{};
  std::string skip_reason;
  std::string backend;
  std::string error;
  ReadbackImage image;
  int mergeRenderers{};
};

Shot renderMerged(score::gfx::GraphicsApi api, const ossia::transform3d& transform)
{
  Shot out;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int producer = p.addIsf(corpus("mergegpu-b2-producer.cs"));
    const int merge = p.addNode(std::make_unique<score::gfx::MergeGeometriesNode>());
    const int raster
        = p.addRaster(corpus("mergegpu-b2-raster.vs"), corpus("mergegpu-b2-raster.fs"));
    const int sink = p.addSink({64, 64});
    p.wire(p.geometryOut(producer, 0), p.nodeGeometryIn(merge, 0));
    p.wire(p.nodeGeometryOut(merge, 0), p.geometryIn(raster, 0));
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));

    if(!p.create(api))
    {
      out.skipped = p.skipped();
      out.skip_reason = p.skipReason();
      out.error = p.error();
      out.backend = p.backend();
      return;
    }
    out.backend = p.backend();

    auto* mergeNode = p.node(merge);
    for(int f = 0; f < 6; f++)
    {
      for(auto& [rl, r] : mergeNode->renderedNodes)
        r->process(0, transform);
      p.render(1);
    }
    out.mergeRenderers = int(mergeNode->renderedNodes.size());
    out.image = p.readback(sink);
    out.error = p.error();
  });
  return out;
}

bool nearRgb(std::array<uint8_t, 4> px, int r, int g, int b, int tol = 6)
{
  return std::abs(px[0] - r) <= tol && std::abs(px[1] - g) <= tol
         && std::abs(px[2] - b) <= tol;
}

bool cleared(std::array<uint8_t, 4> px)
{
  return px[0] <= 8 && px[1] <= 8 && px[2] <= 8;
}

#define PX(img, x, y) \
  "(" << x << "," << y << ")=" << int(img.at(x, y)[0]) << "," << int(img.at(x, y)[1]) \
      << "," << int(img.at(x, y)[2])
}

TEST_CASE(
    "Merge Geometries transforms GPU-resident positions and normals",
    "[gfx][merge][gpu][b2]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  SECTION("identity transform passes the GPU geometry through")
  {
    const auto shot = renderMerged(backend, ossia::transform3d{});
    if(shot.skipped)
      SKIP(shot.backend + ": " + shot.skip_reason);
    if(const char* why = compute_shader_skip_reason(backend))
      SKIP(why);
    INFO("backend=" << shot.backend);
    REQUIRE(shot.error.empty());
    REQUIRE(shot.mergeRenderers >= 1);
    REQUIRE(shot.image.valid());
    const auto& img = shot.image;
    INFO(PX(img, 3, 32) << " " << PX(img, 40, 32) << " " << PX(img, 60, 32));
    CHECK(nearRgb(img.at(3, 32), 255, 128, 128));
    CHECK(nearRgb(img.at(40, 32), 255, 128, 128));
    CHECK(nearRgb(img.at(60, 32), 255, 128, 128));
  }

  SECTION("non-identity transform moves the triangle and turns its normals")
  {
    const auto shot = renderMerged(backend, rotatedScaledShifted());
    if(shot.skipped)
      SKIP(shot.backend + ": " + shot.skip_reason);
    if(const char* why = compute_shader_skip_reason(backend))
      SKIP(why);
    INFO("backend=" << shot.backend);
    REQUIRE(shot.error.empty());
    REQUIRE(shot.mergeRenderers >= 1);
    REQUIRE(shot.image.valid());
    const auto& img = shot.image;
    INFO(
        PX(img, 3, 32) << " " << PX(img, 20, 32) << " " << PX(img, 40, 20) << " "
                       << PX(img, 40, 32) << " " << PX(img, 40, 44) << " "
                       << PX(img, 60, 32));
    // Clip x = 0.89 and -0.89 on the middle row are outside the moved triangle.
    CHECK(cleared(img.at(60, 32)));
    CHECK(cleared(img.at(3, 32)));
    // Inside it, the +X normal became +Y.
    CHECK(nearRgb(img.at(40, 32), 128, 255, 128));
    CHECK(nearRgb(img.at(20, 32), 128, 255, 128));
    CHECK(nearRgb(img.at(40, 20), 128, 255, 128));
    CHECK(nearRgb(img.at(40, 44), 128, 255, 128));
  }
}
