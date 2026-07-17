// Unit tests for the libisf importer branches that had 0% coverage:
//  - parser::parse_shadertoy()       (raw GLSL with mainImage entry point)
//  - parser::parse_shadertoy_json()  (shadertoy.com JSON export format)
//  - parser::parse_glsl_sandbox()    (glslsandbox.com style: time/mouse/resolution)
//  - parser::write_isf()             (ISF serialization / round-trip)
//  - the float_input MIN/MAX/DEFAULT inference lambdas in parse_input<>
//
// Source under test: src/plugins/score-plugin-gfx/3rdparty/libisf/src/isf.cpp
// (compiled into score_plugin_gfx).
//
// Five of these tests were written as [!mayfail] regression tests documenting
// real importer bugs; those bugs are now fixed in isf.cpp and the tests are
// regular must-pass tests. See testplan-reports/D-isf-importers.md and
// testplan-reports/FIX-ISF.md.

#include <isf.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <string>

using Catch::Approx;
using isf::parser;

namespace
{
// Build a minimal ISF shader with a single float input declared with the
// given extra JSON properties (e.g. R"(, "MIN": 0, "MAX": 10)").
static std::string make_float_isf(const std::string& extraJson)
{
  return "/*{ \"ISFVSN\": \"2\", \"INPUTS\": [ { \"NAME\": \"val\", \"TYPE\": "
         "\"float\""
         + extraJson
         + " } ] }*/\n"
           "void main() { isf_FragColor = vec4(val); }\n";
}

static isf::float_input parse_float_input(const std::string& extraJson)
{
  parser p{{}, make_float_isf(extraJson), 450, parser::ShaderType::ISF};
  auto d = p.data();
  REQUIRE(d.inputs.size() == 1);
  REQUIRE(d.inputs[0].name == "val");
  auto* f = ossia::get_if<isf::float_input>(&d.inputs[0].data);
  REQUIRE(f);
  return *f;
}

static bool contains(const std::string& haystack, const std::string& needle)
{
  return haystack.find(needle) != std::string::npos;
}
}

//---------------------------------------------------------------------------
// parse_shadertoy: raw ShaderToy-style GLSL
//---------------------------------------------------------------------------

static const std::string simple_shadertoy = R"_(
// A basic shadertoy-style shader
void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
  vec2 uv = fragCoord / iResolution.xy;
  vec3 col = 0.5 + 0.5 * cos(iTime + uv.xyx + vec3(0, 2, 4));
  col += texture(iChannel0, uv).rgb;
  col += texture(iChannel2, uv).rgb;
  fragColor = vec4(col, 1.0);
}
)_";

TEST_CASE("shadertoy: autodetected from mainImage signature", "[isf][shadertoy]")
{
  parser p{{}, simple_shadertoy, 450, parser::ShaderType::Autodetect};

  const auto frag = p.fragment();

  // Compat prelude + wrappers
  CHECK(contains(frag, "#version 450"));
  CHECK(contains(frag, "#define iResolution vec3(RENDERSIZE, 1.0)"));
  CHECK(contains(frag, "#define iTime TIME"));
  CHECK(contains(frag, "#define iTimeDelta TIMEDELTA"));
  CHECK(contains(frag, "#define iFrame FRAMEINDEX"));
  CHECK(contains(frag, "#define iGlobalTime iTime"));

  // Original source is embedded
  CHECK(contains(frag, "void mainImage(out vec4 fragColor, in vec2 fragCoord)"));

  // main() wrapper calling mainImage
  CHECK(contains(frag, "void main(void)"));
  CHECK(contains(frag, "mainImage(fragColor, isf_FragCoord.xy);"));
  CHECK(contains(frag, "isf_FragColor = fragColor;"));

  // Vertex shader generated
  const auto vert = p.vertex();
  CHECK(contains(vert, "#version 450"));
  CHECK(contains(vert, "isf_vertShaderInit"));

  // Only the referenced channels become image inputs
  const auto d = p.data();
  REQUIRE(d.inputs.size() == 2);
  CHECK(d.inputs[0].name == "iChannel0");
  CHECK(d.inputs[0].label == "Channel 0");
  CHECK(ossia::get_if<isf::image_input>(&d.inputs[0].data));
  CHECK(d.inputs[1].name == "iChannel2");
  CHECK(ossia::get_if<isf::image_input>(&d.inputs[1].data));

  CHECK(p.mode() == isf::descriptor::ISF);
}

TEST_CASE("shadertoy: explicit ShaderType gives same result", "[isf][shadertoy]")
{
  parser autodetected{{}, simple_shadertoy, 450, parser::ShaderType::Autodetect};
  parser explicitly{{}, simple_shadertoy, 450, parser::ShaderType::ShaderToy};

  CHECK(autodetected.fragment() == explicitly.fragment());
  CHECK(autodetected.vertex() == explicitly.vertex());
  CHECK(autodetected.data().inputs.size() == explicitly.data().inputs.size());
}

