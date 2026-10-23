// A particle pipeline built entirely on the GPU never reads a buffer back to
// the CPU. This is the guarantee behind TouchDesigner POP / Notch style
// chains: the particle count, the dispatch sizes and the draw count are all
// decided on the GPU, and the state travels geometry, buffer and texture
// feedback edges, with no frame-time stall.
//
// The chain (corpus syn-pop-*):
//   emit (owns an 8-slot pool, one more slot alive per frame, decided only
//         from the fed-back pool)
//     -> force (counts live slots into indirect dispatch arguments, then an
//              INDIRECT pass brightens each live slot)
//     -> Delayed geometry cable back to emit
//     -> draw (GPU-written commands + GPU-written draw count; dead slots
//             carry poison commands that paint blue on columns 32..63)
//     -> raw raster -> sink
//                   -> trail (PERSISTENT ISF, adds 1/16 of the input per
//                            frame) -> sink
//
// score::gfx::readbackStats() counts the buffer readbacks the graph issues.
// On a backend with GPU indirect draw it must not move. With
// SCORE_GFX_NO_GPU_INDIRECT the raw raster falls back to reading the
// commands back every frame: the counter must move and the picture must stay
// the same, which shows the counter is wired to the readback site.
//
// The library chain (shaderlib/pop TestEmit -> PopToSprites -> SphereSplat,
// with TestEmit's Delayed self-cable) gets the same zero-readback check, and
// a negative control without the feedback cable. Skips without
// SCORE_TEST_LIBRARY_ROOT.
//
//   DISPLAY=:0 SCORE_TEST_API=vulkan ctest -R gfx_gpu_only_particles
//   DISPLAY=:0 SCORE_TEST_API=opengl ctest -R gfx_gpu_only_particles
#include "GfxLogCapture.hpp"
#include "GfxUserLibrary.hpp"
#include "IsfTestCommon.hpp"

#include <Gfx/Graph/ReadbackStats.hpp>

#include <ossia/network/value/value.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <array>
#include <string>

using namespace score::test::gfx;
using namespace score::test::gfx::isf;

namespace
{
constexpr int kSize = 64;

struct Strips
{
  int live = 0;        // strips drawn with the R marker, from column 0
  bool gap = false;    // a drawn strip after an undrawn one
  bool poison = false; // any blue on columns 32..63
  std::array<int, 8> green{};
};

Strips strips(const ReadbackImage& img)
{
  Strips s;
  if(!img.valid())
    return s;
  bool ended = false;
  for(int q = 0; q < 8; ++q)
  {
    const auto px = img.at(4 * q + 2, kSize / 2);
    const bool drawn = px[0] > 200;
    s.green[q] = px[1];
    if(drawn && ended)
      s.gap = true;
    if(drawn && !ended)
      ++s.live;
    if(!drawn)
      ended = true;
  }
  for(int x = 32; x < kSize; ++x)
    for(int y = 0; y < kSize; y += 8)
      if(img.at(x, y)[2] > 100)
        s.poison = true;
  return s;
}

struct ChainShot
{
  bool skipped = false;
  std::string skip_reason, error, backend;
  bool gpuIndirect = false;
  uint64_t dataReadbacks = 0;
  ReadbackImage rasterA, rasterB, rasterC;
  ReadbackImage trailA, trailB;
};

ChainShot run_chain(score::gfx::GraphicsApi be, bool forceCpuFallback)
{
  ChainShot r;
  if(forceCpuFallback)
    qputenv("SCORE_GFX_NO_GPU_INDIRECT", "1");
  else
    qunsetenv("SCORE_GFX_NO_GPU_INDIRECT");

  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    LogCapture log;
    GfxPipeline p;
    const int emit = p.addCsf(corpus("syn-pop-emit.cs"));
    const int force = p.addCsf(corpus("syn-pop-force.cs"));
    const int draw = p.addCsf(corpus("syn-pop-draw.cs"));
    const int raster
        = p.addRaster(corpus("syn-vertex-color.vs"), corpus("syn-vertex-color.fs"));
    const int trail = p.addIsf(corpus("syn-pop-trail.fs"));
    if(emit < 0 || force < 0 || draw < 0 || raster < 0 || trail < 0)
    {
      r.error = "chain build failed: " + p.error();
      return;
    }
    const int rasterSink = p.addSink({kSize, kSize});
    const int trailSink = p.addSink({kSize, kSize});
    p.wire(p.geometryOut(emit, 0), p.geometryIn(force, 0));
    p.wire(p.geometryOut(force, 0), p.geometryIn(draw, 0));
    p.wireFeedback(p.geometryOut(force, 0), p.geometryIn(emit, 0));
    p.wire(p.geometryOut(draw, 0), p.geometryIn(raster, 0));
    p.wire(p.imageOut(raster, 0), p.sinkInput(rasterSink));
    p.wire(p.imageOut(raster, 0), p.imageIn(trail, 0));
    p.wire(p.imageOut(trail, 0), p.sinkInput(trailSink));

    if(!p.create(be))
    {
      r.skipped = p.skipped();
      r.skip_reason = p.skipReason();
      r.backend = p.backend();
      r.error = r.skipped ? std::string{} : p.error();
      return;
    }
    r.backend = p.backend();

    const uint64_t before = score::gfx::readbackStats().data.load();
    p.render(3);
    r.rasterA = p.readback(rasterSink);
    r.trailA = p.readback(trailSink);
    p.render(3);
    r.rasterB = p.readback(rasterSink);
    r.trailB = p.readback(trailSink);
    p.render(20);
    r.rasterC = p.readback(rasterSink);
    r.dataReadbacks = score::gfx::readbackStats().data.load() - before;
    r.gpuIndirect = log.count({u"RHI indirect caps: drawIndirect=1"}) > 0;
    if(r.error.empty())
      r.error = p.error();
  });
  qunsetenv("SCORE_GFX_NO_GPU_INDIRECT");
  return r;
}

