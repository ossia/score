// A CSF storage image shape the engine cannot allocate is rejected at parse
// time. The cube + IS_ARRAY check ran before TYPE "image_cube" marked the image
// as a cube, so it never fired; and a 3D image with IS_ARRAY silently lost
// IS_ARRAY (shader board C18).
#include <isf.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <string>

namespace
{
std::string csf(const std::string& image)
{
  return R"_(/*{
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "RESOURCES": [ )_"
         + image + R"_( ],
  "PASSES": [ { "LOCAL_SIZE": [1, 1, 1], "EXECUTION_MODEL": { "TYPE": "MANUAL", "WORKGROUPS": [1, 1, 1] } } ]
}*/
void main() { }
)_";
}
}

TEST_CASE("CSF image shapes without a GPU type are rejected", "[isf][csf][image][C18]")
{
  const auto image = GENERATE(
      std::string{R"_({ "NAME": "img", "TYPE": "image_cube", "ACCESS": "read_write", "FORMAT": "rgba8", "WIDTH": "8", "HEIGHT": "8", "IS_ARRAY": true, "LAYERS": "2" })_"},
      std::string{R"_({ "NAME": "img", "TYPE": "image", "ACCESS": "read_write", "FORMAT": "rgba8", "WIDTH": "8", "HEIGHT": "8", "DIMENSIONS": 3, "DEPTH": "4", "IS_ARRAY": true })_"});
  CAPTURE(image);
  CHECK_THROWS_WITH(
      (isf::parser{{}, csf(image), 450, isf::parser::ShaderType::CSF}),
      Catch::Matchers::ContainsSubstring("IS_ARRAY"));
}

TEST_CASE("CSF image cube and 3D image without IS_ARRAY parse", "[isf][csf][image][C18]")
{
  const auto image = GENERATE(
      std::string{R"_({ "NAME": "img", "TYPE": "image_cube", "ACCESS": "read_write", "FORMAT": "rgba8", "WIDTH": "8", "HEIGHT": "8" })_"},
      std::string{R"_({ "NAME": "img", "TYPE": "image", "ACCESS": "read_write", "FORMAT": "rgba8", "WIDTH": "8", "HEIGHT": "8", "DIMENSIONS": 3, "DEPTH": "4" })_"});
  CAPTURE(image);
  CHECK_NOTHROW(isf::parser{{}, csf(image), 450, isf::parser::ShaderType::CSF});
}
