// =============================================================================
// TRANSPARENCY header block for raw raster shaders (agent S1).
//
//   * the parser accepts the documented keys and values and rejects the rest;
//   * TARGET direct and TARGET internal give the same colour for two quads
//     drawn back to front with premultiplied over;
//   * the depth test against the consumer's depth hides a quad behind an opaque
//     one, although the transparent node was created first (transparent
//     sources draw last into a shared input);
//   * DEPTH_OUTPUT threshold / expected write the resolved depth into the
//     consumer's depth once per pixel; below THRESHOLD nothing is written;
//   * without the block, a raster keeps its depth write and its draw order.
//
// Fixture: s1tr-quads.vs draws red alpha 0.5 at window depth 0.3, then green
// alpha 0.6 at 0.6; s1tr-opaque-{mid,back}.vs an opaque blue triangle at 0.45
// or 0.1. s1tr-view.fs shows the stored rgb on the left half and the depth as
// grey on the right half.
// =============================================================================
#include "IsfTestCommon.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace score::test::gfx;
using namespace score::test::gfx::isf;

namespace
{
struct Shot
{
  bool skipped = false;
  std::string skip_reason, backend, error;
  ReadbackImage image;
};

// quads (and an optional opaque layer created after them) -> view -> sink
Shot render_quads(score::gfx::GraphicsApi be, const char* quadsFs, const char* opaqueVs)
{
  Shot r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int quads = p.addRaster(corpus("s1tr-quads.vs"), corpus(quadsFs));
    const int opaque
        = opaqueVs ? p.addRaster(corpus(opaqueVs), corpus("s1tr-opaque.fs")) : -2;
    const int view = p.addIsf(corpus("s1tr-view.fs"));
    const int sink = p.addSink({64, 64});
    if(quads < 0 || opaque == -1 || view < 0)
    {
      r.error = p.error().empty() ? "pipeline build failed" : p.error();
      return;
    }
    p.wire(p.imageOut(quads, 0), p.imageIn(view, 0));
    if(opaque >= 0)
      p.wire(p.imageOut(opaque, 0), p.imageIn(view, 0));
    p.wire(p.imageOut(view, 0), p.sinkInput(sink));
    if(!p.create(be))
    {
      r.skipped = p.skipped();
      r.skip_reason = p.skipReason();
      r.backend = p.backend();
      r.error = p.error();
      return;
    }
    r.backend = p.backend();
    p.render(3);
    r.image = p.readback(sink);
    if(r.error.empty())
      r.error = p.error();
    if(r.error.empty() && !r.image.valid())
      r.error = "empty readback";
  });
  return r;
}

#define S1_REQUIRE_LIVE(s)                                \
  if((s).skipped)                                         \
    SKIP((s).backend + ": " + (s).skip_reason);           \
  CAPTURE((s).backend);                                   \
  REQUIRE((s).error.empty());                             \
  REQUIRE((s).image.valid())

std::array<uint8_t, 4> colour(const Shot& s) { return s.image.at(16, 32); }
int depth(const Shot& s) { return s.image.at(48, 32)[0]; }

void check(const Shot& s, std::array<uint8_t, 4> rgb, int d)
{
  const auto c = colour(s);
  INFO("rgb = " << int(c[0]) << "," << int(c[1]) << "," << int(c[2]));
  INFO("depth = " << depth(s));
  CHECK(near(c, rgb, 3));
  CHECK(std::abs(depth(s) - d) <= 2);
}

::isf::descriptor parse(const std::string& transparency, const std::string& extra = {})
{
  const std::string fs = R"_(/*{
  "MODE": "RAW_RASTER_PIPELINE",
  "VERTEX_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "PIPELINE_STATE": { "VERTEX_COUNT": 3 })_"
                         + (transparency.empty() ? std::string{}
                                                 : ",\n  \"TRANSPARENCY\": " + transparency)
                         + extra + R"_(
}*/
void main() { isf_FragColor = vec4(1.0); }
)_";
  ::isf::parser p{"void main() { gl_Position = vec4(0.0); }", fs, 450,
                  ::isf::parser::ShaderType::RawRasterPipeline};
  return p.data();
}

bool rejects(const std::string& transparency, const std::string& extra = {})
{
  try
  {
    parse(transparency, extra);
  }
  catch(const std::exception&)
  {
    return true;
  }
  return false;
}
}

