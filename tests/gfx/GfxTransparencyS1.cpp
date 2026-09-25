// =============================================================================
// LAYER and QUEUE header keys of raw raster shaders.
//
//   * the parser accepts the documented keys and values, synthesises the
//     fragment outputs and the default target, and rejects the rest;
//   * the draw stage declares the targets as outputs, the resolve stage (the
//     same source with ISF_RESOLVE_PASS) declares them as samplers, and both
//     get the depth convention of the shader's DEPTH_COMPARE; every resolve
//     fixture bakes for every backend's language;
//   * the default resolve composites the first target; the depth test against
//     the consumer's depth hides a quad behind an opaque one although the
//     layer node was created first (transparent sources draw last);
//   * two targets with different blends and clears reach the resolve intact;
//   * a resolve writing gl_FragDepth is seen by a downstream depth reader, and
//     a resolve can read the consumer's depth;
//   * weighted blended OIT written in shader code gives the same colour for
//     either draw order, where the sorted over does not;
//   * QUEUE transparent orders a raster without LAYER; without either, a
//     raster keeps its depth write and its draw order.
//
// Fixture: s1tr-*.vs draw red alpha 0.5 at window depth 0.3 and green alpha
// 0.6 at 0.6 (red first, or green first for *-gr); s1tr-opaque-{mid,back}.vs
// an opaque blue triangle at 0.45 or 0.1. s1tr-view.fs shows the stored rgb on
// the left half and the depth as grey on the right half.
// =============================================================================
#include "IsfTestCommon.hpp"

#include <Gfx/Graph/ShaderCache.hpp>

#include <QFile>

#include <isf.hpp>

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

// layer (and an optional opaque layer created after it) -> view -> sink
Shot render_quads(score::gfx::GraphicsApi be, const char* quadsFs, const char* opaqueVs)
{
  Shot r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const QString quadsVs = QString(quadsFs).replace(".fs", ".vs");
    const int quads = p.addRaster(corpus(quadsVs.toUtf8().constData()), corpus(quadsFs));
    const QString opaqueFs = QString(opaqueVs ? opaqueVs : "").replace(".vs", ".fs");
    const int opaque
        = opaqueVs ? p.addRaster(corpus(opaqueVs), corpus(opaqueFs.toUtf8().constData()))
                   : -2;
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

std::string header(const std::string& keys, const std::string& outputs)
{
  return R"_(/*{
  "MODE": "RAW_RASTER_PIPELINE",
  "VERTEX_INPUTS": [],)_"
         + outputs + R"_(
  "PIPELINE_STATE": { "VERTEX_COUNT": 3 })_"
         + keys + R"_(
}*/
)_";
}

const std::string oneOutput
    = R"_( "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],)_";

::isf::parser make(const std::string& keys, const std::string& outputs = oneOutput,
                   const std::string& body = "void main() { isf_FragColor = vec4(1.0); }\n")
{
  return ::isf::parser{"void main() { gl_Position = vec4(0.0); }", header(keys, outputs) + body,
                       450, ::isf::parser::ShaderType::RawRasterPipeline};
}

::isf::descriptor parse(const std::string& keys, const std::string& outputs = oneOutput)
{
  return make(keys, outputs).data();
}

bool rejects(const std::string& keys, const std::string& outputs = oneOutput,
             const std::string& body = "void main() { isf_FragColor = vec4(1.0); }\n")
{
  try
  {
    make(keys, outputs, body);
  }
  catch(const std::exception&)
  {
    return true;
  }
  return false;
}

const std::string twoTargets = R"_(,
  "LAYER": {
    "TARGETS": [
      { "NAME": "accum", "FORMAT": "RGBA16F", "BLEND": { "SRC": "one", "DST": "one" } },
      { "NAME": "reveal", "FORMAT": "r8", "CLEAR": [1], "COMPOSITE": "multiply" }
    ],
    "DEPTH_TEST": false,
    "RESOLVE": { "OUTPUT": "result", "COMPOSITE": "add", "DEPTH_INPUT": "scene" }
  })_";
