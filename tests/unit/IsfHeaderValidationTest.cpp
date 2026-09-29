// The ISF header keys ALPHA, COMPOSITE, QUEUE, LAYER, TOPOLOGY and the
// auxiliary texture ladders: a malformed header is refused with an
// isf::invalid_file naming the key, never with another exception type or a
// crash, and the well-formed forms generate what they promise.
//
// Pure CPU: parse, generate, inspect the GLSL. No QRhi, no display.
#include <isf.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <stdexcept>
#include <string>
#include <typeinfo>
#include <utility>

namespace
{
const char* k_vert = R"_(
void main()
{
    isf_vertShaderInit();
    gl_Position = vec4(position, 1.0);
    isf_vertShaderFinish();
}
)_";

// A raw-raster fragment shader with `extra` spliced into its JSON header.
std::string rawRaster(const std::string& extra, const std::string& body = {})
{
  return R"_(/*{
  "ISFVSN": "2",
  "MODE": "RAW_RASTER_PIPELINE",
  "VERTEX_INPUTS": [ { "TYPE": "vec3", "NAME": "position" } ],
  "VERTEX_OUTPUTS": [],
  "FRAGMENT_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "color" } ],
  "INPUTS": [])_"
         + (extra.empty() ? std::string{} : ",\n" + extra) + R"_(
}*/
)_" + (body.empty() ? std::string{"void main() { color = vec4(1.0); }\n"} : body);
}

std::string isfShader(const std::string& extra)
{
  return R"_(/*{
  "ISFVSN": "2",
  "INPUTS": [])_"
         + (extra.empty() ? std::string{} : ",\n" + extra) + R"_(
}*/
void main() { gl_FragColor = vec4(1.0); }
)_";
}

isf::parser parseRawRaster(const std::string& extra, const std::string& body = {})
{
  return isf::parser{
      std::string(k_vert), rawRaster(extra, body), 450,
      isf::parser::ShaderType::RawRasterPipeline};
}

isf::parser parseIsf(const std::string& extra)
{
  return isf::parser{{}, isfShader(extra), 450, isf::parser::ShaderType::ISF};
}

// The message of the invalid_file the parse throws; fails the test on any
// other outcome.
template <typename F>
std::string refusal(F&& parse)
{
  try
  {
    parse();
  }
  catch(const isf::invalid_file& e)
  {
    return e.what();
  }
  catch(const std::exception& e)
  {
    FAIL("parse threw a " << typeid(e).name() << " instead of invalid_file: " << e.what());
  }
  FAIL("parse accepted a malformed header");
  return {};
}
}

TEST_CASE("ALPHA and COMPOSITE accept their values in any case", "[isf][header]")
{
  auto p = parseIsf(R"("ALPHA": "PreMultiplied", "COMPOSITE": "Screen")");
  const auto d = p.data();
  CHECK(d.alpha == isf::alpha_mode::premultiplied);
  CHECK(d.composite == isf::composite_mode::screen);
  CHECK(isf::resolve_alpha(d) == isf::alpha_mode::premultiplied);
  CHECK(isf::resolve_composite(d) == isf::composite_mode::screen);
}

TEST_CASE("an unspecified ALPHA is straight for ISF, premultiplied for raw raster", "[isf][header]")
{
  CHECK(isf::resolve_alpha(parseIsf("").data()) == isf::alpha_mode::straight);
  CHECK(isf::resolve_alpha(parseRawRaster("").data()) == isf::alpha_mode::premultiplied);
  CHECK(isf::resolve_composite(parseIsf("").data()) == isf::composite_mode::over);
}

TEST_CASE("a straight ISF output composited by multiply is premultiplied in the shader", "[isf][header]")
{
  auto p = parseIsf(R"("COMPOSITE": "multiply")");
  INFO(p.fragment());
  CHECK(p.fragment().find("isf_FragColor.rgb *= isf_FragColor.a;") != std::string::npos);
  CHECK(p.fragment().find("#define main isf_user_main") != std::string::npos);

  auto over = parseIsf(R"("COMPOSITE": "over")");
  CHECK(over.fragment().find("isf_FragColor.rgb *= isf_FragColor.a;") == std::string::npos);
}