TEST_CASE("S1: TRANSPARENCY parses its keys and rejects bad values", "[gfx][isf][transparency][s1]")
{
  using namespace ::isf;
  {
    const auto d = parse({});
    CHECK(!d.transparency.enabled());
  }
  {
    const auto d = parse("{}");
    CHECK(d.transparency.target == transparency_target::internal);
    CHECK(d.transparency.format == "rgba16f");
    CHECK(d.transparency.depth_test);
    CHECK(d.transparency.depth_output == transparency_depth_output::none);
    CHECK(d.transparency.threshold == 0.5f);
    CHECK(resolve_composite(d) == composite_mode::over);
  }
  {
    const auto d = parse(R"_({ "TARGET": "DIRECT", "DEPTH_TEST": false, "COMPOSITE": "add" })_");
    CHECK(d.transparency.target == transparency_target::direct);
    CHECK(!d.transparency.depth_test);
    CHECK(resolve_composite(d) == composite_mode::add);
  }
  {
    const auto d = parse(
        R"_({ "target": "internal", "format": "rgba32f", "depth_output": "expected", "threshold": 0.25 })_",
        R"_(, "COMPOSITE": "screen")_");
    CHECK(d.transparency.format == "rgba32f");
    CHECK(d.transparency.depth_output == transparency_depth_output::expected);
    CHECK(d.transparency.threshold == 0.25f);
    CHECK(resolve_composite(d) == composite_mode::screen);
  }
  {
    const auto d = parse(R"_({ "DEPTH_OUTPUT": "threshold", "THRESHOLD": 1 })_");
    CHECK(d.transparency.depth_output == transparency_depth_output::threshold);
    CHECK(d.transparency.threshold == 1.f);
  }

  CHECK(rejects("true"));
  CHECK(rejects(R"_({ "TARGET": "offscreen" })_"));
  CHECK(rejects(R"_({ "TARGET": 1 })_"));
  CHECK(rejects(R"_({ "FORMAT": "r8" })_"));
  CHECK(rejects(R"_({ "DEPTH_TEST": "yes" })_"));
  CHECK(rejects(R"_({ "DEPTH_OUTPUT": "median" })_"));
  CHECK(rejects(R"_({ "THRESHOLD": 0 })_"));
  CHECK(rejects(R"_({ "THRESHOLD": 1.5 })_"));
  CHECK(rejects(R"_({ "COMPOSITE": "darken" })_"));
  CHECK(rejects(R"_({ "ORDER": "back_to_front" })_"));
  CHECK(rejects(R"_({ "TARGET": "direct", "DEPTH_OUTPUT": "expected" })_"));
  CHECK(rejects("{}", R"_(, "MULTIVIEW": 2)_"));
  CHECK(rejects("{}", R"_(, "EXECUTION_MODEL": { "TYPE": "MANUAL", "COUNT": "2" })_"));
  CHECK(rejects("{}", R"_(, "OUTPUTS": [ { "NAME": "a" }, { "NAME": "b" } ])_"));

  const std::string isfSrc = R"_(/*{ "ISFVSN": "2", "TRANSPARENCY": { "TARGET": "direct" } }*/
void main() { gl_FragColor = vec4(1.0); })_";
  CHECK_THROWS(::isf::parser{"", isfSrc, 450, ::isf::parser::ShaderType::ISF});
}

TEST_CASE(
    "S1: TRANSPARENCY generates the depth accumulation output only for DEPTH_OUTPUT",
    "[gfx][isf][transparency][s1]")
{
  auto frag = [](const std::string& t) {
    const std::string fs = R"_(/*{
  "MODE": "RAW_RASTER_PIPELINE",
  "VERTEX_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "TRANSPARENCY": )_" + t + R"_(
}*/
void main() { isf_FragColor = vec4(1.0); }
)_";
    ::isf::parser p{"void main() { gl_Position = vec4(0.0); }", fs, 450,
                    ::isf::parser::ShaderType::RawRasterPipeline};
    return p.fragment();
  };
  CHECK(frag("{}").find("isf_TransparencyDepth") == std::string::npos);
  const auto expected = frag(R"_({ "DEPTH_OUTPUT": "expected" })_");
  CHECK(expected.find("layout(location = 1) out vec4 isf_TransparencyDepth;")
        != std::string::npos);
  CHECK(expected.find("gl_FragCoord.z * isf_FragColor.a") != std::string::npos);
  const auto threshold = frag(R"_({ "DEPTH_OUTPUT": "threshold", "THRESHOLD": 0.7 })_");
  CHECK(threshold.find("isf_FragColor.a >= 0.700000") != std::string::npos);
}