const std::string resolveBody = R"_(#if defined(ISF_RESOLVE_PASS)
void main() { result = LAYER_TEXEL(accum) * LAYER_TEXEL(reveal).r * LAYER_TEXEL(scene).r; }
#else
void main() { accum = vec4(1.0); reveal = vec4(0.5); }
#endif
)_";
}

TEST_CASE("S1: LAYER and QUEUE parse their keys and reject bad values", "[gfx][isf][layer][s1]")
{
  using namespace ::isf;
  {
    const auto d = parse({});
    CHECK(!d.layer.enabled());
    CHECK(d.queue == render_queue::unspecified);
    CHECK(!draws_transparent(d));
  }
  {
    const auto d = parse(R"_(, "LAYER": {})_");
    REQUIRE(d.layer.enabled());
    REQUIRE(d.layer.targets.size() == 1);
    const auto& t = d.layer.targets[0];
    CHECK(t.name == "isf_FragColor");
    CHECK(t.format == "rgba16f");
    CHECK(t.clear == std::array<float, 4>{0.f, 0.f, 0.f, 0.f});
    CHECK(!t.blend);
    CHECK(t.composite == composite_mode::unspecified);
    CHECK(d.layer.depth_test);
    CHECK(!d.layer.resolve.declared);
    CHECK(d.layer.resolve.fragment.empty());
    CHECK(d.outputs.empty());
    CHECK(draws_transparent(d));
  }
  {
    const auto d = parse(R"_(, "LAYER": {}, "QUEUE": "OPAQUE")_");
    CHECK(!draws_transparent(d));
  }
  {
    const auto d = parse(R"_(, "QUEUE": "transparent")_");
    CHECK(!d.layer.enabled());
    CHECK(draws_transparent(d));
  }
  {
    const auto p = make(twoTargets, {}, resolveBody);
    const auto& d = p.data();
    REQUIRE(d.layer.targets.size() == 2);
    CHECK(d.outputs.empty());
    REQUIRE(d.fragment_outputs.size() == 2);
    CHECK(d.fragment_outputs[0].name == "accum");
    CHECK(d.fragment_outputs[1].name == "reveal");
    CHECK(d.fragment_outputs[1].location == 1);
    CHECK(d.layer.targets[0].format == "rgba16f");
    REQUIRE(d.layer.targets[0].blend);
    CHECK(d.layer.targets[0].blend->enable);
    CHECK(d.layer.targets[0].blend->src_color == "one");
    CHECK(d.layer.targets[0].blend->dst_alpha == "one");
    CHECK(d.layer.targets[1].format == "r8");
    CHECK(d.layer.targets[1].clear == std::array<float, 4>{1.f, 0.f, 0.f, 0.f});
    CHECK(d.layer.targets[1].composite == composite_mode::multiply);
    CHECK(!d.layer.depth_test);
    CHECK(d.layer.resolve.declared);
    CHECK(d.layer.resolve.output == "result");
    CHECK(d.layer.resolve.composite == composite_mode::add);
    CHECK(d.layer.resolve.depth_input == "scene");
    CHECK(!d.layer.resolve.depth_write);
  }
  {
    const auto p = make(
        R"_(, "LAYER": { "TARGETS": [ { "NAME": "a" }, { "NAME": "b", "BLEND": false } ], "RESOLVE": { "BLEND": { "SRC_COLOR": "one" }, "DEPTH_WRITE": true } })_",
        R"_( "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "a" }, { "TYPE": "vec4", "NAME": "b" } ],)_",
        "#if defined(ISF_RESOLVE_PASS)\nvoid main() { isf_FragColor = LAYER_TEXEL(a); }\n"
        "#else\nvoid main() { a = vec4(1.0); b = vec4(1.0); }\n#endif\n");
    const auto& d = p.data();
    REQUIRE(d.layer.targets.size() == 2);
    REQUIRE(d.layer.targets[1].blend);
    CHECK(!d.layer.targets[1].blend->enable);
    REQUIRE(d.layer.resolve.blend);
    CHECK(d.layer.resolve.blend->src_color == "one");
    CHECK(d.layer.resolve.depth_write);
  }

  const std::string ab
      = R"_( "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "a" }, { "TYPE": "vec4", "NAME": "b" } ],)_";
  CHECK(rejects(R"_(, "LAYER": true)_"));
  CHECK(rejects(R"_(, "LAYER": { "TARGETS": [] })_"));
  CHECK(rejects(R"_(, "LAYER": { "TARGETS": {} })_"));
  CHECK(rejects(R"_(, "LAYER": { "TARGETS": [ {} ] })_"));
  CHECK(rejects(R"_(, "LAYER": { "TARGETS": [ { "NAME": "1a" } ] })_"));
  CHECK(rejects(R"_(, "LAYER": { "TARGETS": [ { "NAME": "isf_FragColor", "FORMAT": "r32ui" } ] })_"));
  CHECK(rejects(R"_(, "LAYER": { "TARGETS": [ { "NAME": "isf_FragColor", "CLEAR": [0, 0, 0, 0, 0] } ] })_"));
  CHECK(rejects(R"_(, "LAYER": { "TARGETS": [ { "NAME": "isf_FragColor", "CLEAR": "black" } ] })_"));
  CHECK(rejects(R"_(, "LAYER": { "TARGETS": [ { "NAME": "isf_FragColor", "BLEND": "add" } ] })_"));
  CHECK(rejects(R"_(, "LAYER": { "TARGETS": [ { "NAME": "isf_FragColor", "BLEND": true, "COMPOSITE": "add" } ] })_"));
  CHECK(rejects(R"_(, "LAYER": { "TARGETS": [ { "NAME": "isf_FragColor", "COMPOSITE": "darken" } ] })_"));
  CHECK(rejects(R"_(, "LAYER": { "TARGETS": [ { "NAME": "isf_FragColor", "ORDER": 1 } ] })_"));
  CHECK(rejects(R"_(, "LAYER": { "TARGETS": [ { "NAME": "other" } ] })_"));
  CHECK(rejects(R"_(, "LAYER": { "TARGETS": [ { "NAME": "b" }, { "NAME": "a" } ] })_", ab));
  CHECK(rejects(R"_(, "LAYER": { "TARGETS": [ { "NAME": "a" }, { "NAME": "a" } ] })_", ab));
  CHECK(rejects(R"_(, "LAYER": { "TARGETS": [ { "NAME": "a" } ] })_", ab));
  CHECK(rejects(R"_(, "LAYER": { "DEPTH_TEST": "yes" })_"));
  CHECK(rejects(R"_(, "LAYER": { "DEPTH_OUTPUT": "expected" })_"));
  CHECK(rejects(R"_(, "LAYER": { "THRESHOLD": 0.5 })_"));
  CHECK(rejects(R"_(, "LAYER": { "RESOLVE": true })_"));
  CHECK(rejects(R"_(, "LAYER": { "RESOLVE": {} })_"));
  CHECK(rejects(R"_(, "LAYER": { "RESOLVE": { "OUTPUT": "isf_FragColor" } })_", oneOutput,
                "#ifdef ISF_RESOLVE_PASS\n#endif\nvoid main() {}\n"));
  CHECK(rejects(R"_(, "LAYER": { "RESOLVE": { "DEPTH_WRITE": true, "DEPTH_INPUT": "d" } })_",
                oneOutput, "#ifdef ISF_RESOLVE_PASS\n#endif\nvoid main() {}\n"));
  CHECK(rejects(R"_(, "LAYER": { "RESOLVE": { "DEPTH_INPUT": "isf_FragColor" } })_",
                oneOutput, "#ifdef ISF_RESOLVE_PASS\n#endif\nvoid main() {}\n"));
  CHECK(rejects(R"_(, "LAYER": { "RESOLVE": { "COMPOSITE": "add", "BLEND": true } })_",
                oneOutput, "#ifdef ISF_RESOLVE_PASS\n#endif\nvoid main() {}\n"));
  CHECK(rejects(R"_(, "LAYER": { "RESOLVE": { "DEPTH_WRITE": 1 } })_", oneOutput,
                "#ifdef ISF_RESOLVE_PASS\n#endif\nvoid main() {}\n"));
  CHECK(rejects(R"_(, "LAYER": { "TARGET": "internal" })_"));
  CHECK(rejects(R"_(, "LAYER": {}, "MULTIVIEW": 2)_"));
  CHECK(rejects(R"_(, "LAYER": {}, "EXECUTION_MODEL": { "TYPE": "MANUAL", "COUNT": "2" })_"));
  CHECK(rejects(R"_(, "LAYER": {}, "OUTPUTS": [ { "NAME": "a" }, { "NAME": "b" } ])_", ab));
  CHECK(rejects(R"_(, "QUEUE": "overlay")_"));

  const std::string isfSrc = R"_(/*{ "ISFVSN": "2", "LAYER": {} }*/
