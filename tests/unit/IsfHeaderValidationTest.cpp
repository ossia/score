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

// A classic ISF draws its fullscreen mesh: PIPELINE_STATE VERTEX_COUNT /
// INSTANCE_COUNT were parsed and silently ignored, 64 requested points drawing
// 4 (shader board F16). They are refused, top-level or per pass, pointing to
// raw raster or VSA; the rest of PIPELINE_STATE stays accepted.
TEST_CASE("a classic ISF refuses VERTEX_COUNT and INSTANCE_COUNT", "[isf][header][F16]")
{
  for(const char* extra :
      {R"("PIPELINE_STATE": { "VERTEX_COUNT": 64 })",
       R"("PIPELINE_STATE": { "VERTEX_COUNT": "$USER" })",
       R"("PIPELINE_STATE": { "INSTANCE_COUNT": 4 })",
       R"("PIPELINE_STATE": { "INSTANCE_COUNT": "2 * 2" })",
       R"("PASSES": [ { "PIPELINE_STATE": { "VERTEX_COUNT": 6 } } ])",
       R"("PASSES": [ { "PIPELINE_STATE": { "INSTANCE_COUNT": 3 } } ])"})
  {
    CAPTURE(extra);
    const auto msg = refusal([&] { parseIsf(extra); });
    const bool names_key = msg.find("VERTEX_COUNT") != std::string::npos
                           || msg.find("INSTANCE_COUNT") != std::string::npos;
    CHECK(names_key);
    CHECK_THAT(msg, Catch::Matchers::ContainsSubstring("RAW_RASTER_PIPELINE"));
  }

  CHECK_NOTHROW(parseIsf(R"("PIPELINE_STATE": { "TOPOLOGY": "points", "CULL_MODE": "none" })"));
}

// BLEND_PER_ATTACHMENT has one entry per colour output. A shorter list used to
// repeat its last entry for the remaining outputs, a longer one dropped the
// extra ones, silently either way (shader board R18 / R31): both are refused.
TEST_CASE(
    "BLEND_PER_ATTACHMENT has one entry per fragment output", "[isf][header][R18]")
{
  const std::string one = R"({ "ENABLE": true, "SRC_COLOR": "one", "DST_COLOR": "one" })";
  const auto two_outputs = [](const std::string& state) {
    return isf::parser{
        std::string(k_vert),
        R"_(/*{
  "ISFVSN": "2",
  "MODE": "RAW_RASTER_PIPELINE",
  "VERTEX_INPUTS": [ { "TYPE": "vec3", "NAME": "position" } ],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "a" }, { "TYPE": "vec4", "NAME": "b" } ],
  "OUTPUTS": [ { "NAME": "outA", "TYPE": "color" }, { "NAME": "outB", "TYPE": "color" } ],
  "INPUTS": [],
  )_" + state + R"_(
}*/
void main() { a = vec4(1.0); b = vec4(1.0); }
)_",
        450, isf::parser::ShaderType::RawRasterPipeline};
  };

  // One output, two entries.
  CHECK_THAT(
      refusal([&] {
    parseRawRaster(R"("PIPELINE_STATE": { "BLEND_PER_ATTACHMENT": [ )" + one + ", " + one
                   + " ] }");
  }),
      Catch::Matchers::ContainsSubstring("BLEND_PER_ATTACHMENT"));
  // Two outputs, one entry.
  CHECK_THAT(
      refusal([&] {
    two_outputs(R"("PIPELINE_STATE": { "BLEND_PER_ATTACHMENT": [ )" + one + " ] }");
  }),
      Catch::Matchers::ContainsSubstring("2 fragment outputs"));

  CHECK_NOTHROW(two_outputs(
      R"("PIPELINE_STATE": { "BLEND_PER_ATTACHMENT": [ )" + one + ", " + one + " ] }"));
  CHECK_NOTHROW(
      parseRawRaster(R"("PIPELINE_STATE": { "BLEND_PER_ATTACHMENT": [ )" + one + " ] }"));

  // Without FRAGMENT_OUTPUTS the shader writes its own single colour output:
  // one entry is that output's. It was refused, counted against no output.
  const auto implicit_output = [](const std::string& state) {
    return isf::parser{
        std::string(k_vert),
        R"_(/*{
  "ISFVSN": "2",
  "MODE": "RAW_RASTER_PIPELINE",
  "VERTEX_INPUTS": [ { "TYPE": "vec3", "NAME": "position" } ],
  "INPUTS": [],
  )_" + state + R"_(
}*/
layout(location = 0) out vec4 fragColor;
void main() { fragColor = vec4(1.0); }
)_",
        450, isf::parser::ShaderType::RawRasterPipeline};
  };
  CHECK_NOTHROW(implicit_output(R"("PIPELINE_STATE": { "CULL_MODE": "none" })"));
  CHECK_NOTHROW(
      implicit_output(R"("PIPELINE_STATE": { "BLEND_PER_ATTACHMENT": [ )" + one + " ] }"));
  CHECK_THAT(
      refusal([&] {
    implicit_output(
        R"("PIPELINE_STATE": { "BLEND_PER_ATTACHMENT": [ )" + one + ", " + one + " ] }");
  }),
      Catch::Matchers::ContainsSubstring("BLEND_PER_ATTACHMENT"));
}