TEST_CASE(
    "S1: TRANSPARENCY direct and internal composite two quads alike",
    "[gfx][raster][transparency][s1]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  // green 0.6 over red 0.5: rgb (0.2, 0.6, 0), alpha 0.8.
  const Shot direct = render_quads(backend, "s1tr-quads-direct.fs", nullptr);
  S1_REQUIRE_LIVE(direct);
  check(direct, {51, 153, 0, 255}, 0);

  const Shot internal = render_quads(backend, "s1tr-quads-internal.fs", nullptr);
  S1_REQUIRE_LIVE(internal);
  check(internal, {51, 153, 0, 255}, 0);
  CHECK(near(colour(direct), colour(internal), 2));
}

TEST_CASE(
    "S1: TRANSPARENCY tests against the consumer's depth and draws after opaque sources",
    "[gfx][raster][transparency][s1]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  // Opaque blue at 0.45 hides the red quad at 0.3; green 0.6 over blue:
  // (0, 0.6, 0.4). The consumer's depth stays the opaque one.
  for(const char* fs : {"s1tr-quads-direct.fs", "s1tr-quads-internal.fs"})
  {
    CAPTURE(fs);
    const Shot s = render_quads(backend, fs, "s1tr-opaque-mid.vs");
    S1_REQUIRE_LIVE(s);
    check(s, {0, 153, 102, 255}, 115);
  }
  // DEPTH_TEST false: the red quad shows too. Over blue at the pass order:
  // blue, then red 0.5, green 0.6: (0.2, 0.6, 0.2).
  const Shot s = render_quads(backend, "s1tr-quads-internal-notest.fs", "s1tr-opaque-mid.vs");
  S1_REQUIRE_LIVE(s);
  check(s, {51, 153, 51, 255}, 115);
}

TEST_CASE(
    "S1: TRANSPARENCY DEPTH_OUTPUT writes the resolved depth once per pixel",
    "[gfx][raster][transparency][s1]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  // Opaque blue at 0.1 behind both quads: colour (0.2, 0.6, 0.2) every time.
  // threshold 0.5: the nearest fragment with alpha >= 0.5 is green at 0.6.
  // expected: weights 0.6 (green) and 0.5 * 0.4 = 0.2 (red), accumulated 0.8:
  // (0.6 * 0.6 + 0.2 * 0.3) / 0.8 = 0.525.
  struct Case
  {
    const char* fs;
    int depth;
  };
  for(auto [fs, d] : {Case{"s1tr-quads-internal.fs", 26}, Case{"s1tr-quads-threshold.fs", 153},
                      Case{"s1tr-quads-threshold-high.fs", 26},
                      Case{"s1tr-quads-expected.fs", 134},
                      Case{"s1tr-quads-expected-high.fs", 26}})
  {
    CAPTURE(fs);
    const Shot s = render_quads(backend, fs, "s1tr-opaque-back.vs");
    S1_REQUIRE_LIVE(s);
    check(s, {51, 153, 51, 255}, d);
  }
  // Opaque in front of the red quad: only green contributes, 0.6 either way.
  for(const char* fs : {"s1tr-quads-threshold.fs", "s1tr-quads-expected.fs"})
  {
    CAPTURE(fs);
    const Shot s = render_quads(backend, fs, "s1tr-opaque-mid.vs");
    S1_REQUIRE_LIVE(s);
    check(s, {0, 153, 102, 255}, 153);
  }
}

TEST_CASE(
    "S1: without TRANSPARENCY a raster keeps its depth write and its draw order",
    "[gfx][raster][transparency][s1]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  const Shot alone = render_quads(backend, "s1tr-quads-none.fs", nullptr);
  S1_REQUIRE_LIVE(alone);
  check(alone, {51, 153, 0, 255}, 153);

  // Created first, so drawn first: its depth 0.6 then rejects the opaque blue
  // at 0.45.
  const Shot first = render_quads(backend, "s1tr-quads-none.fs", "s1tr-opaque-mid.vs");
  S1_REQUIRE_LIVE(first);
  check(first, {51, 153, 0, 255}, 153);
}
