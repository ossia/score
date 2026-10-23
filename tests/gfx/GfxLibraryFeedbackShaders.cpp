// Library compute shaders that keep state through a delayed self-cable, run
// the way a score wires them. Skips without SCORE_TEST_LIBRARY_ROOT.
//
//   * shaderlib/generators/CircularHistory.cs: a ring buffer of 100 points,
//     its write index in a `state` auxiliary. The index used to be advanced in
//     the pass that also reads it, by thread 0 of a 64-wide dispatch over 100
//     points, racing with the second workgroup; it is advanced in a second,
//     single-thread pass. Fed 48 points a frame (csf-storage-items.cs, 24
//     vec4 = 48 vec2), the index goes 0, 48, 96, 44, 92, ...
//   * shaderlib/simulations/NBodyGravity.cs: every body reads every other
//     body's position, so position is ACCESS gather and the pass reads last
//     frame's positions apart from the ones it writes. The bodies must stay
//     finite and keep moving.
#include "GfxUserLibrary.hpp"

#include <score_test/Gfx.hpp>
#include <score_test/GfxBufferSink.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <cmath>
#include <cstring>

using namespace score::test::gfx;

namespace
{
const BufferSinkNode::NamedReadback*
named(const std::deque<BufferSinkNode::NamedReadback>& d, std::string_view name)
{
  for(const auto& r : d)
    if(r.name == name)
      return &r;
  return nullptr;
}
}

TEST_CASE(
    "CircularHistory advances its write index by the points it receives",
    "[gfx][csf][library][feedback]")
{
  const QString shader = library::find("shaderlib/generators/CircularHistory.cs");
  if(shader.isEmpty())
    SKIP(library::skip_reason("shaderlib/generators/CircularHistory.cs"));
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  std::string skip;
  std::vector<uint32_t> index, initialized;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    if(const char* why = compute_shader_skip_reason(api))
    {
      skip = std::string{backend_name(api)} + ": " + why;
      return;
    }
    library::RootGuard guard{ctx};
    GfxPipeline p;
    const int items = p.addCsf(QStringLiteral(GFX_TEST_CORPUS_DIR "/csf-storage-items.cs"));
    const int hist = p.addCsf(shader);
    REQUIRE(items >= 0);
    REQUIRE(hist >= 0);
    auto sink = attach_buffer_sink(p);
    REQUIRE(sink.node);
    p.wire(p.bufferOut(items, 0), p.bufferIn(hist, 0));
    p.wire(p.geometryOut(hist, 0), sink.node->sinkInput());
    p.wireFeedback(p.geometryOut(hist, 0), p.geometryIn(hist, 0));
    sink.node->setSize({64, 64});
    REQUIRE(p.create(api));
    REQUIRE(sink.node->renderState());
    if(!sink.node->renderState()->rhi->isFeatureSupported(QRhi::ReadBackNonUniformBuffer))
    {
      skip = std::string{backend_name(api)} + ": no non-uniform buffer readback";
      return;
    }
    for(int f = 0; f < 6; f++)
    {
      render_with_buffer_sink(p, *sink.node, 1);
      REQUIRE(p.error().empty());
      const auto* st = named(sink.node->harvest->auxiliaries, "state");
      REQUIRE(st);
      REQUIRE(st->rb.data.size() >= 8);
      uint32_t w[2];
      std::memcpy(w, st->rb.data.constData(), 8);
      index.push_back(w[0]);
      initialized.push_back(w[1]);
    }
  });
  if(!skip.empty())
    SKIP(skip);
  CAPTURE(index, initialized);
  REQUIRE(index.size() == 6);
  for(auto i : initialized)
    CHECK(i == 1u);
  CHECK(index == std::vector<uint32_t>{0, 48, 96, 44, 92, 40});
}

TEST_CASE(
    "NBodyGravity bodies stay finite and keep moving", "[gfx][csf][library][feedback]")
{
  const QString shader = library::find("shaderlib/simulations/NBodyGravity.cs");
  if(shader.isEmpty())
    SKIP(library::skip_reason("shaderlib/simulations/NBodyGravity.cs"));
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  std::string skip;
  std::vector<std::vector<float>> frames;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    if(const char* why = compute_shader_skip_reason(api))
    {
      skip = std::string{backend_name(api)} + ": " + why;
      return;
    }
    library::RootGuard guard{ctx};
    GfxPipeline p;
    const int nbody = p.addCsf(shader);
    REQUIRE(nbody >= 0);
    auto sink = attach_buffer_sink(p);
    REQUIRE(sink.node);
    p.wire(p.geometryOut(nbody, 0), sink.node->sinkInput());
    p.wireFeedback(p.geometryOut(nbody, 0), p.geometryIn(nbody, 0));
    sink.node->setSize({64, 64});
    REQUIRE(p.create(api));
    REQUIRE(sink.node->renderState());
    if(!sink.node->renderState()->rhi->isFeatureSupported(QRhi::ReadBackNonUniformBuffer))
    {
      skip = std::string{backend_name(api)} + ": no non-uniform buffer readback";
      return;
    }
    for(int f = 0; f < 6; f++)
    {
      render_with_buffer_sink(p, *sink.node, 1);
      REQUIRE(p.error().empty());
      const auto* pos = named(sink.node->harvest->attributes, "position");
      REQUIRE(pos);
      frames.push_back(as_floats(pos->rb.data));
    }
  });
  if(!skip.empty())
    SKIP(skip);
  REQUIRE(frames.size() == 6);
  REQUIRE(frames.back().size() >= 4);
  int nonfinite = 0;
  for(float v : frames.back())
    nonfinite += !std::isfinite(v);
  CHECK(nonfinite == 0);
  // The bodies move between the third and the last frame.
  double moved = 0;
  for(std::size_t i = 0; i < frames.back().size() && i < frames[2].size(); i++)
    moved += std::abs(frames.back()[i] - frames[2][i]);
  CHECK(moved > 0.);
}
