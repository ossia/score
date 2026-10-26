// An INPUTS `image` that declares ACCESS or FORMAT is a storage image
// (csf_image_input, isf.cpp parse of INPUTS): it is an image2D, read with
// imageLoad, and cannot be sampled. A shader that samples it anyway --
// IMG_NORM_PIXEL / IMG_PIXEL / IMG_THIS_PIXEL / texture / texelFetch -- is
// refused with an isf::invalid_file that says why and how to fix it, instead of
// the raw GLSL "no matching overloaded function" from the compiler.
//
// Pure CPU: the parser and its GLSL generation.
#include <isf.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <string>
#include <typeinfo>

namespace
{
std::string isfSource(const std::string& input, const std::string& body)
{
  return R"_(/*{
  "ISFVSN": "2",
  "INPUTS": [ )_"
         + input + R"_( ]
}*/
void main() { )_"
         + body + " }\n";
}

constexpr const char* k_vert = R"_(
void main()
{
    isf_vertShaderInit();
    gl_Position = vec4(position, 1.0);
    isf_vertShaderFinish();
}
)_";

std::string raster(const std::string& input, const std::string& body)
{
  return R"_(/*{
  "ISFVSN": "2",
  "MODE": "RAW_RASTER_PIPELINE",
  "VERTEX_INPUTS": [ { "TYPE": "vec3", "NAME": "position" } ],
  "VERTEX_OUTPUTS": [],
  "FRAGMENT_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "color" } ],
  "INPUTS": [ )_"
         + input + R"_( ]
}*/
void main() { )_"
         + body + " }\n";
}

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
  FAIL("parse accepted a sampled storage image");
  return {};
}
}

TEST_CASE("sampling a storage image declared in INPUTS is refused clearly", "[isf][storage]")
{
  using Catch::Matchers::ContainsSubstring;
  const std::string input = GENERATE(
      std::string{R"({ "NAME": "img", "TYPE": "image", "ACCESS": "read_only" })"},
      std::string{R"({ "NAME": "img", "TYPE": "image", "FORMAT": "RGBA8" })"});
  const std::string read = GENERATE(
      std::string{"IMG_NORM_PIXEL(img, vec2(0.5))"}, std::string{"IMG_PIXEL(img, vec2(1.))"},
      std::string{"IMG_THIS_PIXEL(img)"}, std::string{"IMG_THIS_NORM_PIXEL(img)"},
      std::string{"texture(img, vec2(0.5))"}, std::string{"texelFetch(img, ivec2(0), 0)"});
  CAPTURE(input, read);

  const std::string msg = refusal([&] {
    isf::parser{{}, isfSource(input, "gl_FragColor = " + read + ";"), 450,
                isf::parser::ShaderType::ISF};
  });
  CHECK_THAT(msg, ContainsSubstring("\"img\""));
  CHECK_THAT(msg, ContainsSubstring("storage image"));
  CHECK_THAT(msg, ContainsSubstring("ACCESS"));
  CHECK_THAT(msg, ContainsSubstring("FORMAT"));
  CHECK_THAT(msg, ContainsSubstring("imageLoad"));

  const std::string rmsg = refusal([&] {
    isf::parser{
        std::string(k_vert), raster(input, "color = " + read + ";"), 450,
        isf::parser::ShaderType::RawRasterPipeline};
  });
  CHECK_THAT(rmsg, ContainsSubstring("storage image"));
}

TEST_CASE("a storage image read with imageLoad, or a sampled image, is accepted", "[isf][storage]")
{
  CHECK_NOTHROW(isf::parser{
      {},
      isfSource(R"({ "NAME": "img", "TYPE": "image", "ACCESS": "read_only", "FORMAT": "RGBA8" })",
          "gl_FragColor = imageLoad(img, ivec2(0));"),
      450, isf::parser::ShaderType::ISF});
  CHECK_NOTHROW(isf::parser{
      {}, isfSource(R"({ "NAME": "img", "TYPE": "image" })", "gl_FragColor = IMG_NORM_PIXEL(img, vec2(0.5));"),
      450, isf::parser::ShaderType::ISF});
  // A name that merely contains the storage image's.
  CHECK_NOTHROW(isf::parser{
      {},
      isfSource(R"({ "NAME": "img", "TYPE": "image", "ACCESS": "read_only" }, { "NAME": "img2", "TYPE": "image" })",
          "gl_FragColor = imageLoad(img, ivec2(0)) + IMG_NORM_PIXEL(img2, vec2(0.5)) + texture(img2, vec2(0.));"),
      450, isf::parser::ShaderType::ISF});
}