void main() { gl_FragColor = vec4(1.0); })_";
  CHECK_THROWS(::isf::parser{"", isfSrc, 450, ::isf::parser::ShaderType::ISF});
}

TEST_CASE(
    "S1: LAYER compiles the draw with its targets as outputs and the resolve with "
    "them as samplers",
    "[gfx][isf][layer][s1]")
{
  const auto p = make(twoTargets, {}, resolveBody);
  const std::string& draw = p.fragment();
  const std::string& resolve = p.data().layer.resolve.fragment;
  CHECK(draw.find("layout(location = 0) out vec4 accum;") != std::string::npos);
  CHECK(draw.find("layout(location = 1) out vec4 reveal;") != std::string::npos);
  CHECK(draw.find("#define ISF_RESOLVE_PASS") == std::string::npos);
  CHECK(draw.find("isf_TransparencyDepth") == std::string::npos);
  CHECK(draw.find("#define ISF_DEPTH_NEARER_IS_GREATER 1") != std::string::npos);

  CHECK(resolve.find("#define ISF_RESOLVE_PASS 1") != std::string::npos);
  CHECK(resolve.find("layout(location = 0) out vec4 result;") != std::string::npos);
  CHECK(resolve.find("layout(binding = 3) uniform sampler2D accum;") != std::string::npos);
  CHECK(resolve.find("layout(binding = 4) uniform sampler2D reveal;") != std::string::npos);
  CHECK(resolve.find("layout(binding = 5) uniform sampler2D scene;") != std::string::npos);
  CHECK(resolve.find("out vec4 accum") == std::string::npos);
  CHECK(resolve.find("uniform material_t") != std::string::npos);
  CHECK(resolve.find("#define ISF_DEPTH_FAR 0.0") != std::string::npos);

  const auto less = make(
      twoTargets + R"_(, "PIPELINE_STATE": { "DEPTH_COMPARE": "less" })_", {}, resolveBody);
  CHECK(less.fragment().find("#define ISF_DEPTH_NEARER_IS_GREATER 0") != std::string::npos);
  CHECK(less.data().layer.resolve.fragment.find("#define ISF_DEPTH_FAR 1.0")
        != std::string::npos);
  CHECK(!::isf::depth_nearer_is_greater(less.data()));

  // A straight output multiplied onto the consumer is premultiplied by the
  // engine, for a target and for the resolve output alike.
  const auto straight = make(
      twoTargets + R"_(, "ALPHA": "straight", "COMPOSITE": "multiply")_", {}, resolveBody);
  CHECK(straight.fragment().find("reveal.rgb *= reveal.a;") != std::string::npos);
  CHECK(straight.fragment().find("accum.rgb *= accum.a;") == std::string::npos);
  CHECK(straight.data().layer.resolve.fragment.find("result.rgb *= result.a;")
        == std::string::npos);
  const auto straightScreen = make(
      R"_(, "ALPHA": "straight", "LAYER": { "RESOLVE": { "COMPOSITE": "screen" } })_",
      R"_( "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "c" } ],)_",
      "#ifdef ISF_RESOLVE_PASS\nvoid main() { isf_FragColor = LAYER_TEXEL(c); }\n"
      "#else\nvoid main() { c = vec4(1.0); }\n#endif\n");
  CHECK(straightScreen.data().layer.resolve.fragment.find("isf_FragColor.rgb *= isf_FragColor.a;")
        != std::string::npos);
}