// Sampler keys QRhi has no setting for are parsed, then ignored by the engine:
// ANISOTROPY, BORDER_COLOR, LOD_BIAS, MIN_LOD, MAX_LOD and the mirror_once wrap
// (shader board U36). They stay accepted, and each one set produces a warning
// naming the input and the key.
TEST_CASE("unsupported sampler keys are kept and warned about", "[isf][header][sampler][U36]")
{
  const auto parse = [](const std::string& input) {
    return isf::parser{
        {},
        R"_(/*{ "ISFVSN": "2", "INPUTS": [ )_" + input
            + R"_( ] }*/
void main() { gl_FragColor = IMG_NORM_PIXEL(tex, vec2(0.5)); }
)_",
        450, isf::parser::ShaderType::ISF};
  };
  const auto warnings = [&](const std::string& input) { return parse(input).data().warnings; };
  const auto mentions = [](const std::vector<std::string>& w, const char* what) {
    for(const auto& s : w)
      if(s.find(what) != std::string::npos && s.find("tex") != std::string::npos)
        return true;
    return false;
  };

  CHECK(warnings(R"({ "NAME": "tex", "TYPE": "image", "FILTER": "linear", "WRAP": "repeat" })")
            .empty());

  for(const char* key : {"ANISOTROPY", "LOD_BIAS", "MIN_LOD", "MAX_LOD"})
  {
    CAPTURE(key);
    const auto w = warnings(
        std::string{R"({ "NAME": "tex", "TYPE": "image", ")"} + key + R"(": 2 })");
    CHECK(mentions(w, key));
  }
  CHECK(mentions(
      warnings(R"({ "NAME": "tex", "TYPE": "image", "BORDER_COLOR": "opaque_black" })"),
      "BORDER_COLOR"));
  CHECK(mentions(
      warnings(R"({ "NAME": "tex", "TYPE": "image", "WRAP_S": "mirror_once" })"),
      "mirror_once"));
  // Still parsed into the descriptor.
  const auto d = parse(R"({ "NAME": "tex", "TYPE": "image", "ANISOTROPY": 8 })").data();
  REQUIRE(!d.inputs.empty());
  const auto* img = ossia::get_if<isf::image_input>(&d.inputs[0].data);
  REQUIRE(img);
  CHECK(img->sampler.anisotropy == 8.f);
}