TEST_CASE("malformed ALPHA / COMPOSITE / QUEUE values are refused by name", "[isf][header]")
{
  CHECK_THAT(
      refusal([] { parseIsf(R"("ALPHA": "linear")"); }),
      Catch::Matchers::ContainsSubstring("ALPHA"));
  CHECK_THAT(
      refusal([] { parseIsf(R"("ALPHA": 1)"); }),
      Catch::Matchers::ContainsSubstring("ALPHA"));
  CHECK_THAT(
      refusal([] { parseIsf(R"("COMPOSITE": "darken")"); }),
      Catch::Matchers::ContainsSubstring("COMPOSITE"));
  // Bytes above 0x7f: lower-casing them must not be undefined behaviour.
  CHECK_THAT(
      refusal([] { parseIsf("\"COMPOSITE\": \"\xc3\x89t\xc3\xa9\""); }),
      Catch::Matchers::ContainsSubstring("COMPOSITE"));
  CHECK_THAT(
      refusal([] { parseRawRaster(R"("QUEUE": "overlay")"); }),
      Catch::Matchers::ContainsSubstring("QUEUE"));
}

TEST_CASE("QUEUE decides draws_transparent, else LAYER does", "[isf][header]")
{
  CHECK(isf::draws_transparent(parseRawRaster(R"("QUEUE": "Transparent")").data()));
  CHECK(!isf::draws_transparent(parseRawRaster(R"("QUEUE": "opaque")").data()));
  CHECK(!isf::draws_transparent(parseRawRaster("").data()));
}

TEST_CASE("LAYER is only accepted on raw-raster shaders", "[isf][header][layer]")
{
  CHECK_THAT(
      refusal([] { parseIsf(R"("LAYER": {})"); }),
      Catch::Matchers::ContainsSubstring("RAW_RASTER_PIPELINE"));
}

TEST_CASE("a LAYER without TARGETS gets one target per fragment output", "[isf][header][layer]")
{
  auto p = parseRawRaster(R"("LAYER": {})");
  const auto d = p.data();
  REQUIRE(d.layer.enabled());
  REQUIRE(d.layer.targets.size() == 1);
  CHECK(d.layer.targets[0].name == "color");
  CHECK(d.layer.targets[0].format == "rgba16f");
  CHECK(d.outputs.empty());
  CHECK(isf::draws_transparent(d));
}

TEST_CASE("malformed LAYER entries are refused by name", "[isf][header][layer]")
{
  CHECK_THAT(
      refusal([] { parseRawRaster(R"("LAYER": [])"); }),
      Catch::Matchers::ContainsSubstring("LAYER must be an object"));
  CHECK_THAT(
      refusal([] { parseRawRaster(R"("LAYER": { "TARGETS": [] })"); }),
      Catch::Matchers::ContainsSubstring("non-empty"));
  CHECK_THAT(
      refusal([] { parseRawRaster(R"("LAYER": { "BOGUS": 1 })"); }),
      Catch::Matchers::ContainsSubstring("unknown key BOGUS"));
  CHECK_THAT(
      refusal([] {
    parseRawRaster(R"("LAYER": { "TARGETS": [ { "NAME": "1abc" } ] })");
  }),
      Catch::Matchers::ContainsSubstring("GLSL identifier"));
  CHECK_THAT(
      refusal([] {
    parseRawRaster(R"("LAYER": { "TARGETS": [ { "NAME": "color", "FORMAT": "bgra8" } ] })");
  }),
      Catch::Matchers::ContainsSubstring("FORMAT"));
  CHECK_THAT(
      refusal([] {
    parseRawRaster(R"("LAYER": { "TARGETS": [ { "NAME": "color", "CLEAR": [0,0,0,0,0] } ] })");
  }),
      Catch::Matchers::ContainsSubstring("CLEAR"));
  CHECK_THAT(
      refusal([] {
    parseRawRaster(
        R"("LAYER": { "TARGETS": [ { "NAME": "color", "BLEND": true, "COMPOSITE": "add" } ] })");
  }),
      Catch::Matchers::ContainsSubstring("exclusive"));
  CHECK_THAT(
      refusal([] {
    parseRawRaster(R"("LAYER": { "TARGETS": [ { "NAME": "other" } ] })");
  }),
      Catch::Matchers::ContainsSubstring("FRAGMENT_OUTPUTS"));
}

