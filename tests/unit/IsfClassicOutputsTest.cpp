// Classic ISF renders its OUTPUTS as 2D (or layered) targets. A volume
// (DEPTH > 1), CUBEMAP or GENERATE_MIPS output was accepted and drawn as a
// plain 2D image, even with no pass targeting it (shader board F15). It is a
// parse error until the classic renderers implement it.
#include <isf.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <string>

TEST_CASE("classic ISF rejects outputs it cannot render", "[gfx][isf][F15]")
{
  const auto output = GENERATE(
      std::string{R"_({ "NAME": "vol", "WIDTH": 16, "HEIGHT": 16, "DEPTH": 8 })_"},
      std::string{R"_({ "NAME": "cube", "WIDTH": 16, "HEIGHT": 16, "CUBEMAP": true })_"},
      std::string{R"_({ "NAME": "mips", "WIDTH": 16, "HEIGHT": 16, "GENERATE_MIPS": true })_"});
  CAPTURE(output);
  const std::string src = R"_(/*{
  "ISFVSN": "2.0",
  "INPUTS": [],
  "OUTPUTS": [ { "NAME": "colour" }, )_" + output + R"_( ]
}*/
void main() { colour = vec4(1.0); }
)_";
  CHECK_THROWS_WITH(
      (::isf::parser{{}, src, 450, ::isf::parser::ShaderType::ISF}),
      Catch::Matchers::ContainsSubstring("classic ISF does not render"));
}

TEST_CASE("classic ISF accepts layered outputs", "[gfx][isf][F15]")
{
  const std::string src = R"_(/*{
  "ISFVSN": "2.0",
  "INPUTS": [],
  "OUTPUTS": [ { "NAME": "colour", "WIDTH": 16, "HEIGHT": 16, "LAYERS": 4 } ]
}*/
void main() { colour = vec4(1.0); }
)_";
  CHECK_NOTHROW((::isf::parser{{}, src, 450, ::isf::parser::ShaderType::ISF}));
}