TEST_CASE("shadertoy: no channels referenced -> no inputs", "[isf][shadertoy]")
{
  const std::string src = R"_(
void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
  fragColor = vec4(1.0);
}
)_";
  parser p{{}, src, 450, parser::ShaderType::ShaderToy};
  CHECK(p.data().inputs.empty());
}

TEST_CASE("shadertoy: mainSound and mainVR categorization", "[isf][shadertoy]")
{
  const std::string src = R"_(
vec2 mainSound(int samp, float time2)
{
  return vec2(sin(6.2831 * 440.0 * time2));
}
void mainVR(out vec4 fragColor, in vec2 fragCoord, in vec3 ro, in vec3 rd)
{
  fragColor = vec4(rd, 1.0);
}
void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
  fragColor = vec4(iTime);
}
)_";
  parser p{{}, src, 450, parser::ShaderType::ShaderToy};
  const auto d = p.data();
  REQUIRE(d.categories.size() == 2);
  CHECK(d.categories[0] == "Shadertoy Sound");
  CHECK(d.categories[1] == "Shadertoy VR");
}

TEST_CASE(
    "shadertoy: ISFVSN marker prevents shadertoy autodetection", "[isf][shadertoy]")
{
  // A file that contains mainImage but declares itself as ISF must go
  // through the ISF path, not the shadertoy path.
  const std::string src = R"_(/*{
  "ISFVSN": "2",
  "INPUTS": [ { "NAME": "inputImage", "TYPE": "image" } ]
}*/
// void mainImage( <- red herring in a comment
void main() { isf_FragColor = IMG_THIS_PIXEL(inputImage); }
)_";
  parser p{{}, src, 450, parser::ShaderType::Autodetect};
  const auto d = p.data();
  REQUIRE(d.inputs.size() == 1);
  CHECK(d.inputs[0].name == "inputImage");
}

//---------------------------------------------------------------------------
// parse_shadertoy_json: shadertoy.com JSON export
//---------------------------------------------------------------------------

// NB: autodetection requires the exact prefix [{"ver":"
static const std::string shadertoy_json = R"_([{"ver":"0.1","info":{"id":"AbCdEf","name":"Test Shader","username":"testuser","description":"A test shader","tags":["plasma","procedural"]},"renderpass":[{"inputs":[{"channel":1,"ctype":"texture"},{"channel":2,"ctype":"music"},{"channel":0,"ctype":"webcam"}],"outputs":[],"code":"void mainImage(out vec4 c, in vec2 f){ c = vec4(texture(iChannel1, f/iResolution.xy).rgb, 1.0); }","name":"Image","type":"image"}]}])_";

TEST_CASE("shadertoy json: metadata and inputs extraction", "[isf][shadertoy_json]")
{
  parser p{{}, shadertoy_json, 450, parser::ShaderType::Autodetect};
  const auto d = p.data();

  CHECK(d.description == "Shadertoy: Test Shader\nA test shader");
  CHECK(d.credits == "By testuser on Shadertoy");
  REQUIRE(d.categories.size() == 2);
  CHECK(d.categories[0] == "plasma");
  CHECK(d.categories[1] == "procedural");

  // Channel inputs, in declaration order: texture -> image, music -> audio,
  // webcam -> image with annotated label.
  REQUIRE(d.inputs.size() == 3);
  CHECK(d.inputs[0].name == "iChannel1");
  CHECK(ossia::get_if<isf::image_input>(&d.inputs[0].data));
  CHECK(d.inputs[1].name == "iChannel2");
  CHECK(ossia::get_if<isf::audio_input>(&d.inputs[1].data));
  CHECK(d.inputs[2].name == "iChannel0");
  CHECK(ossia::get_if<isf::image_input>(&d.inputs[2].data));
  CHECK(d.inputs[2].label == "Channel 0 (webcam)");

  // The code of the image pass is embedded, plus the main() wrapper
  const auto frag = p.fragment();
  CHECK(contains(frag, "void mainImage(out vec4 c, in vec2 f)"));
  CHECK(contains(frag, "mainImage(fragColor, isf_FragCoord.xy);"));

  // Vertex shader is generated
  CHECK(contains(p.vertex(), "isf_vertShaderInit"));
}