void check_picture(const ChainShot& r)
{
  const Strips a = strips(r.rasterA), b = strips(r.rasterB), c = strips(r.rasterC);
  INFO("live: " << a.live << " -> " << b.live << " -> " << c.live);
  // The live count grows one slot per frame, decided on the GPU from the
  // fed-back pool, and the draw takes exactly that count.
  CHECK(a.live >= 1);
  CHECK(a.live < 8);
  CHECK(b.live > a.live);
  CHECK(c.live == 8);
  CHECK_FALSE(a.gap);
  CHECK_FALSE(b.gap);
  CHECK_FALSE(a.poison);
  CHECK_FALSE(b.poison);
  CHECK_FALSE(c.poison);
  // The indirectly dispatched force keeps brightening live slots through the
  // loop: the oldest slot gets brighter over time and is brighter than the
  // newest one.
  INFO("slot 0 green: " << a.green[0] << " -> " << b.green[0] << " -> " << c.green[0]);
  CHECK(b.green[0] > a.green[0]);
  CHECK(c.green[0] > b.green[0]);
  CHECK(b.green[0] > b.green[b.live - 1]);
  // Texture feedback: the trail of slot 0 keeps adding up.
  const auto ta = r.trailA.at(2, kSize / 2), tb = r.trailB.at(2, kSize / 2);
  INFO("trail slot 0 red: " << int(ta[0]) << " -> " << int(tb[0]));
  CHECK(tb[0] > ta[0] + 20);
}
}

TEST_CASE(
    "a GPU-only particle chain reads nothing back to the CPU",
    "[gfx][csf][feedback][indirect][U42]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  const ChainShot r = run_chain(api, false);
  if(r.skipped)
    SKIP("backend unavailable: " << r.skip_reason);
  INFO(r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.rasterA.valid());
  if(!r.gpuIndirect)
    SKIP(r.backend << " has no GPU indirect draw: the CPU readback fallback is its documented path");

  check_picture(r);
  CHECK(r.dataReadbacks == 0);
}

TEST_CASE(
    "the readback counter sees the raw raster's CPU fallback",
    "[gfx][csf][feedback][indirect][U42]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  const ChainShot r = run_chain(api, true);
  if(r.skipped)
    SKIP("backend unavailable: " << r.skip_reason);
  INFO(r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.rasterA.valid());
  CHECK_FALSE(r.gpuIndirect);

  // Same picture, read back every frame: 26 frames, commands and count.
  check_picture(r);
  CHECK(r.dataReadbacks >= 26);
}