TEST_CASE("S1: every LAYER resolve fixture bakes for every backend", "[gfx][isf][layer][s1]")
{
  const auto api = GENERATE(
      score::gfx::OpenGL, score::gfx::Vulkan, score::gfx::D3D11, score::gfx::D3D12,
      score::gfx::Metal);
  CAPTURE(backend_name(api));
  const QShaderVersion version = api == score::gfx::OpenGL   ? QShaderVersion(450)
                                 : api == score::gfx::Vulkan ? QShaderVersion(100)
                                 : api == score::gfx::Metal  ? QShaderVersion(12)
                                                             : QShaderVersion(50);
  auto read = [](const char* f) {
    QFile file(corpus(f));
    file.open(QIODevice::ReadOnly);
    return file.readAll().toStdString();
  };
  for(const char* fs :
      {"s1tr-quads-depth.fs", "s1tr-quads-mrt.fs", "s1tr-quads-depthin.fs", "s1tr-oit-rg.fs"})
  {
    CAPTURE(fs);
    std::string vs = fs;
    vs.replace(vs.size() - 3, 3, ".vs");
    ::isf::parser p{read(vs.c_str()), read(fs), 450,
                    ::isf::parser::ShaderType::RawRasterPipeline};
    const auto& src = p.data().layer.resolve.fragment;
    REQUIRE(!src.empty());
    for(const auto& [stage, text] :
        {std::pair{QShader::FragmentStage, src},
         std::pair{QShader::FragmentStage, p.fragment()}})
    {
      const auto& [shader, error] = score::gfx::ShaderCache::get(
          api, version, QByteArray::fromStdString(text), stage);
      INFO(error.toStdString());
      CHECK(error.isEmpty());
      CHECK(shader.isValid());
    }
  }
}

