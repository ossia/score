// A storage image declared without VISIBILITY defaulted to "compute" in every
// shader, so a graphics ISF never declared or bound it; and "all" was a
// graphics visibility for the binding code but not for the code generator
// (shader board U43). The default follows the shader (compute in a CSF,
// fragment in a graphics shader), the spellings are normalised, and an unknown
// VISIBILITY is an error.
#include <isf.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <string>

namespace
{
std::string isf_with(const std::string& visibility)
{
  return R"_(/*{
  "ISFVSN": "2.0",
  "INPUTS": [ { "NAME": "accum", "TYPE": "image", "ACCESS": "read_write", "FORMAT": "r32f")_"
         + visibility + R"_( } ]
}*/
void main() { gl_FragColor = vec4(imageLoad(accum, ivec2(0)).r); }
)_";
}

// A copy: parser::data() returns the descriptor by value, so a reference into
// it dangles once the statement ends (the visibility read "" on macOS).
isf::csf_image_input image_of(const isf::parser& p)
{
  return ossia::get<isf::csf_image_input>(p.data().inputs.at(0).data);
}
}

TEST_CASE("a graphics storage image defaults to fragment visibility", "[gfx][isf][U43]")
{
  ::isf::parser p{{}, isf_with(""), 450, ::isf::parser::ShaderType::ISF};
  CHECK(image_of(p).visibility == "fragment");
  CHECK(p.fragment().find(" accum;") != std::string::npos);
}

TEST_CASE("a CSF storage image defaults to compute visibility", "[gfx][csf][U43]")
{
  const std::string cs = R"_(/*{
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "RESOURCES": [ { "NAME": "outputImage", "TYPE": "image", "ACCESS": "write_only", "FORMAT": "rgba8", "WIDTH": "8", "HEIGHT": "8" } ],
  "PASSES": [ { "LOCAL_SIZE": [8, 8, 1], "EXECUTION_MODEL": { "TYPE": "2D_IMAGE", "TARGET": "outputImage" } } ]
}*/
void main() { }
)_";
  ::isf::parser p{{}, cs, 450, ::isf::parser::ShaderType::CSF};
  CHECK(image_of(p).visibility == "compute");
}

TEST_CASE("VISIBILITY spellings are normalised", "[gfx][isf][U43]")
{
  struct Case
  {
    std::string json, expected;
  };
  const auto c = GENERATE(
      Case{R"_(, "VISIBILITY": "all")_", "vertex+fragment"},
      Case{R"_(, "VISIBILITY": "Both")_", "vertex+fragment"},
      Case{R"_(, "VISIBILITY": "graphics")_", "vertex+fragment"},
      Case{R"_(, "VISIBILITY": "VERTEX")_", "vertex"});
  CAPTURE(c.json);
  ::isf::parser p{{}, isf_with(c.json), 450, ::isf::parser::ShaderType::ISF};
  CHECK(image_of(p).visibility == c.expected);
  CHECK(p.fragment().find(" accum;") != std::string::npos);
}

TEST_CASE("an unknown VISIBILITY is rejected", "[gfx][isf][U43]")
{
  CHECK_THROWS_WITH(
      (::isf::parser{{}, isf_with(R"_(, "VISIBILITY": "pixel")_"), 450, ::isf::parser::ShaderType::ISF}),
      Catch::Matchers::ContainsSubstring("VISIBILITY"));
}