namespace
{
// Ports of one type, in creation order, and the k-th of them.
int nth_port(const score::gfx::Node& n, score::gfx::Types t, int k)
{
  int seen = 0;
  for(int i = 0; i < int(n.input.size()); ++i)
    if(n.input[i]->type == t && seen++ == k)
      return i;
  return -1;
}

// A float or point3D control by name: each kind creates its ports in
// declaration order.
void set_by_name(score::gfx::ISFNode& n, std::string_view name, const ossia::value& v)
{
  int floats = 0, vec3s = 0;
  for(const auto& in : n.descriptor().inputs)
  {
    if(ossia::get_if<::isf::float_input>(&in.data))
    {
      if(in.name == name)
        setControl(n, nth_port(n, score::gfx::Types::Float, floats), v);
      ++floats;
    }
    else if(ossia::get_if<::isf::point3d_input>(&in.data))
    {
      if(in.name == name)
        setControl(n, nth_port(n, score::gfx::Types::Vec3, vec3s), v);
      ++vec3s;
    }
  }
}

int lit_pixels(const ReadbackImage& img)
{
  int n = 0;
  if(!img.valid())
    return 0;
  for(int y = 0; y < img.height; ++y)
    for(int x = 0; x < img.width; ++x)
    {
      const auto p = img.at(x, y);
      if(p[0] + p[1] + p[2] > 120)
        ++n;
    }
  return n;
}

struct LibraryShot
{
  bool skipped = false;
  std::string skip_reason, error;
  uint64_t dataReadbacks = 0;
  ReadbackImage img;
};

LibraryShot run_library(score::gfx::GraphicsApi be, bool feedback)
{
  LibraryShot r;
  qunsetenv("SCORE_GFX_NO_GPU_INDIRECT");
  const QString emitPath = library::find(QStringLiteral("shaderlib/pop/TestEmit.cs"));
  const QString spritesPath = library::find(QStringLiteral("shaderlib/pop/PopToSprites.cs"));
  const QString splatVs = library::find(QStringLiteral("shaderlib/renderers/SphereSplat.vs"));
  const QString splatFs = library::find(QStringLiteral("shaderlib/renderers/SphereSplat.fs"));
  if(emitPath.isEmpty() || spritesPath.isEmpty() || splatVs.isEmpty() || splatFs.isEmpty())
  {
    r.skipped = true;
    r.skip_reason = library::skip_reason(QStringLiteral("shaderlib/pop/TestEmit.cs"));
    return r;
  }
  score::test::run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    library::RootGuard root{ctx};
    GfxPipeline p;
    const int emit = p.addCsf(emitPath);
    const int sprites = p.addCsf(spritesPath);
    const int splat = p.addRaster(splatVs, splatFs);
    if(emit < 0 || sprites < 0 || splat < 0)
    {
      r.error = "chain build failed: " + p.error();
      return;
    }
    const int sink = p.addSink({128, 128});
    if(feedback)
      p.wireFeedback(p.geometryOut(emit, 0), p.geometryIn(emit, 0));
    p.wire(p.geometryOut(emit, 0), p.geometryIn(sprites, 0));
    p.wire(p.geometryOut(sprites, 0), p.geometryIn(splat, 0));
    p.wire(p.imageOut(splat, 0), p.sinkInput(sink));
    if(!p.create(be))
    {
      r.skipped = p.skipped();
      r.skip_reason = p.skipReason();
      r.error = r.skipped ? std::string{} : p.error();
      return;
    }
    // As the pop-testemit showcase sets it: 64 slots, framed by the camera.
    // TestEmit's "geoOut vertex count" ($USER) is its first int port.
    setControl(*p.isf(emit), nth_port(*p.isf(emit), score::gfx::Types::Int, 0), ossia::value{64});
    set_by_name(*p.isf(emit), "spacing", ossia::value{0.06f});
    set_by_name(*p.isf(emit), "size", ossia::value{0.04f});
    set_by_name(*p.isf(splat), "cameraPos", ossia::value{ossia::vec3f{0.f, 0.3f, 4.f}});
    set_by_name(*p.isf(splat), "fov", ossia::value{50.f});

    const uint64_t before = score::gfx::readbackStats().data.load();
    p.render(60);
    r.img = p.readback(sink);
    r.dataReadbacks = score::gfx::readbackStats().data.load() - before;
    if(r.error.empty())
      r.error = p.error();
  });
  return r;
}
}

TEST_CASE(
    "the library POP feedback chain reads nothing back to the CPU",
    "[gfx][csf][feedback][library][U42]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  const LibraryShot loop = run_library(api, true);
  if(loop.skipped)
    SKIP(loop.skip_reason);
  INFO(loop.error);
  REQUIRE(loop.error.empty());
  REQUIRE(loop.img.valid());
  const LibraryShot open = run_library(api, false);
  INFO(open.error);
  REQUIRE(open.error.empty());

  // With the Delayed cable the pool keeps every particle emitted so far (a
  // ramp of dots); without it only the current frame's particle shows.
  const int withLoop = lit_pixels(loop.img), without = lit_pixels(open.img);
  INFO("lit pixels: feedback " << withLoop << ", no feedback " << without);
  CHECK(without > 0);
  CHECK(withLoop > 8 * without);
  CHECK(loop.dataReadbacks == 0);
  CHECK(open.dataReadbacks == 0);
}