TEST_CASE(
    "shadertoy json: generated fragment must be self-contained", "[isf][shadertoy_json]")
{
  // BUG (fixed): unlike parse_shadertoy(), parse_shadertoy_json() never
  // prepends GLSL45.versionPrelude / fragmentPrelude / defaultUniforms, yet
  // its compat block references TIME / RENDERSIZE / isf_process_uniforms /
  // isf_FragCoord / isf_FragColor. The emitted fragment therefore does not
  // compile stand-alone.
  parser p{{}, shadertoy_json, 450, parser::ShaderType::Autodetect};
  const auto frag = p.fragment();
  CHECK(contains(frag, "#version"));
  CHECK(contains(frag, "isf_process_uniforms")); // used...
  CHECK(contains(frag, "uniform process_t"));    // ...so it must be declared
}

TEST_CASE("shadertoy json: multipass and buffer categories", "[isf][shadertoy_json]")
{
  const std::string json
      = R"_([{"ver":"0.1","info":{"name":"MP"},"renderpass":[{"inputs":[],"code":"vec4 buf() { return vec4(1.); }","type":"buffer"},{"inputs":[],"code":"void mainImage(out vec4 c, in vec2 f){ c = vec4(1.0); }","type":"image"}]}])_";
  parser p{{}, json, 450, parser::ShaderType::ShaderToy};
  const auto d = p.data();
  REQUIRE(!d.categories.empty());
  CHECK(d.categories[0] == "Shadertoy Multipass");
  // Image pass code was picked, not the buffer pass
  CHECK(contains(p.fragment(), "void mainImage(out vec4 c, in vec2 f)"));
}

TEST_CASE("shadertoy json: sound pass categorization", "[isf][shadertoy_json]")
{
  const std::string json
      = R"_([{"ver":"0.1","info":{"name":"S"},"renderpass":[{"inputs":[],"code":"vec2 mainSound(int s, float t){ return vec2(0.); }","type":"sound"},{"inputs":[],"code":"void mainImage(out vec4 c, in vec2 f){ c = vec4(1.0); }","type":"image"}]}])_";
  parser p{{}, json, 450, parser::ShaderType::ShaderToy};
  const auto d = p.data();
  REQUIRE(!d.categories.empty());
  CHECK(d.categories[0] == "Shadertoy Sound");
}

TEST_CASE(
    "shadertoy json: iChannel in code without inputs array -> default 4 channels",
    "[isf][shadertoy_json]")
{
  const std::string json
      = R"_([{"ver":"0.1","info":{"name":"C"},"renderpass":[{"code":"void mainImage(out vec4 c, in vec2 f){ c = texture(iChannel0, f); }","type":"image"}]}])_";
  parser p{{}, json, 450, parser::ShaderType::ShaderToy};
  const auto d = p.data();
  REQUIRE(d.inputs.size() == 4);
  for(int i = 0; i < 4; i++)
  {
    CHECK(d.inputs[i].name == "iChannel" + std::to_string(i));
    CHECK(ossia::get_if<isf::image_input>(&d.inputs[i].data));
  }
}

TEST_CASE("shadertoy json: no categories -> tagged Shadertoy", "[isf][shadertoy_json]")
{
  const std::string json
      = R"_([{"ver":"0.1","renderpass":[{"code":"void mainImage(out vec4 c, in vec2 f){ c = vec4(0.); }","type":"image"}]}])_";
  parser p{{}, json, 450, parser::ShaderType::ShaderToy};
  const auto d = p.data();
  REQUIRE(d.categories.size() == 1);
  CHECK(d.categories[0] == "Shadertoy");
}

TEST_CASE("shadertoy json: error paths", "[isf][shadertoy_json][errors]")
{
  // Constructed through the ShaderToy path with the JSON prefix but broken
  // content -> invalid_file from the constructor.
  const std::string broken = R"_([{"ver":" oh no)_";
  CHECK_THROWS_AS(
      (parser{{}, broken, 450, parser::ShaderType::ShaderToy}), isf::invalid_file);

  // Direct calls on an existing parser instance
  parser p{{}, "void main() {}", 450, parser::ShaderType::Autodetect};

  // Root is not an array
  CHECK_THROWS_AS(p.parse_shadertoy_json(R"({"ver":"0.1"})"), isf::invalid_file);
  // Empty array
  CHECK_THROWS_AS(p.parse_shadertoy_json("[]"), isf::invalid_file);
  // First element is not an object
  CHECK_THROWS_AS(p.parse_shadertoy_json("[42]"), isf::invalid_file);
  // No image pass with code
  CHECK_THROWS_AS(
      p.parse_shadertoy_json(
          R"_([{"ver":"0.1","renderpass":[{"code":"vec2 mainSound(int s, float t){return vec2(0.);}","type":"sound"}]}])_"),
      isf::invalid_file);
  // Object with no renderpass at all
  CHECK_THROWS_AS(p.parse_shadertoy_json(R"_([{"ver":"0.1"}])_"), isf::invalid_file);
}

//---------------------------------------------------------------------------
// parse_glsl_sandbox
//---------------------------------------------------------------------------