// A depth OUTPUT has no portable mip generation: Qt's Vulkan blit uses colour
// aspects (17 validation errors) and D3D refuses it. GENERATE_MIPS on depth is
// refused rather than skipped in silence (shader board R32); on colour it is
// accepted.
TEST_CASE("GENERATE_MIPS on a depth output is refused", "[isf][header][R32]")
{
  CHECK_THAT(
      refusal([] {
    parseRawRaster(R"("OUTPUTS": [ { "NAME": "color", "TYPE": "color" },
                                   { "NAME": "depth", "TYPE": "depth", "GENERATE_MIPS": true } ])");
  }),
      Catch::Matchers::ContainsSubstring("GENERATE_MIPS"));
  CHECK_NOTHROW(parseRawRaster(
      R"("OUTPUTS": [ { "NAME": "color", "TYPE": "color", "GENERATE_MIPS": true },
                      { "NAME": "depth", "TYPE": "depth" } ])"));
}

// An input name the generated GLSL already uses, or that GLSL reserves, made a
// shader that did not compile, or one where the engine's value silently
// replaced the user's (shader board U05): an INPUT named "layout" (the
// cubemap_from_source preset), "TIME" (#define TIME in the prelude), "isf_..."
// or "gl_...", two inputs with one name, a raw-raster input named like the
// control the engine adds ("Mode"). Each is refused naming the input; names are
// never renamed behind the author's back.
TEST_CASE("an input name that collides with generated code is refused", "[isf][header][U05]")
{
  const auto isf_with = [](const std::string& inputs) {
    return isf::parser{
        {},
        R"_(/*{ "ISFVSN": "2", "INPUTS": [ )_" + inputs + R"_( ] }*/
void main() { gl_FragColor = vec4(1.0); }
)_",
        450, isf::parser::ShaderType::ISF};
  };
  const auto f = [](const char* name) {
    return std::string{R"({ "NAME": ")"} + name + R"(", "TYPE": "float" })";
  };

  for(const char* name :
      {"layout", "uniform", "buffer", "sample", "vec4", "sampler2D", "TIME", "RENDERSIZE",
       "PASSINDEX", "FRAMEINDEX", "DATE", "IMG_PIXEL", "isf_FragColor", "isf_custom",
       "gl_Position", "a__b"})
  {
    CAPTURE(name);
    const auto msg = refusal([&] { isf_with(f(name)); });
    CHECK_THAT(msg, Catch::Matchers::ContainsSubstring(std::string{"\""} + name + "\""));
  }

  CHECK_THAT(
      refusal([&] { isf_with(f("speed") + ", " + f("speed")); }),
      Catch::Matchers::ContainsSubstring("\"speed\""));

  CHECK_THAT(
      refusal([&] {
    isf::parser{
        std::string(k_vert),
        R"_(/*{
  "ISFVSN": "2",
  "MODE": "RAW_RASTER_PIPELINE",
  "VERTEX_INPUTS": [ { "TYPE": "vec3", "NAME": "position" } ],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "color" } ],
  "INPUTS": [ { "NAME": "Mode", "TYPE": "float" } ]
}*/
void main() { color = vec4(1.0); }
)_",
        450, isf::parser::ShaderType::RawRasterPipeline};
  }),
      Catch::Matchers::ContainsSubstring("\"Mode\""));

  // Ordinary names, including ones that only contain a reserved word.
  CHECK_NOTHROW(isf_with(f("speed") + ", " + f("layoutScale") + ", " + f("time")
                         + ", " + f("_private") + ", " + f("myTIME")));
}

// A COMPUTE_SHADER declares its controls and resources in RESOURCES (shader
// board C07): there "image" is a storage image and "texture" a sampled one.
// INPUTS, the ISF list, is still read with its ISF meaning ("image" is a
// sampled texture unless it has an ACCESS or a FORMAT), with a deprecation
// warning: the published libraries' saved documents embed it.
namespace
{
isf::parser csfWith(const std::string& list, const std::string& entries)
{
  return isf::parser{
      {},
      R"_(/*{
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  )_" + list + R"_(: [
    )_" + entries + R"_(,
    { "NAME": "outputImage", "TYPE": "image", "ACCESS": "write_only", "FORMAT": "rgba8", "WIDTH": 8, "HEIGHT": 8 }
  ],
  "PASSES": [ { "LOCAL_SIZE": [8, 8, 1], "EXECUTION_MODEL": { "TYPE": "2D_IMAGE", "TARGET": "outputImage" } } ]
}*/
void main() { imageStore(outputImage, ivec2(gl_GlobalInvocationID.xy), vec4(1.0)); }
)_",
      450, isf::parser::ShaderType::CSF};
}

