// What a node fed straight back into itself sees through the self-feedback
// target of RenderList (N64 limits).
//
// * depth: a raw raster writing depth 0.5 samples its own input's depth; the
//   depth it wrote the frame before must come back, with the colour.
// * mips: an ISF sampling its input's 1x1 mip level; the level must follow
//   the base level, both through a plain cable and through the self-cable.
// * array input (GrabsFromSource): a layered ISF reading layer 0 of its own
//   output must read the previous frame, not the texture it is rendering.
//
//   DISPLAY=:0 SCORE_GPU_VALIDATION=0 ctest -R gfx_selffb_limits_d2
#include <score_test/Gfx.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <functional>

using namespace score::test::gfx;

namespace
{
QString corpus(const char* file)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR "/") + QString::fromUtf8(file);
}

int raw_image_input_index(const score::gfx::Node& n, int k)
{
  int seen = 0;
  for(std::size_t i = 0; i < n.input.size(); ++i)
    if(n.input[i]->type == score::gfx::Types::Image)
      if(seen++ == k)
        return int(i);
  return -1;
}

void request_mips(score::gfx::Node& n)
{
  ossia::render_target_spec spec;
  spec.mipmap_mode = ossia::texture_filter::LINEAR;
  setRenderTargetSpec(n, raw_image_input_index(n, 0), spec);
}

struct Frames
{
  bool skipped = false;
  std::string skip_reason, backend, error;
  std::array<int, 4> early{-1, -1, -1, -1}, late{-1, -1, -1, -1};
};

std::array<int, 4> center(const ReadbackImage& img)
{
  if(!img.valid())
    return {-1, -1, -1, -1};
  const auto c = img.center();
  return {c[0], c[1], c[2], c[3]};
}

// build() adds the nodes and cables and returns the sink index, or -1.
Frames render_frames(
    score::gfx::GraphicsApi api, const std::function<int(GfxPipeline&)>& build)
{
  Frames r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int sink = build(p);
    if(sink < 0)
    {
      r.error = p.error().empty() ? "build failed" : p.error();
      return;
    }
    if(!p.create(api))
    {
      r.skipped = p.skipped();
      r.skip_reason = p.skipReason();
      r.backend = p.backend();
      r.error = r.skipped ? std::string{} : p.error();
      return;
    }
    r.backend = p.backend();
    p.render(2);
    r.early = center(p.readback(sink));
    p.render(4);
    r.late = center(p.readback(sink));
    if(r.error.empty())
      r.error = p.error();
  });
  return r;
}
}

TEST_CASE(
    "a self-fed raw raster samples the depth it wrote the frame before",
    "[gfx][feedback][selffb-d2][depth]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const auto cable = GENERATE(
      Process::CableType::DelayedGlutton, Process::CableType::ImmediateGlutton);
  CAPTURE(backend_name(api), int(cable));

  const Frames f = render_frames(api, [&](GfxPipeline& p) {
    const int a
        = p.addRaster(corpus("d2selffb-depth.vs"), corpus("d2selffb-depth.fs"));
    if(a < 0)
      return -1;
    const int sink = p.addSink({32, 32});
    p.wire(p.imageOut(a, 0), p.imageIn(a, 0), cable);
    p.wire(p.imageOut(a, 0), p.sinkInput(sink));
    return sink;
  });
  if(f.skipped)
    SKIP(f.skip_reason);
  REQUIRE(f.error.empty());
  CAPTURE(f.backend, f.early, f.late);

  // Depth 0.5 is 128 in 8 bits.
  CHECK(f.early[0] >= 120);
  CHECK(f.early[0] <= 136);
  CHECK(f.late[0] >= 120);
  CHECK(f.late[0] <= 136);
  // Green climbs one step of 8 per frame.
  CHECK(f.early[1] >= 16);
  CHECK(f.late[1] >= f.early[1] + 24);
}

TEST_CASE(
    "a mipmapped input's small levels follow its base level",
    "[gfx][feedback][selffb-d2][mips]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const Frames f = render_frames(api, [&](GfxPipeline& p) {
    const int a = p.addIsf(corpus("fixm-step-add.fs"));
    const int b = p.addIsf(corpus("d2selffb-mip.fs"));
    if(a < 0 || b < 0)
      return -1;
    request_mips(*p.isf(b));
    const int sink = p.addSink({32, 32});
    p.wire(p.imageOut(a, 0), p.imageIn(b, 0));
    p.wire(p.imageOut(b, 0), p.sinkInput(sink));
    return sink;
  });
  if(f.skipped)
    SKIP(f.skip_reason);
  REQUIRE(f.error.empty());
  CAPTURE(f.backend, f.early, f.late);

  // A draws 8 over its empty input; B adds 8 to A's 1x1 level.
  CHECK(f.early[0] >= 14);
  CHECK(f.early[0] <= 18);
  CHECK(f.late[0] >= 14);
  CHECK(f.late[0] <= 18);
}

