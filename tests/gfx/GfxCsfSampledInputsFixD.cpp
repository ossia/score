// CSF inputs that are sampled textures in fragment ISF: TYPE image with no
// ACCESS / FORMAT (N68) and the audio inputs (N67). parse_csf() used to create
// their inlets and then emit no declaration, so the bake failed with
// "'<name>' : undeclared identifier". Pins that both are declared as samplers
// and bake, that an image input the compute path cannot sample is rejected at
// parse time, and that the sampled image reaches the shader on every backend.
#include <Gfx/Graph/ShaderCache.hpp>

#include <score_test/Gfx.hpp>

#include <isf.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <string>

namespace
{
std::string csf(const char* inputs)
{
  return std::string(R"_(/*{
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "INPUTS": [ )_") + inputs + R"_( ],
  "RESOURCES": [ { "NAME": "outImage", "TYPE": "image", "ACCESS": "write_only", "FORMAT": "RGBA8", "WIDTH": "64", "HEIGHT": "64" } ],
  "PASSES": [ { "LOCAL_SIZE": [8, 8, 1], "EXECUTION_MODEL": { "TYPE": "2D_IMAGE", "TARGET": "outImage" } } ]
}*/
void main()
{
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    if(any(greaterThanEqual(pos, imageSize(outImage)))) return;
    IMG_STORE(outImage, pos, IMG_NORM_PIXEL(src, vec2(pos) / 64.0));
}
)_";
}

QByteArray baked_source(const isf::parser& p)
{
  QString src = QString::fromStdString(p.compute_shader());
  src.replace("ISF_LOCAL_SIZE_X", "8");
  src.replace("ISF_LOCAL_SIZE_Y", "8");
  src.replace("ISF_LOCAL_SIZE_Z", "1");
  return src.toUtf8();
}

void check_bakes(const isf::parser& p)
{
  const QByteArray src = baked_source(p);
  INFO(src.toStdString());
  const auto& [shader, error] = score::gfx::ShaderCache::get(
      score::gfx::Vulkan, QShaderVersion(100), src, QShader::ComputeStage);
  INFO("bake error: " << error.toStdString());
  CHECK(error.isEmpty());
  CHECK(shader.isValid());
}
}

TEST_CASE("a CSF TYPE image input without ACCESS is a sampled texture", "[gfx][csf][isf]")
{
  isf::parser p{
      csf(R"_({ "NAME": "src", "TYPE": "image" })_"), isf::parser::ShaderType::CSF};
  const auto desc = p.data();
  REQUIRE(!desc.inputs.empty());
  REQUIRE(desc.inputs[0].name == "src");
  CHECK(ossia::get_if<isf::texture_input>(&desc.inputs[0].data));
  CHECK(p.compute_shader().find("uniform sampler2D src;") != std::string::npos);
  check_bakes(p);
}

TEST_CASE("a CSF TYPE image input with DIMENSIONS 3 is a sampler3D", "[gfx][csf][isf]")
{
  isf::parser p{
      csf(R"_({ "NAME": "src", "TYPE": "image", "DIMENSIONS": 3 })_"),
      isf::parser::ShaderType::CSF};
  CHECK(p.compute_shader().find("uniform sampler3D src;") != std::string::npos);
}

TEST_CASE("a CSF sampled image input the compute path cannot bind is rejected", "[gfx][csf][isf]")
{
  CHECK_THROWS_AS(
      (isf::parser{
          csf(R"_({ "NAME": "src", "TYPE": "image", "IS_ARRAY": true })_"),
          isf::parser::ShaderType::CSF}),
      isf::invalid_file);
}

TEST_CASE("CSF audio inputs are declared as samplers", "[gfx][csf][isf]")
{
  const auto type = GENERATE(
      std::string("audio"), std::string("audioFFT"), std::string("audioHistogram"));
  CAPTURE(type);
  isf::parser p{
      csf((R"_({ "NAME": "src", "TYPE": ")_" + type + R"_(", "MAX": 256 })_").c_str()),
      isf::parser::ShaderType::CSF};
  const auto code = p.compute_shader();
  INFO(code);
  CHECK(code.find("layout(binding = 3) uniform sampler2D src;") != std::string::npos);
  CHECK(code.find("layout(binding = 2, rgba8) writeonly uniform image2D outImage;")
        != std::string::npos);
  check_bakes(p);
}

TEST_CASE("a CSF TYPE image input samples its upstream", "[gfx][l3][csf]")
{
  const auto backend = GENERATE(from_range(score::test::gfx::platform_backends()));
  CAPTURE(score::test::gfx::backend_name(backend));

  score::test::gfx::IsfResult r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = score::test::gfx::render_isf_chain(
        backend, {QString{GFX_TEST_CORPUS_DIR "/isf-solid-color.fs"},
                  QString{GFX_TEST_CORPUS_DIR "/csf-image-sampled-fixd.cs"}});
  });

  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  if(const char* why = score::test::gfx::compute_shader_skip_reason(backend))
    SKIP(why);

  INFO("backend=" << r.backend << " error=" << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 1);
  const auto& img = r.outputs[0];
  REQUIRE(img.valid());
  const auto c = img.center();
  INFO("centre=(" << (int)c[0] << "," << (int)c[1] << "," << (int)c[2] << ","
                  << (int)c[3] << ")");
  CHECK(score::test::gfx::near(c, {255, 0, 255, 255}, 8));
}