static const std::string sandbox_src = R"_(#ifdef GL_ES
precision mediump float;
#endif
uniform float time;
uniform vec2 mouse;
uniform vec2 resolution;

void main( void ) {
  vec2 p = (gl_FragCoord.xy / resolution.xy) + mouse / 4.0;
  gl_FragColor = vec4(sin(time), p, 1.0);
}
)_";

TEST_CASE("glsl sandbox: autodetection and uniform rewriting", "[isf][glslsandbox]")
{
  parser p{{}, sandbox_src, 450, parser::ShaderType::Autodetect};

  const auto frag = p.fragment();
  // Compat header
  CHECK(contains(frag, "uniform float TIME;"));
  CHECK(contains(frag, "uniform vec2 MOUSE;"));
  CHECK(contains(frag, "uniform vec2 RENDERSIZE;"));
  CHECK(contains(frag, "out vec2 isf_FragNormCoord;"));

  // time/mouse/resolution rewritten in the body
  CHECK(contains(frag, "sin(TIME)"));
  CHECK(contains(frag, "RENDERSIZE.xy"));
  CHECK(contains(frag, "MOUSE / 4.0"));
  CHECK(!contains(frag, "uniform vec2 resolution;"));

  // Vertex shader
  const auto vert = p.vertex();
  CHECK(contains(vert, "in vec2 position;"));
  CHECK(contains(vert, "isf_FragNormCoord"));

  // No inputs are synthesized for the sandbox path
  CHECK(p.data().inputs.empty());
}

TEST_CASE("glsl sandbox: explicit ShaderType", "[isf][glslsandbox]")
{
  parser byDetect{{}, sandbox_src, 450, parser::ShaderType::Autodetect};
  parser byType{{}, sandbox_src, 450, parser::ShaderType::GLSLSandBox};
  CHECK(byDetect.fragment() == byType.fragment());
  CHECK(byDetect.vertex() == byType.vertex());
}

TEST_CASE(
    "glsl sandbox: uniform declarations must not be duplicated",
    "[isf][glslsandbox]")
{
  // BUG (fixed): the compat header prepends `uniform float TIME;` and
  // the textual replacement then turns the source's own `uniform float time;`
  // into a second `uniform float TIME;` declaration -> redeclaration error at
  // GLSL compile time.
  parser p{{}, sandbox_src, 450, parser::ShaderType::GLSLSandBox};
  const auto frag = p.fragment();
  const auto first = frag.find("uniform float TIME;");
  REQUIRE(first != std::string::npos);
  CHECK(frag.find("uniform float TIME;", first + 1) == std::string::npos);
}

TEST_CASE(
    "glsl sandbox: replacement must be identifier-aware", "[isf][glslsandbox]")
{
  // BUG (fixed): boost::replace_all("time" -> "TIME") also rewrites
  // identifiers *containing* the words, e.g. `lifetime` -> `lifeTIME`,
  // `mousepos` -> `MOUSEpos`, breaking user code.
  const std::string src = R"_(uniform float time;
float lifetime = 3.0;
void main( void ) { gl_FragColor = vec4(lifetime * time); }
)_";
  parser p{{}, src, 450, parser::ShaderType::GLSLSandBox};
  const auto frag = p.fragment();
  CHECK(contains(frag, "lifetime")); // must not be mangled to lifeTIME
}

//---------------------------------------------------------------------------
// write_isf + round-trip
//---------------------------------------------------------------------------