TEST_CASE("a LAYER RESOLVE needs its section in the fragment shader", "[isf][header][layer]")
{
  CHECK_THAT(
      refusal([] { parseRawRaster(R"("LAYER": { "RESOLVE": {} })"); }),
      Catch::Matchers::ContainsSubstring("ISF_RESOLVE_PASS"));

  const std::string body = R"_(
#if defined(ISF_RESOLVE_PASS)
void main() { isf_FragColor = LAYER_TEXEL(color); }
#else
void main() { color = vec4(1.0); }
#endif
)_";
  auto p = parseRawRaster(R"("LAYER": { "RESOLVE": { "COMPOSITE": "add" } })", body);
  const auto d = p.data();
  REQUIRE(d.layer.resolve.declared);
  CHECK(d.layer.resolve.composite == isf::composite_mode::add);
  CHECK(d.layer.resolve.fragment.find("#define ISF_RESOLVE_PASS 1") != std::string::npos);
  CHECK(d.layer.resolve.fragment.find("uniform sampler2D color;") != std::string::npos);
}

TEST_CASE("DEPTH_COMPARE decides the depth convention macros", "[isf][header][depth]")
{
  const auto convention = [](const std::string& cmp) {
    auto p = parseRawRaster(R"("PIPELINE_STATE": { "DEPTH_COMPARE": ")" + cmp + "\" }");
    return std::pair{isf::depth_nearer_is_greater(p.data()), p.fragment()};
  };

  CHECK(isf::depth_nearer_is_greater(parseRawRaster("").data()));
  for(std::string greater : {"greater", "GREATER_OR_EQUAL", "greater-equal", "gequal"})
  {
    auto [g, frag] = convention(greater);
    INFO(greater);
    CHECK(g);
    CHECK(frag.find("#define ISF_DEPTH_NEARER_IS_GREATER 1") != std::string::npos);
  }
  for(std::string less : {std::string("less"), std::string("LessOrEqual"),
                           std::string("always"), std::string(200, 'g')})
  {
    auto [g, frag] = convention(less);
    INFO(less);
    CHECK(!g);
    CHECK(frag.find("#define ISF_DEPTH_NEARER_IS_GREATER 0") != std::string::npos);
  }
}

TEST_CASE("an unknown geometry TOPOLOGY is refused by name", "[isf][header]")
{
  const std::string csf = std::string(R"_(/*{
  "ISFVSN": "2",
  "MODE": "COMPUTE_SHADER",
  "RESOURCES": [ { "NAME": "geo", "TYPE": "geometry", "ACCESS": "read_write", "TOPOLOGY": "quads" } ],
  "PASSES": [ { "LOCAL_SIZE": [64, 1, 1] } ]
}*/
void main() { }
)_");
  CHECK_THAT(
      refusal([&] { isf::parser{{}, csf, 450, isf::parser::ShaderType::CSF}; }),
      Catch::Matchers::ContainsSubstring("TOPOLOGY"));
}

TEST_CASE("a run of indexed auxiliary textures collapses into one array", "[isf][header][ladder]")
{
  auto p = parseRawRaster(R"("AUXILIARY": [
    { "NAME": "baseColor0", "TYPE": "texture" },
    { "NAME": "baseColor1", "TYPE": "texture" },
    { "NAME": "baseColor2", "TYPE": "texture" },
    { "NAME": "lonely0", "TYPE": "texture" }
  ])");
  const auto d = p.data();
  REQUIRE(d.auxiliary_textures.size() == 4);
  for(int i = 0; i < 3; i++)
  {
    CHECK(d.auxiliary_textures[i].ladder_base == "baseColor");
    CHECK(d.auxiliary_textures[i].ladder_index == i);
    CHECK(d.auxiliary_textures[i].ladder_size == 3);
  }
  CHECK(!d.auxiliary_textures[3].in_ladder());
  INFO(p.fragment());
  CHECK(p.fragment().find("texture2D baseColor_tex[3];") != std::string::npos);
  CHECK(p.fragment().find("uniform sampler baseColor_smp;") != std::string::npos);
  CHECK(p.fragment().find("sampler2D lonely0;") != std::string::npos);
}

TEST_CASE("an auxiliary texture index too large for an int is not a ladder rung", "[isf][header][ladder]")
{
  // std::stoi threw std::out_of_range on this, which no caller catches.
  auto p = parseRawRaster(R"("AUXILIARY": [
    { "NAME": "t0", "TYPE": "texture" },
    { "NAME": "t99999999999999999999999", "TYPE": "texture" },
    { "NAME": "u0", "TYPE": "texture" },
    { "NAME": "u01", "TYPE": "texture" }
  ])");
  const auto d = p.data();
  REQUIRE(d.auxiliary_textures.size() == 4);
  for(const auto& t : d.auxiliary_textures)
    CHECK(!t.in_ladder());
}