TEST_CASE(
    "S1: the default LAYER resolve composites the first target, tested against the "
    "consumer's depth, after the opaque sources",
    "[gfx][raster][layer][s1]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  // green 0.6 over red 0.5: rgb (0.2, 0.6, 0), alpha 0.8; no depth written.
  const Shot alone = render_quads(backend, "s1tr-quads-layer.fs", nullptr);
  S1_REQUIRE_LIVE(alone);
  check(alone, {51, 153, 0, 255}, 0);

  // Opaque blue at 0.45, created after the layer node, hides the red quad at
  // 0.3; green 0.6 over blue: (0, 0.6, 0.4). The consumer's depth stays the
  // opaque one.
  const Shot hidden = render_quads(backend, "s1tr-quads-layer.fs", "s1tr-opaque-mid.vs");
  S1_REQUIRE_LIVE(hidden);
  check(hidden, {0, 153, 102, 255}, 115);

  // DEPTH_TEST false: the red quad shows too, (0.2, 0.6, 0.2) over blue.
  const Shot notest
      = render_quads(backend, "s1tr-quads-layer-notest.fs", "s1tr-opaque-mid.vs");
  S1_REQUIRE_LIVE(notest);
  check(notest, {51, 153, 51, 255}, 115);
}

TEST_CASE(
    "S1: two LAYER targets with their own blends and clears reach the resolve",
    "[gfx][raster][layer][s1]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  // sum (additive from 0): (0.5, 0.6, 0, 1.1); peak (max from 0.25):
  // (0.5, 0.6, 0.25, 0.6). The resolve writes
  // (sum.r + sum.g - sum.a / 2, peak.g, peak.b) = (0.55, 0.6, 0.25).
  const Shot s = render_quads(backend, "s1tr-quads-mrt.fs", nullptr);
  S1_REQUIRE_LIVE(s);
  check(s, {140, 153, 64, 255}, 0);
}

