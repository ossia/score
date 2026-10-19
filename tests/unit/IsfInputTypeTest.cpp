// An input or resource whose TYPE the parser does not know was dropped
// silently, its port and its declaration with it (shader board U38). It is a
// parse error naming the entry now.
#include <isf.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <string>

TEST_CASE("an ISF input of an unknown TYPE is rejected", "[gfx][isf][U38]")
{
  const auto type = GENERATE(std::string{"\"audioHist\""}, std::string{"\"flaot\""}, std::string{"3"});
  CAPTURE(type);
  const std::string src = R"_(/*{
  "ISFVSN": "2.0",
  "INPUTS": [ { "NAME": "gain", "TYPE": "float" }, { "NAME": "spectrum", "TYPE": )_" + type
                          + R"_( } ]
}*/
void main() { gl_FragColor = vec4(gain); }
)_";
  CHECK_THROWS_WITH(
      (::isf::parser{{}, src, 450, ::isf::parser::ShaderType::ISF}),
      Catch::Matchers::ContainsSubstring("INPUTS[1]"));
}

TEST_CASE("a CSF resource of an unknown or missing TYPE is rejected", "[gfx][csf][U38]")
{
  const auto resource = GENERATE(
      std::string{R"_({ "NAME": "buf", "TYPE": "buffer", "LAYOUT": [ { "NAME": "v", "TYPE": "float[]" } ] })_"},
      std::string{R"_({ "NAME": "buf", "LAYOUT": [ { "NAME": "v", "TYPE": "float[]" } ] })_"});
  CAPTURE(resource);
  const std::string src = R"_(/*{
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "RESOURCES": [ )_" + resource + R"_( ],
  "PASSES": [ { "LOCAL_SIZE": [1, 1, 1], "EXECUTION_MODEL": { "TYPE": "MANUAL", "WORKGROUPS": [1, 1, 1] } } ]
}*/
void main() { }
)_";
  CHECK_THROWS_WITH(
      (::isf::parser{{}, src, 450, ::isf::parser::ShaderType::CSF}),
      Catch::Matchers::ContainsSubstring("RESOURCES[0]"));
}

TEST_CASE("audioHistogram and audioFloatHistogram are known", "[gfx][isf][U38]")
{
  for(std::string type : {"audioHistogram", "audioFloatHistogram", "AUDIOFFT"})
  {
    CAPTURE(type);
    const std::string src = R"_(/*{ "ISFVSN": "2.0", "INPUTS": [ { "NAME": "a", "TYPE": ")_" + type
                            + R"_(" } ] }*/
void main() { gl_FragColor = vec4(1.0); }
)_";
    ::isf::parser p{{}, src, 450, ::isf::parser::ShaderType::ISF};
    CHECK(p.data().inputs.size() == 1);
  }
}