TEST_CASE(
    "a self-fed mipmapped input's small levels follow the feedback",
    "[gfx][feedback][selffb-d2][mips]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const auto cable = GENERATE(
      Process::CableType::DelayedGlutton, Process::CableType::ImmediateGlutton);
  CAPTURE(backend_name(api), int(cable));

  const Frames f = render_frames(api, [&](GfxPipeline& p) {
    const int a = p.addIsf(corpus("d2selffb-mip.fs"));
    if(a < 0)
      return -1;
    request_mips(*p.isf(a));
    const int sink = p.addSink({32, 32});
    p.wire(p.imageOut(a, 0), p.imageIn(a, 0), cable);
    p.wire(p.imageOut(a, 0), p.sinkInput(sink));
    return sink;
  });
  if(f.skipped)
    SKIP(f.skip_reason);
  REQUIRE(f.error.empty());
  CAPTURE(f.backend, f.early, f.late);

  CHECK(f.early[0] >= 16);
  CHECK(f.late[0] >= f.early[0] + 24);
}

TEST_CASE(
    "a self-fed array input reads the previous frame",
    "[gfx][feedback][selffb-d2][grabs]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const auto cable = GENERATE(
      Process::CableType::DelayedGlutton, Process::CableType::ImmediateGlutton);
  CAPTURE(backend_name(api), int(cable));

  const Frames f = render_frames(api, [&](GfxPipeline& p) {
    const int a = p.addIsf(corpus("d2selffb-array.fs"));
    const int r = p.addIsf(corpus("d2selffb-array-reader.fs"));
    if(a < 0 || r < 0)
      return -1;
    const int sink = p.addSink({32, 32});
    p.wire(p.imageOut(a, 0), p.imageIn(a, 0), cable);
    p.wire(p.imageOut(a, 0), p.imageIn(r, 0));
    p.wire(p.imageOut(r, 0), p.sinkInput(sink));
    return sink;
  });
  if(f.skipped)
    SKIP(f.skip_reason);
  REQUIRE(f.error.empty());
  CAPTURE(f.backend, f.early, f.late);

  // One step per frame: at least two after two frames, four more after four.
  CHECK(f.early[0] >= 16);
  CHECK(f.late[0] >= f.early[0] + 24);
}

// The same self-cables added and removed while rendering, through the
// incremental path the app uses.
TEST_CASE(
    "a self-cable on a depth or array input added live carries its state",
    "[gfx][feedback][selffb-d2][live]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const bool depth = GENERATE(true, false);
  CAPTURE(backend_name(api), depth);

  bool skipped = false;
  std::string skip_reason, backend, error;
  std::array<int, 4> before{}, early{}, late{}, after{};
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int a
        = depth ? p.addRaster(corpus("d2selffb-depth.vs"), corpus("d2selffb-depth.fs"))
                : p.addIsf(corpus("d2selffb-array.fs"));
    const int r = depth ? a : p.addIsf(corpus("d2selffb-array-reader.fs"));
    const int sink = p.addSink({32, 32});
    if(a < 0 || r < 0)
    {
      error = p.error().empty() ? "build failed" : p.error();
      return;
    }
    if(!depth)
      p.wire(p.imageOut(a, 0), p.imageIn(r, 0));
    p.wire(p.imageOut(r, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      skipped = p.skipped();
      skip_reason = p.skipReason();
      error = skipped ? std::string{} : p.error();
      return;
    }
    backend = p.backend();

    p.render(2);
    before = center(p.readback(sink));
    p.addEdgeIncremental(
        p.imageOut(a, 0), p.imageIn(a, 0), Process::CableType::DelayedGlutton);
    p.render(2);
    early = center(p.readback(sink));
    p.render(4);
    late = center(p.readback(sink));
    p.removeEdgeIncremental(p.imageOut(a, 0), p.imageIn(a, 0));
    p.render(2);
    after = center(p.readback(sink));
    if(error.empty())
      error = p.error();
  });
  if(skipped)
    SKIP(skip_reason);
  REQUIRE(error.empty());
  CAPTURE(backend, before, early, late, after);

  const int level = depth ? 1 : 0;
  CHECK(before[level] >= 6);
  CHECK(before[level] <= 10);
  CHECK(early[level] >= before[level] + 8);
  CHECK(late[level] >= early[level] + 24);
  CHECK(after[level] >= 6);
  CHECK(after[level] <= 10);
  if(depth)
  {
    CHECK(before[0] <= 4);
    CHECK(early[0] >= 120);
    CHECK(early[0] <= 136);
    CHECK(late[0] >= 120);
    CHECK(late[0] <= 136);
    CHECK(after[0] <= 4);
  }
}