bool warnsAboutInputs(const isf::descriptor& d)
{
  for(const auto& w : d.warnings)
    if(w.find("INPUTS is deprecated") != std::string::npos
       && w.find("RESOURCES") != std::string::npos)
      return true;
  return false;
}
}

TEST_CASE("in a COMPUTE_SHADER's RESOURCES an image is a storage image", "[isf][header][csf][C07]")
{
  const auto d = csfWith(R"("RESOURCES")", R"({ "NAME": "gain", "TYPE": "float", "DEFAULT": 1.0 },
    { "NAME": "src", "TYPE": "image" },
    { "NAME": "tex", "TYPE": "texture" },
    { "NAME": "cube", "TYPE": "cubemap" })").data();
  REQUIRE(d.inputs.size() == 5);
  CHECK(ossia::get_if<isf::float_input>(&d.inputs[0].data));
  CHECK(ossia::get_if<isf::csf_image_input>(&d.inputs[1].data));
  CHECK(ossia::get_if<isf::texture_input>(&d.inputs[2].data));
  CHECK(ossia::get_if<isf::cubemap_input>(&d.inputs[3].data));
  CHECK(ossia::get_if<isf::csf_image_input>(&d.inputs[4].data));
  CHECK(!warnsAboutInputs(d));
}

TEST_CASE("a COMPUTE_SHADER still reads INPUTS the ISF way, and warns", "[isf][header][csf][C07]")
{
  const auto d = csfWith(R"("INPUTS")", R"({ "NAME": "gain", "TYPE": "float", "DEFAULT": 1.0 },
    { "NAME": "src", "TYPE": "image" },
    { "NAME": "layers", "TYPE": "image", "IS_ARRAY": true })").data();
  REQUIRE(d.inputs.size() == 4);
  CHECK(ossia::get_if<isf::float_input>(&d.inputs[0].data));
  auto* src = ossia::get_if<isf::texture_input>(&d.inputs[1].data);
  REQUIRE(src);
  CHECK(!src->is_array);
  auto* layers = ossia::get_if<isf::texture_input>(&d.inputs[2].data);
  REQUIRE(layers);
  CHECK(layers->is_array);
  CHECK(ossia::get_if<isf::csf_image_input>(&d.inputs[3].data));
  CHECK(warnsAboutInputs(d));
}

TEST_CASE("a texture takes IS_ARRAY, STATIC and CUBE", "[isf][header][csf][C07]")
{
  const auto p = csfWith(R"("RESOURCES")", R"({ "NAME": "layers", "TYPE": "texture", "IS_ARRAY": true },
    { "NAME": "lut", "TYPE": "texture", "STATIC": true },
    { "NAME": "env", "TYPE": "texture", "CUBE": true })");
  const auto& d = p.data();
  REQUIRE(d.inputs.size() == 4);
  auto* layers = ossia::get_if<isf::texture_input>(&d.inputs[0].data);
  REQUIRE(layers);
  CHECK(layers->is_array);
  auto* lut = ossia::get_if<isf::texture_input>(&d.inputs[1].data);
  REQUIRE(lut);
  CHECK(lut->is_static);
  auto* env = ossia::get_if<isf::texture_input>(&d.inputs[2].data);
  REQUIRE(env);
  CHECK(env->cube);
  CHECK_THAT(p.compute_shader(), Catch::Matchers::ContainsSubstring("sampler2DArray layers"));
  CHECK_THAT(p.compute_shader(), Catch::Matchers::ContainsSubstring("samplerCube env"));

  // A depth companion is an ISF image input's.
  CHECK_THROWS_WITH(
      csfWith(R"("RESOURCES")", R"({ "NAME": "d", "TYPE": "texture", "IS_DEPTH": true })"),
      Catch::Matchers::ContainsSubstring("IS_DEPTH"));
}