TEST_CASE("write_isf: full input-type round trip", "[isf][write_isf]")
{
  const std::string src = R"_(/*{
  "DESCRIPTION": "roundtrip test",
  "CREDIT": "unit test",
  "CATEGORIES": [ "Test", "Generator" ],
  "INPUTS": [
    { "NAME": "amount", "TYPE": "float", "MIN": 0.25, "MAX": 8.5, "DEFAULT": 2.5 },
    { "NAME": "mode", "TYPE": "long", "VALUES": [0, 1, 2], "LABELS": ["a", "b", "c"], "DEFAULT": 1 },
    { "NAME": "flag", "TYPE": "bool", "DEFAULT": true },
    { "NAME": "bang", "TYPE": "event" },
    { "NAME": "pos", "TYPE": "point2D", "MIN": [0.0, 0.0], "MAX": [1.0, 1.0], "DEFAULT": [0.5, 0.5] },
    { "NAME": "tint", "TYPE": "color", "DEFAULT": [1.0, 0.5, 0.25, 1.0] },
    { "NAME": "tex", "TYPE": "image" }
  ]
}*/
void main() { isf_FragColor = vec4(amount) * tint; }
)_";

  parser p{{}, src, 450, parser::ShaderType::ISF};
  const auto d1 = p.data();
  REQUIRE(d1.inputs.size() == 7);

  const std::string written = p.write_isf();

  // Header structure
  CHECK(written.starts_with("/*"));
  CHECK(contains(written, "\"DESCRIPTION\": \"roundtrip test\""));
  CHECK(contains(written, "\"CREDIT\": \"unit test\""));
  CHECK(contains(written, "\"INPUTS\": ["));
  CHECK(contains(written, "\"TYPE\": \"float\""));
  CHECK(contains(written, "\"TYPE\": \"long\""));
  CHECK(contains(written, "\"TYPE\": \"bool\""));
  CHECK(contains(written, "\"TYPE\": \"event\""));
  CHECK(contains(written, "\"TYPE\": \"point2D\""));
  CHECK(contains(written, "\"TYPE\": \"color\""));
  CHECK(contains(written, "\"TYPE\": \"image\""));

  // Re-parse the written ISF and compare descriptors
  parser p2{{}, written, 450, parser::ShaderType::ISF};
  const auto d2 = p2.data();

  CHECK(d2.description == d1.description);
  CHECK(d2.credits == d1.credits);
  REQUIRE(d2.categories.size() == d1.categories.size());
  CHECK(d2.categories[0] == "Test");
  CHECK(d2.categories[1] == "Generator");

  REQUIRE(d2.inputs.size() == d1.inputs.size());
  for(std::size_t i = 0; i < d1.inputs.size(); i++)
  {
    CHECK(d2.inputs[i].name == d1.inputs[i].name);
    CHECK(d2.inputs[i].data.index() == d1.inputs[i].data.index());
  }

  // float input values survive
  auto* f1 = ossia::get_if<isf::float_input>(&d1.inputs[0].data);
  auto* f2 = ossia::get_if<isf::float_input>(&d2.inputs[0].data);
  REQUIRE(f1);
  REQUIRE(f2);
  CHECK(f2->min == Approx(f1->min));
  CHECK(f2->max == Approx(f1->max));
  CHECK(f2->def == Approx(f1->def));

  // long input values/labels survive
  auto* l1 = ossia::get_if<isf::long_input>(&d1.inputs[1].data);
  auto* l2 = ossia::get_if<isf::long_input>(&d2.inputs[1].data);
  REQUIRE(l1);
  REQUIRE(l2);
  CHECK(l2->values.size() == l1->values.size());
  CHECK(l2->labels == l1->labels);
  CHECK(l2->def == l1->def);

  // bool default survives
  auto* b2 = ossia::get_if<isf::bool_input>(&d2.inputs[2].data);
  REQUIRE(b2);
  CHECK(b2->def == true);

  // point2d min/max/default survive
  auto* pt2 = ossia::get_if<isf::point2d_input>(&d2.inputs[4].data);
  REQUIRE(pt2);
  REQUIRE(pt2->def);
  CHECK((*pt2->def)[0] == Approx(0.5));
  CHECK((*pt2->def)[1] == Approx(0.5));

  // color default survives
  auto* c2 = ossia::get_if<isf::color_input>(&d2.inputs[5].data);
  REQUIRE(c2);
  REQUIRE(c2->def);
  CHECK((*c2->def)[0] == Approx(1.0));
  CHECK((*c2->def)[1] == Approx(0.5));
  CHECK((*c2->def)[2] == Approx(0.25));
  CHECK((*c2->def)[3] == Approx(1.0));
}

TEST_CASE("write_isf: imported shadertoy becomes an ISF with inputs", "[isf][write_isf]")
{
  parser p{{}, simple_shadertoy, 450, parser::ShaderType::ShaderToy};
  const std::string written = p.write_isf();

  CHECK(written.starts_with("/*"));
  CHECK(contains(written, "\"NAME\": \"iChannel0\""));
  CHECK(contains(written, "\"NAME\": \"iChannel2\""));
  CHECK(contains(written, "\"TYPE\": \"image\""));
  // Body: the converted fragment shader follows the header
  CHECK(contains(written, "*/"));
  CHECK(contains(written, "mainImage(fragColor, isf_FragCoord.xy);"));
}

