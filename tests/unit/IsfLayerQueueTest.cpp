// The LAYER and QUEUE header keys of raw raster shaders, parser side: the
// parser accepts the documented keys and values, synthesises the fragment
// outputs and the default target, and rejects the rest; the draw stage
// declares the targets as outputs, the resolve stage (the same source with
// ISF_RESOLVE_PASS) declares them as samplers, and both get the depth
// convention of the shader's DEPTH_COMPARE; every resolve fixture bakes for
// every backend's language. What they render is tests/gfx/GfxRasterLayerQueue.
#include <Gfx/Graph/ShaderCache.hpp>

#include <QFile>

#include <isf.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <array>
#include <string>
#include <utility>

namespace
{
QString corpus(const char* file)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR "/") + QString::fromUtf8(file);
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

TEST_CASE("LAYER and QUEUE parse their keys and reject bad values", "[gfx][isf][layer]")
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
    "LAYER compiles the draw with its targets as outputs and the resolve with "
    "them as samplers",
    "[gfx][isf][layer]")
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

TEST_CASE("every LAYER resolve fixture bakes for every backend", "[gfx][isf][layer]")
{
  const auto api = GENERATE(
      score::gfx::OpenGL, score::gfx::Vulkan, score::gfx::D3D11, score::gfx::D3D12,
      score::gfx::Metal);
  CAPTURE(int(api));
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
      {"layer-quads-depth.fs", "layer-quads-mrt.fs", "layer-quads-depthin.fs", "layer-oit-rg.fs"})
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