TEST_CASE("write_isf writes a compute shader's RESOURCES", "[isf][header][csf][C07][write_isf]")
{
  const auto p = csfWith(R"("RESOURCES")", R"({ "NAME": "tex", "TYPE": "texture", "IS_ARRAY": true },
    { "NAME": "lut", "TYPE": "texture", "STATIC": true })");
  const std::string written = p.write_isf();
  CAPTURE(written);
  CHECK_THAT(written, Catch::Matchers::ContainsSubstring("\"RESOURCES\""));
  CHECK_THAT(written, !Catch::Matchers::ContainsSubstring("\"INPUTS\""));
  const isf::parser again{{}, written, 450, isf::parser::ShaderType::CSF};
  const auto& d = again.data();
  REQUIRE(d.inputs.size() == 3);
  auto* tex = ossia::get_if<isf::texture_input>(&d.inputs[0].data);
  REQUIRE(tex);
  CHECK(tex->is_array);
  auto* lut = ossia::get_if<isf::texture_input>(&d.inputs[1].data);
  REQUIRE(lut);
  CHECK(lut->is_static);
  CHECK(ossia::get_if<isf::csf_image_input>(&d.inputs[2].data));
  CHECK(!warnsAboutInputs(d));
}

// A blend entry has one spelling per field: SRC_COLOR / SRC_ALPHA, DST_COLOR /
// DST_ALPHA, OP_COLOR / OP_ALPHA. The SRC / DST / OP shorthand for both is
// removed (shader board R14), and an unknown key is refused rather than
// ignored, so a removed or misspelt key never blends differently in silence.
TEST_CASE("a blend entry refuses the SRC / DST / OP shorthand and unknown keys", "[isf][header][R14]")
{
  for(const char* key : {"SRC", "DST", "OP"})
  {
    CAPTURE(key);
    const auto msg = refusal([&] {
      parseRawRaster(
          std::string{R"("PIPELINE_STATE": { "BLEND": { "ENABLE": true, ")"} + key
          + R"(": "one" } })");
    });
    CHECK_THAT(msg, Catch::Matchers::ContainsSubstring(std::string{key} + "_COLOR"));
  }
  CHECK_THAT(
      refusal([] {
    parseRawRaster(R"("PIPELINE_STATE": { "BLEND": { "ENABLE": true, "SCR_COLOR": "one" } })");
  }),
      Catch::Matchers::ContainsSubstring("SCR_COLOR"));
  CHECK_THAT(
      refusal([] {
    parseRawRaster(R"("LAYER": { "TARGETS": [ { "NAME": "color", "BLEND": { "SRC": "one" } } ] })");
  }),
      Catch::Matchers::ContainsSubstring("SRC_COLOR"));
  CHECK_NOTHROW(parseRawRaster(
      R"("PIPELINE_STATE": { "BLEND": { "ENABLE": true, "SRC_COLOR": "one", "DST_COLOR": "one",
         "OP_COLOR": "add", "SRC_ALPHA": "one", "DST_ALPHA": "one", "OP_ALPHA": "max",
         "COLOR_WRITE": "rgba" } })"));
}