TEST_CASE("write_isf: PASSES serialization", "[isf][write_isf]")
{
  // BUG (fixed): the trailing-comma cleanup in the PASSES emission loop
  // does `oss.str(fixed_string)` on a plain std::ostringstream, which resets
  // the write position to the *beginning* of the buffer; every subsequent
  // write then overwrites the start of the document. Any descriptor with a
  // non-empty PASSES array produces corrupted output.
  const std::string src = R"_(/*{
  "ISFVSN": "2",
  "INPUTS": [ { "NAME": "inputImage", "TYPE": "image" } ],
  "PASSES": [
    { "TARGET": "bufferA", "PERSISTENT": true, "FLOAT": true, "WIDTH": 640, "HEIGHT": 480 },
    { }
  ]
}*/
void main() { isf_FragColor = IMG_THIS_PIXEL(inputImage); }
)_";
  parser p{{}, src, 450, parser::ShaderType::ISF};
  REQUIRE(p.data().passes.size() == 2);

  const std::string written = p.write_isf();

  // The written document must still be a valid ISF: header first...
  CHECK(written.starts_with("/*"));
  CHECK(contains(written, "\"PASSES\": ["));
  CHECK(contains(written, "\"TARGET\": \"bufferA\""));

  // ...and re-parseable with the passes intact.
  parser p2{{}, written, 450, parser::ShaderType::ISF};
  const auto d2 = p2.data();
  REQUIRE(d2.passes.size() == 2);
  CHECK(d2.passes[0].target == "bufferA");
  CHECK(d2.passes[0].persistent);
  CHECK(d2.passes[0].float_storage);
}

TEST_CASE("write_isf: audio, cubemap, point3D and numeric-long round trip", "[isf][write_isf]")
{
  const std::string src = R"_(/*{
  "INPUTS": [
    { "NAME": "wave", "TYPE": "audio", "MAX": 128 },
    { "NAME": "spectrum", "TYPE": "audioFFT", "MAX": 512, "FILTER": "nearest" },
    { "NAME": "hist", "TYPE": "audioHistogram" },
    { "NAME": "env", "TYPE": "cubemap" },
    { "NAME": "dir", "TYPE": "point3D", "DEFAULT": [0.0, 1.0, 0.0], "AS_COLOR": true },
    { "NAME": "count", "TYPE": "long", "MIN": 1, "MAX": 16, "DEFAULT": 4 }
  ]
}*/
void main() { isf_FragColor = vec4(dir, float(count)); }
)_";
  parser p{{}, src, 450, parser::ShaderType::ISF};
  const auto d1 = p.data();
  REQUIRE(d1.inputs.size() == 6);

  const std::string written = p.write_isf();
  CHECK(contains(written, "\"TYPE\": \"audio\""));
  CHECK(contains(written, "\"TYPE\": \"audioFFT\""));
  CHECK(contains(written, "\"TYPE\": \"audioHistogram\""));
  CHECK(contains(written, "\"TYPE\": \"cubemap\""));
  CHECK(contains(written, "\"TYPE\": \"point3D\""));
  CHECK(contains(written, "\"AS_COLOR\": true"));

  parser p2{{}, written, 450, parser::ShaderType::ISF};
  const auto d2 = p2.data();
  REQUIRE(d2.inputs.size() == 6);

  auto* wave = ossia::get_if<isf::audio_input>(&d2.inputs[0].data);
  REQUIRE(wave);
  CHECK(wave->max == 128);

  auto* fft = ossia::get_if<isf::audioFFT_input>(&d2.inputs[1].data);
  REQUIRE(fft);
  CHECK(fft->max == 512);
  CHECK(fft->sampler.filter == "nearest");

  CHECK(ossia::get_if<isf::audioHist_input>(&d2.inputs[2].data));
  CHECK(ossia::get_if<isf::cubemap_input>(&d2.inputs[3].data));

  auto* dir = ossia::get_if<isf::point3d_input>(&d2.inputs[4].data);
  REQUIRE(dir);
  REQUIRE(dir->def);
  CHECK((*dir->def)[1] == Approx(1.0));
  CHECK(dir->as_color);

  auto* count = ossia::get_if<isf::long_input>(&d2.inputs[5].data);
  REQUIRE(count);
  CHECK(count->values.empty());
  REQUIRE(count->min);
  REQUIRE(count->max);
  CHECK(*count->min == 1);
  CHECK(*count->max == 16);
  CHECK(count->def == 4);
}

TEST_CASE("write_isf: json string escaping", "[isf][write_isf]")
{
  const std::string src = "/*{ \"DESCRIPTION\": \"line1\\nwith \\\"quotes\\\" and "
                          "back\\\\slash\", \"INPUTS\": [] }*/\nvoid main() {}\n";
  parser p{{}, src, 450, parser::ShaderType::ISF};
  const auto desc = p.data().description;
  REQUIRE(desc == "line1\nwith \"quotes\" and back\\slash");

  const std::string written = p.write_isf();
  parser p2{{}, written, 450, parser::ShaderType::ISF};
  CHECK(p2.data().description == desc);
}

//---------------------------------------------------------------------------
// float_input MIN/MAX/DEFAULT inference lambdas
//---------------------------------------------------------------------------

TEST_CASE("float input: no min/max/default -> [0, 1] @ 0", "[isf][float_input]")
{
  const auto f = parse_float_input("");
  CHECK(f.min == Approx(0.));
  CHECK(f.max == Approx(1.));
  CHECK(f.def == Approx(0.));
}