TEST_CASE(
    "S1: a LAYER resolve writes the consumer's depth, and reads it",
    "[gfx][raster][layer][s1]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  // Opaque blue at 0.1 behind both quads: colour (0.2, 0.6, 0.2) every time.
  // The weights of the expected depth: 0.6 (green) and 0.5 * 0.4 = 0.2 (red),
  // accumulated 0.8: (0.6 * 0.6 + 0.2 * 0.3) / 0.8 = 0.525, written where the
  // coverage reaches the threshold input (0.5, not 0.9).
  struct Case
  {
    const char* fs;
    int depth;
  };
  for(auto [fs, d] : {Case{"s1tr-quads-layer.fs", 26}, Case{"s1tr-quads-depth.fs", 134},
                      Case{"s1tr-quads-depth-high.fs", 26}})
  {
    CAPTURE(fs);
    const Shot s = render_quads(backend, fs, "s1tr-opaque-back.vs");
    S1_REQUIRE_LIVE(s);
    check(s, {51, 153, 51, 255}, d);
  }
  // Opaque in front of the red quad: only green contributes, 0.6.
  {
    const Shot s = render_quads(backend, "s1tr-quads-depth.fs", "s1tr-opaque-mid.vs");
    S1_REQUIRE_LIVE(s);
    check(s, {0, 153, 102, 255}, 153);
  }
  // DEPTH_INPUT: the resolve replaces the consumer with (its depth 0.45, the
  // layer's green 0.6, 0); without an opaque source the depth is the clear.
  {
    const Shot s = render_quads(backend, "s1tr-quads-depthin.fs", "s1tr-opaque-mid.vs");
    S1_REQUIRE_LIVE(s);
    check(s, {115, 153, 0, 255}, 115);
  }
  {
    const Shot s = render_quads(backend, "s1tr-quads-depthin.fs", nullptr);
    S1_REQUIRE_LIVE(s);
    check(s, {0, 153, 0, 255}, 0);
  }
}

TEST_CASE(
    "S1: weighted blended OIT in shader code is independent of the draw order",
    "[gfx][raster][layer][oit][s1]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  // Weights 1 + 9 * nearness: red 3.7, green 6.4. accum = (1.85, 3.84, 0,
  // 5.69), revealage 0.5 * 0.4 = 0.2: (0.3251, 0.6749, 0) * 0.8 over black.
  const Shot rg = render_quads(backend, "s1tr-oit-rg.fs", nullptr);
  S1_REQUIRE_LIVE(rg);
  check(rg, {66, 138, 0, 255}, 0);
  const Shot gr = render_quads(backend, "s1tr-oit-gr.fs", nullptr);
  S1_REQUIRE_LIVE(gr);
  check(gr, {66, 138, 0, 255}, 0);

  // The sorted over depends on it: green over red, red over green.
  const Shot overRg = render_quads(backend, "s1tr-quads-layer.fs", nullptr);
  S1_REQUIRE_LIVE(overRg);
  check(overRg, {51, 153, 0, 255}, 0);
  const Shot overGr = render_quads(backend, "s1tr-over-gr.fs", nullptr);
  S1_REQUIRE_LIVE(overGr);
  check(overGr, {128, 77, 0, 255}, 0);
}

TEST_CASE(
    "S1: QUEUE transparent draws after the opaque sources; without it a raster keeps "
    "its depth write and its draw order",
    "[gfx][raster][layer][s1]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  const Shot queued = render_quads(backend, "s1tr-quads-queue.fs", "s1tr-opaque-mid.vs");
  S1_REQUIRE_LIVE(queued);
  check(queued, {0, 153, 102, 255}, 115);

  const Shot alone = render_quads(backend, "s1tr-quads-none.fs", nullptr);
  S1_REQUIRE_LIVE(alone);
  check(alone, {51, 153, 0, 255}, 153);

  // Created first, so drawn first: its depth 0.6 then rejects the opaque blue
  // at 0.45.
  const Shot first = render_quads(backend, "s1tr-quads-none.fs", "s1tr-opaque-mid.vs");
  S1_REQUIRE_LIVE(first);
  check(first, {51, 153, 0, 255}, 153);
}