// Textures (shader board R44). The flag that makes a texture hold depth is
// IS_DEPTH, like IS_ARRAY; DEPTH is the extent of a volume. A boolean DEPTH, the
// older spelling of the flag, is still read as IS_DEPTH, silently: the
// published libraries and saved documents use it.
// An auxiliary texture is a storage image only when its TYPE says so
// (storage_image, storage_3d, ...): a FORMAT other than rgba8 used to turn it
// into one silently.
TEST_CASE("texture DEPTH, IS_DEPTH and storage are explicit", "[isf][header][R44]")
{
  const auto isf_with = [](const std::string& input) {
    return isf::parser{
        {},
        R"_(/*{ "ISFVSN": "2", "INPUTS": [ )_" + input + R"_( ] }*/
void main() { gl_FragColor = vec4(1.0); }
)_",
        450, isf::parser::ShaderType::ISF};
  };
  for(const char* key : {"IS_DEPTH", "DEPTH"})
  {
    CAPTURE(key);
    for(const bool v : {true, false})
    {
      CAPTURE(v);
      const std::string flag = std::string{"\""} + key + "\": " + (v ? "true" : "false");
      const auto d = isf_with(R"({ "NAME": "depthTex", "TYPE": "image", )" + flag + " }").data();
      REQUIRE(!d.inputs.empty());
      const auto* img = ossia::get_if<isf::image_input>(&d.inputs[0].data);
      REQUIRE(img);
      CHECK(img->depth == v);
      CHECK(d.warnings.empty());
      const auto c = isf_with(R"({ "NAME": "env", "TYPE": "cubemap", )" + flag + " }").data();
      REQUIRE(!c.inputs.empty());
      const auto* cube = ossia::get_if<isf::cubemap_input>(&c.inputs[0].data);
      REQUIRE(cube);
      CHECK(cube->depth == v);
    }
  }

  const auto aux = [](const std::string& entry) {
    return isf::parser{
        std::string(k_vert),
        R"_(/*{
  "ISFVSN": "2",
  "MODE": "RAW_RASTER_PIPELINE",
  "VERTEX_INPUTS": [ { "TYPE": "vec3", "NAME": "position" } ],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "color" } ],
  "INPUTS": [ { "NAME": "geo", "TYPE": "geometry",
                "ATTRIBUTES": [ { "NAME": "position", "SEMANTIC": "position", "TYPE": "vec3", "ACCESS": "read_only" } ],
                "AUXILIARY": [ )_" + entry + R"_( ] } ]
}*/
void main() { color = vec4(1.0); }
)_",
        450, isf::parser::ShaderType::RawRasterPipeline};
  };
  const auto aux_tex = [&](const std::string& entry) {
    const auto d = aux(entry).data();
    for(const auto& in : d.inputs)
      if(auto* g = ossia::get_if<isf::geometry_input>(&in.data))
        if(!g->auxiliary_textures.empty())
          return g->auxiliary_textures.front();
    FAIL("no auxiliary texture");
    return isf::geometry_input::auxiliary_texture_request{};
  };

  // FORMAT alone no longer makes a storage image.
  CHECK_FALSE(aux_tex(R"({ "NAME": "lut", "TYPE": "image", "FORMAT": "r32f" })").is_storage);
  {
    const auto t = aux_tex(
        R"({ "NAME": "vox", "TYPE": "storage_3d", "FORMAT": "r32ui", "ACCESS": "read_only", "DEPTH": 64 })");
    CHECK(t.is_storage);
    CHECK(t.dimensions == 3);
    CHECK(t.depth_expression == "64");
  }
  CHECK(aux_tex(R"({ "NAME": "shadow", "TYPE": "image", "IS_DEPTH": true })").is_depth);
  CHECK(aux_tex(R"({ "NAME": "shadow", "TYPE": "image", "DEPTH": true })").is_depth);
  CHECK_FALSE(aux_tex(R"({ "NAME": "shadow", "TYPE": "image", "DEPTH": false })").is_depth);
  CHECK_THAT(
      refusal([&] { aux(R"({ "NAME": "img", "TYPE": "image", "STORAGE": true })"); }),
      Catch::Matchers::ContainsSubstring("storage_image"));
}

