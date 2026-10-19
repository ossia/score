// The ISF header is the first block comment holding a JSON object. A licence
// comment before it hid it ("Missing start comment"), and the JSON had to be
// strict: no // comments, no trailing commas (shader board U01).
#include <isf.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <string>

TEST_CASE("the ISF header may follow a licence comment", "[gfx][isf][U01]")
{
  const std::string src = R"_(/*
 * Copyright (c) 2026 Someone. MIT licence.
 */
/*{
  "ISFVSN": "2.0",
  "DESCRIPTION": "after a licence",
  "INPUTS": [ { "NAME": "gain", "TYPE": "float" } ]
}*/
void main() { gl_FragColor = vec4(gain); }
)_";
  ::isf::parser p{{}, src, 450, ::isf::parser::ShaderType::ISF};
  CHECK(p.data().description == "after a licence");
  CHECK(p.data().inputs.size() == 1);
  // The licence comment is not left in front of the generated code's #version.
  CHECK(p.fragment().find("Copyright") == std::string::npos);
}

TEST_CASE("the ISF header accepts comments and trailing commas", "[gfx][isf][U01]")
{
  const std::string src = R"_(/*{
  // what this does
  "ISFVSN": "2.0",
  "DESCRIPTION": "see https://example.org/a,b // not a comment, ]",
  "INPUTS": [
    { "NAME": "gain", "TYPE": "float", "DEFAULT": 0.5, }, // trailing
    { "NAME": "on", "TYPE": "bool", },
  ],
}*/
void main() { gl_FragColor = vec4(gain); }
)_";
  ::isf::parser p{{}, src, 450, ::isf::parser::ShaderType::ISF};
  CHECK(p.data().description == "see https://example.org/a,b // not a comment, ]");
  REQUIRE(p.data().inputs.size() == 2);
  CHECK(p.data().inputs[1].name == "on");
}

TEST_CASE("a malformed ISF header still reports its position", "[gfx][isf][U01]")
{
  const std::string src = "/*{\n  // comment\n  \"ISFVSN\": \"2.0\",\n  \"INPUTS\": [ { \"NAME\": } ]\n}*/\nvoid main() { }\n";
  CHECK_THROWS_WITH(
      (::isf::parser{{}, src, 450, ::isf::parser::ShaderType::ISF}),
      Catch::Matchers::ContainsSubstring("at L4:"));
}

// A licence comment that itself starts with '{' (as "/* {c} 2024 ..." does)
// is not the header: the header is the first block comment whose content
// parses as a JSON object. The first '{' comment was taken, and its JSON
// error reported.
TEST_CASE("a licence comment starting with a brace is not the header", "[gfx][isf][U01]")
{
  const std::string src = R"_(/* {c} 2024 Some Author, MIT licence */
/*{ "ISFVSN": "2", "INPUTS": [ { "NAME": "gain", "TYPE": "float" } ] }*/
void main() { gl_FragColor = vec4(gain); }
)_";
  isf::parser p{{}, src, 450, isf::parser::ShaderType::ISF};
  REQUIRE(p.data().inputs.size() == 1);
  CHECK(p.data().inputs[0].name == "gain");
}

// The header's keys are case-independent (U02), so the autodetection reads
// them so too: a lower-case header next to a mainImage was taken for a
// Shadertoy, which ignores the header and its inputs.
TEST_CASE("autodetection reads a lower-case ISF header", "[gfx][isf][U02]")
{
  const std::string src = R"_(/*{ "isfvsn": "2", "inputs": [ { "name": "gain", "type": "float" } ] }*/
void mainImage(out vec4 fragColor, in vec2 fragCoord) { fragColor = vec4(gain); }
)_";
  isf::parser p{{}, src, 450, isf::parser::ShaderType::Autodetect};
  REQUIRE(p.data().inputs.size() >= 1);
  CHECK(p.data().inputs[0].name == "gain");
}