TEST_CASE("float input: only positive default", "[isf][float_input]")
{
  // min = -|def|, max = 2|def|
  const auto f = parse_float_input(R"(, "DEFAULT": 0.5)");
  CHECK(f.min == Approx(-0.5));
  CHECK(f.max == Approx(1.0));
  CHECK(f.def == Approx(0.5));
}

TEST_CASE("float input: only negative default", "[isf][float_input]")
{
  const auto f = parse_float_input(R"(, "DEFAULT": -2)");
  CHECK(f.min == Approx(-2.));
  CHECK(f.max == Approx(4.));
  CHECK(f.def == Approx(-2.));
}

TEST_CASE("float input: only max", "[isf][float_input]")
{
  // min derived from max: v - |v|
  const auto f = parse_float_input(R"(, "MAX": 10)");
  CHECK(f.min == Approx(0.));
  CHECK(f.max == Approx(10.));
  CHECK(f.def == Approx(0.));
}

TEST_CASE("float input: max and default", "[isf][float_input]")
{
  // min derived from def: v - |v|
  const auto f = parse_float_input(R"(, "MAX": 10, "DEFAULT": 4)");
  CHECK(f.min == Approx(0.));
  CHECK(f.max == Approx(10.));
  CHECK(f.def == Approx(4.));
}

TEST_CASE("float input: only positive min", "[isf][float_input]")
{
  // max derived from min: v + |v|; default clamped up to min
  const auto f = parse_float_input(R"(, "MIN": 2)");
  CHECK(f.min == Approx(2.));
  CHECK(f.max == Approx(4.));
  CHECK(f.def == Approx(2.)); // clamped from 0
}

TEST_CASE("float input: only negative min", "[isf][float_input]")
{
  // max derived from min: -v for v < 0
  const auto f = parse_float_input(R"(, "MIN": -3)");
  CHECK(f.min == Approx(-3.));
  CHECK(f.max == Approx(3.));
  CHECK(f.def == Approx(0.));
}

TEST_CASE("float input: reversed min/max are swapped", "[isf][float_input]")
{
  // Some ISF editor shaders use MIN > MAX to show reversed sliders
  const auto f = parse_float_input(R"(, "MIN": 5, "MAX": -5, "DEFAULT": 1)");
  CHECK(f.min == Approx(-5.));
  CHECK(f.max == Approx(5.));
  CHECK(f.def == Approx(1.));
}

TEST_CASE("float input: degenerate min == max == default", "[isf][float_input]")
{
  const auto f = parse_float_input(R"(, "MIN": 3, "MAX": 3, "DEFAULT": 3)");
  CHECK(f.min == Approx(3.));
  CHECK(f.max == Approx(6.)); // expanded to 2v
  CHECK(f.def == Approx(3.));
}

TEST_CASE("float input: degenerate negative min == max", "[isf][float_input]")
{
  const auto f = parse_float_input(R"(, "MIN": -4, "MAX": -4)");
  CHECK(f.min == Approx(-4.));
  CHECK(f.max == Approx(0.)); // v < 0 -> 0
}

TEST_CASE("float input: degenerate min == max with distinct default", "[isf][float_input]")
{
  const auto f = parse_float_input(R"(, "MIN": 2, "MAX": 2, "DEFAULT": 1.5)");
  // max re-derived from default: 2|def| = 3
  CHECK(f.min == Approx(2.));
  CHECK(f.max == Approx(3.));
  CHECK(f.def == Approx(2.)); // then clamped into [2, 3]
}

TEST_CASE("float input: default clamped to range", "[isf][float_input]")
{
  auto high = parse_float_input(R"(, "MIN": 1, "MAX": 10, "DEFAULT": 20)");
  CHECK(high.def == Approx(10.));

  auto low = parse_float_input(R"(, "MIN": 1, "MAX": 10, "DEFAULT": -5)");
  CHECK(low.def == Approx(1.));
}

TEST_CASE("float input: non-numeric values fall back to 0", "[isf][float_input]")
{
  // Strings / arrays are not numbers for a float input: everything defaults
  const auto f = parse_float_input(R"(, "MIN": "abc", "MAX": [1, 2], "DEFAULT": "x")");
  CHECK(f.min == Approx(0.));
  CHECK(f.max == Approx(1.));
  CHECK(f.def == Approx(0.));
}