// VERTEX_COUNT makes a raw raster draw procedural: it draws that many vertices
// from gl_VertexIndex, without the mesh. Next to VERTEX_INPUTS, which read the
// mesh, it was silently clamped to the mesh's vertex count and drew it without
// its index buffer (shader board R21); that is refused. INSTANCE_COUNT alone
// is accepted with vertex inputs: it multiplies the mesh draws.
TEST_CASE("raw raster VERTEX_COUNT is for procedural draws", "[isf][header][R21]")
{
  CHECK_THAT(
      refusal([] { parseRawRaster(R"("PIPELINE_STATE": { "VERTEX_COUNT": 3 })"); }),
      Catch::Matchers::ContainsSubstring("VERTEX_INPUTS"));
  CHECK_NOTHROW(parseRawRaster(R"("PIPELINE_STATE": { "INSTANCE_COUNT": 4 })"));
  CHECK_NOTHROW(isf::parser{
      std::string(k_vert),
      R"_(/*{
  "ISFVSN": "2",
  "MODE": "RAW_RASTER_PIPELINE",
  "VERTEX_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "color" } ],
  "INPUTS": [],
  "PIPELINE_STATE": { "VERTEX_COUNT": 3, "INSTANCE_COUNT": 2 }
}*/
void main() { color = vec4(1.0); }
)_",
      450, isf::parser::ShaderType::RawRasterPipeline});
}

// MULTIVIEW N draws into N layers of every colour output: an output with fewer
// layers was accepted, then rendered as a single 2D view, or looped over the
// layers it had (shader board R39). It is refused; a cubemap has 6 layers.
TEST_CASE("a MULTIVIEW output has at least as many layers as views", "[isf][header][R39]")
{
  const auto mv = [](const std::string& outputs) {
    return isf::parser{
        std::string(k_vert),
        R"_(/*{
  "ISFVSN": "2",
  "MODE": "RAW_RASTER_PIPELINE",
  "MULTIVIEW": 4,
  "VERTEX_INPUTS": [ { "TYPE": "vec3", "NAME": "position" } ],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "color" } ],
  "OUTPUTS": [ )_" + outputs + R"_( ],
  "INPUTS": []
}*/
void main() { color = vec4(1.0); }
)_",
        450, isf::parser::ShaderType::RawRasterPipeline};
  };
  CHECK_THAT(
      refusal([&] { mv(R"({ "NAME": "views", "TYPE": "color", "LAYERS": 2 })"); }),
      Catch::Matchers::ContainsSubstring("MULTIVIEW"));
  CHECK_THAT(
      refusal([&] { mv(R"({ "NAME": "views", "TYPE": "color" })"); }),
      Catch::Matchers::ContainsSubstring("\"views\""));
  CHECK_NOTHROW(mv(R"({ "NAME": "views", "TYPE": "color", "LAYERS": 4 })"));
  CHECK_NOTHROW(mv(R"({ "NAME": "views", "TYPE": "color", "LAYERS": 8 })"));
  CHECK_NOTHROW(mv(R"({ "NAME": "cube", "TYPE": "color", "CUBEMAP": true })"));
}

// A depth companion comes from a render target's depth attachment; a volume
// has none, so IS_DEPTH on a DIMENSIONS 3 input is refused (shader board U32).
TEST_CASE("a volume input has no depth companion", "[isf][header][U32]")
{
  const auto isf_with = [](const std::string& input) {
    return isf::parser{
        {},
        R"_(/*{ "ISFVSN": "2", "INPUTS": [ )_" + input + R"_( ] }*/
void main() { gl_FragColor = vec4(1.0); }
)_",
        450, isf::parser::ShaderType::ISF};
  };
  CHECK_THAT(
      refusal([&] {
    isf_with(R"({ "NAME": "vol", "TYPE": "image", "DIMENSIONS": 3, "IS_DEPTH": true })");
  }),
      Catch::Matchers::ContainsSubstring("IS_DEPTH"));
  CHECK_NOTHROW(isf_with(R"({ "NAME": "arr", "TYPE": "image", "IS_ARRAY": true, "IS_DEPTH": true })"));
  CHECK_NOTHROW(isf_with(R"({ "NAME": "img", "TYPE": "image", "STATIC": true, "IS_DEPTH": true })"));
}

// demote is a keyword only with GL_EXT_demote_to_helper_invocation, which no
// generated shader enables: an input of that name compiles and is not refused.
TEST_CASE("an input named demote is accepted", "[isf][header][U05]")
{
  CHECK_NOTHROW(isf::parser{
      {},
      R"_(/*{ "ISFVSN": "2", "INPUTS": [ { "NAME": "demote", "TYPE": "float" } ] }*/
void main() { gl_FragColor = vec4(demote); }
)_",
      450, isf::parser::ShaderType::ISF});
}

// Header errors and warnings say where (review F-19): the input of an
// invalid VISIBILITY, the blend entry of an unknown key, a negative count as
// the integer written; a FORMAT the engine cannot allocate is a descriptor
// warning (shown with the shader), not only a line on stderr.
TEST_CASE("header messages name what they are about", "[isf][header]")
{
  {
    const auto msg = refusal([] {
      isf::parser{
          {},
          R"_(/*{ "ISFVSN": "2", "INPUTS": [ { "NAME": "accum", "TYPE": "image", "ACCESS": "read_write", "FORMAT": "r32f", "VISIBILITY": "pixel" } ] }*/
void main() { gl_FragColor = vec4(1.0); }
)_",
          450, isf::parser::ShaderType::ISF};
    });
    CHECK_THAT(msg, Catch::Matchers::ContainsSubstring("accum"));
  }
  {
    const std::string ok = R"({ "ENABLE": true, "SRC_COLOR": "one", "DST_COLOR": "one" })";
    const auto msg = refusal([&] {
      isf::parser{
          std::string(k_vert),
          R"_(/*{
  "ISFVSN": "2",
  "MODE": "RAW_RASTER_PIPELINE",
  "VERTEX_INPUTS": [ { "TYPE": "vec3", "NAME": "position" } ],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "a" }, { "TYPE": "vec4", "NAME": "b" } ],
  "OUTPUTS": [ { "NAME": "outA", "TYPE": "color" }, { "NAME": "outB", "TYPE": "color" } ],
  "INPUTS": [],
  "PIPELINE_STATE": { "BLEND_PER_ATTACHMENT": [ )_" + ok + R"_(, { "ENABLE": true, "SRC_COLOUR": "one" } ] }
}*/
void main() { a = vec4(1.0); b = vec4(1.0); }
)_",
          450, isf::parser::ShaderType::RawRasterPipeline};
    });
    CHECK_THAT(msg, Catch::Matchers::ContainsSubstring("BLEND_PER_ATTACHMENT[1]"));
  }
  {
    const auto msg = refusal([] {
      parseRawRaster(R"("PIPELINE_STATE": { "INSTANCE_COUNT": -1 })");
    });
    CHECK_THAT(msg, Catch::Matchers::ContainsSubstring("got -1."));
    CHECK_THAT(msg, !Catch::Matchers::ContainsSubstring("000000"));
  }
  {
    const isf::parser p{
        {},
        R"_(/*{ "ISFVSN": "2", "MODE": "COMPUTE_SHADER",
  "RESOURCES": [ { "NAME": "img", "TYPE": "image", "ACCESS": "write_only", "FORMAT": "rgba16", "WIDTH": 8, "HEIGHT": 8 } ],
  "PASSES": [ { "LOCAL_SIZE": [8, 8, 1], "EXECUTION_MODEL": { "TYPE": "2D_IMAGE", "TARGET": "img" } } ] }*/
void main() { }
)_",
        450, isf::parser::ShaderType::CSF};
    bool warned = false;
    for(const auto& w : p.data().warnings)
      warned |= w.find("rgba16") != std::string::npos;
    CHECK(warned);
  }
}