TEST_CASE("color input: array min/max/default parsing", "[isf][float_input][color]")
{
  const std::string src = R"_(/*{
  "INPUTS": [
    { "NAME": "cWithAll", "TYPE": "color",
      "MIN": [0.0, 0.1, 0.2, 0.3], "MAX": [1.0, 0.9, 0.8, 0.7],
      "DEFAULT": [0.5, 0.5, 0.5, 0.5] },
    { "NAME": "cDefOnly", "TYPE": "color", "DEFAULT": [0.25, 0.5, 0.75, 1.0] },
    { "NAME": "cNothing", "TYPE": "color" }
  ]
}*/
void main() { isf_FragColor = cWithAll + cDefOnly + cNothing; }
)_";
  parser p{{}, src, 450, parser::ShaderType::ISF};
  const auto d = p.data();
  REQUIRE(d.inputs.size() == 3);

  auto* all = ossia::get_if<isf::color_input>(&d.inputs[0].data);
  REQUIRE(all);
  REQUIRE(all->min);
  REQUIRE(all->max);
  REQUIRE(all->def);
  CHECK((*all->min)[1] == Approx(0.1));
  CHECK((*all->max)[3] == Approx(0.7));
  CHECK((*all->def)[0] == Approx(0.5));

  // Default only: min/max derived per-component from the default
  auto* defOnly = ossia::get_if<isf::color_input>(&d.inputs[1].data);
  REQUIRE(defOnly);
  REQUIRE(defOnly->min);
  REQUIRE(defOnly->max);
  CHECK((*defOnly->min)[0] == Approx(-0.25));
  CHECK((*defOnly->max)[0] == Approx(0.5));

  // Nothing: [0, 1] range
  auto* nothing = ossia::get_if<isf::color_input>(&d.inputs[2].data);
  REQUIRE(nothing);
  REQUIRE(nothing->min);
  REQUIRE(nothing->max);
  CHECK((*nothing->min)[0] == Approx(0.));
  CHECK((*nothing->max)[0] == Approx(1.));
}

//---------------------------------------------------------------------------
// Robustness / error paths
//---------------------------------------------------------------------------

TEST_CASE("garbage input does not crash", "[isf][errors]")
{
  // Pure garbage: falls through autodetection, passthrough fragment
  const std::string garbage = "\x01\x02 utter garbage &*#@ not a shader";
  parser p{{}, garbage, 450, parser::ShaderType::Autodetect};
  CHECK(p.fragment() == garbage);
  CHECK(p.data().inputs.empty());

  // Empty input
  parser empty{{}, "", 450, parser::ShaderType::Autodetect};
  CHECK(empty.fragment().empty());

  // Sandbox parser on empty input
  parser sandbox{{}, "", 450, parser::ShaderType::GLSLSandBox};
  CHECK(!sandbox.vertex().empty());

  // ShaderToy parser on input without mainImage: wraps anyway, no crash
  parser st{{}, "void notMain() {}", 450, parser::ShaderType::ShaderToy};
  CHECK(contains(st.fragment(), "void notMain() {}"));
}

TEST_CASE("invalid ISF headers throw invalid_file", "[isf][errors]")
{
  // Header looks ISF-ish (INPUTS + comment) but JSON is broken
  CHECK_THROWS_AS(
      (parser{
          {}, "/*{ \"INPUTS\": }*/ void main() {}", 450, parser::ShaderType::Autodetect}),
      isf::invalid_file);

  // Forced ISF parse without any comment block
  CHECK_THROWS_AS(
      (parser{{}, "void main() {}", 450, parser::ShaderType::ISF}), isf::invalid_file);

  // Unterminated comment
  CHECK_THROWS_AS(
      (parser{{}, "/*{ \"INPUTS\": [] } void main() {}", 450, parser::ShaderType::ISF}),
      isf::invalid_file);

  // Root is not a JSON object
  CHECK_THROWS_AS(
      (parser{{}, "/*[1, 2, 3]*/ void main() {}", 450, parser::ShaderType::ISF}),
      isf::invalid_file);
}

TEST_CASE("parse_isf_header standalone", "[isf][errors]")
{
  auto [end, d] = parser::parse_isf_header(
      "/*{ \"DESCRIPTION\": \"x\", \"INPUTS\": [] }*/ void main() {}");
  CHECK(d.description == "x");
  CHECK(d.inputs.empty());

  CHECK_THROWS_AS(parser::parse_isf_header("no comment here"), isf::invalid_file);
}

TEST_CASE(
    "constructor: default_vertex_shader flag reflects the vertex argument",
    "[isf][errors]")
{
  // BUG (fixed): the constructor moves `vert` into m_sourceVertex and
  // *then* evaluates `vert.empty()` on the moved-from string. For any vertex
  // source longer than the SSO buffer the flag is true even though a custom
  // vertex shader was provided.
  const std::string longVertex
      = "void main() { gl_Position = vec4(0.0, 0.0, 0.0, 1.0); } // padding padding";
  parser p{longVertex, "void main() {}", 450, parser::ShaderType::Autodetect};
  CHECK(!p.data().default_vertex_shader);

  parser p2{{}, "void main() {}", 450, parser::ShaderType::Autodetect};
  CHECK(p2.data().default_vertex_shader);
}
