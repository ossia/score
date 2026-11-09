// A CSF geometry attribute's DEFAULT is parsed into the words of its declared
// type: a scalar is broadcast to every component, a float keeps its float
// bits, an int its two's complement, a uint its value. A value the type
// cannot hold exactly, a count matching neither one nor the component count,
// a type without components and an attribute that is required or only written
// are refused with invalid_file. write_isf writes the DEFAULT back.
//
// Pure CPU: the parser only.
#include <isf.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <bit>
#include <cstdint>
#include <string>
#include <vector>

namespace
{
std::string csf(const std::string& attributes)
{
  return R"_(/*{
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "RESOURCES": [
    { "NAME": "geo", "TYPE": "geometry",
      "ATTRIBUTES": [ { "NAME": "position", "TYPE": "vec4", "ACCESS": "read_only" }, )_"
         + attributes + R"_( ] }
  ],
  "PASSES": [ { "LOCAL_SIZE": [1, 1, 1], "EXECUTION_MODEL": { "TYPE": "MANUAL", "WORKGROUPS": [1, 1, 1] } } ]
}*/
void main() { }
)_";
}

std::string optional(const std::string& type, const std::string& value)
{
  return R"_({ "NAME": "a", "TYPE": ")_" + type
         + R"_(", "ACCESS": "read_only", "REQUIRED": false, "DEFAULT": )_" + value + " }";
}

const isf::geometry_input::attribute_request& attribute(const isf::descriptor& d)
{
  return ossia::get<isf::geometry_input>(d.inputs.at(0).data).attributes.at(1);
}

std::vector<uint32_t> words(const std::string& type, const std::string& value)
{
  ::isf::parser p{{}, csf(optional(type, value)), 450, ::isf::parser::ShaderType::CSF};
  return attribute(p.data()).default_value;
}

std::string refusal(const std::string& attribute)
{
  try
  {
    ::isf::parser p{{}, csf(attribute), 450, ::isf::parser::ShaderType::CSF};
  }
  catch(const isf::invalid_file& e)
  {
    return e.what();
  }
  return "accepted";
}

uint32_t f(float v)
{
  return std::bit_cast<uint32_t>(v);
}
}

TEST_CASE("a DEFAULT is stored as the declared type's words", "[isf][csf][default]")
{
  CHECK(words("float", "1") == std::vector<uint32_t>{f(1.f)});
  CHECK(words("float", "-0.25") == std::vector<uint32_t>{f(-0.25f)});
  CHECK(words("vec3", "[0.25, 0.5, 0.75]") == std::vector<uint32_t>{f(.25f), f(.5f), f(.75f)});
  CHECK(words("vec4", "2") == std::vector<uint32_t>{f(2.f), f(2.f), f(2.f), f(2.f)});
  CHECK(words("int", "-3") == std::vector<uint32_t>{0xFFFFFFFDu});
  CHECK(words("ivec2", "[-1, 2147483647]") == std::vector<uint32_t>{0xFFFFFFFFu, 0x7FFFFFFFu});
  CHECK(words("uint", "4294967295") == std::vector<uint32_t>{0xFFFFFFFFu});
  CHECK(words("uvec2", "7") == std::vector<uint32_t>{7u, 7u});
  CHECK(words("uint", "3.0") == std::vector<uint32_t>{3u});
}

TEST_CASE("an attribute without DEFAULT has none", "[isf][csf][default]")
{
  ::isf::parser p{
      {},
      csf(R"_({ "NAME": "a", "TYPE": "float", "ACCESS": "read_only", "REQUIRED": false })_"),
      450, ::isf::parser::ShaderType::CSF};
  CHECK(attribute(p.data()).default_value.empty());
}

TEST_CASE("a DEFAULT the type cannot hold is refused", "[isf][csf][default]")
{
  using Catch::Matchers::ContainsSubstring;
  CHECK_THAT(refusal(optional("int", "1.5")), ContainsSubstring("is not an int"));
  CHECK_THAT(refusal(optional("int", "2147483648")), ContainsSubstring("is not an int"));
  CHECK_THAT(refusal(optional("uint", "-1")), ContainsSubstring("is not a uint"));
  CHECK_THAT(refusal(optional("uint", "4294967296")), ContainsSubstring("is not a uint"));
  CHECK_THAT(refusal(optional("float", "1e39")), ContainsSubstring("is not a float"));
  CHECK_THAT(refusal(optional("vec3", "[1, 2]")), ContainsSubstring("expected one number or 3"));
  CHECK_THAT(refusal(optional("vec2", "[]")), ContainsSubstring("expected one number or 2"));
  CHECK_THAT(refusal(optional("float", "\"one\"")), ContainsSubstring("expected a number"));
  CHECK_THAT(refusal(optional("float", "[true]")), ContainsSubstring("expected a number"));
  CHECK_THAT(refusal(optional("mat4", "1")), ContainsSubstring("scalars and vectors"));
  CHECK_THAT(refusal(optional("bool", "1")), ContainsSubstring("scalars and vectors"));
}

TEST_CASE("a DEFAULT is only for optional attributes the shader reads", "[isf][csf][default]")
{
  using Catch::Matchers::ContainsSubstring;
  CHECK_THAT(
      refusal(R"_({ "NAME": "a", "TYPE": "float", "ACCESS": "read_only", "DEFAULT": 1 })_"),
      ContainsSubstring("REQUIRED false"));
  CHECK_THAT(
      refusal(
          R"_({ "NAME": "a", "TYPE": "float", "ACCESS": "read_only", "REQUIRED": true, "DEFAULT": 1 })_"),
      ContainsSubstring("REQUIRED false"));
  CHECK_THAT(
      refusal(
          R"_({ "NAME": "a", "TYPE": "float", "ACCESS": "write_only", "REQUIRED": false, "DEFAULT": 1 })_"),
      ContainsSubstring("ACCESS read_only"));
  CHECK(
      refusal(
          R"_({ "NAME": "a", "TYPE": "float", "ACCESS": "gather", "REQUIRED": false, "DEFAULT": 1 })_")
      == "accepted");
  CHECK(
      refusal(
          R"_({ "NAME": "a", "TYPE": "vec2", "ACCESS": "read_write", "REQUIRED": false, "DEFAULT": [1, 2] })_")
      == "accepted");
}

TEST_CASE("write_isf writes a DEFAULT back", "[isf][csf][default]")
{
  ::isf::parser p{
      {},
      csf(optional("vec3", "[0.25, -2, 3]") + ", "
          + R"_({ "NAME": "b", "TYPE": "ivec2", "ACCESS": "read_only", "REQUIRED": false, "DEFAULT": -7 })_"),
      450, ::isf::parser::ShaderType::CSF};
  const std::string header = p.write_isf();
  INFO(header);
  using Catch::Matchers::ContainsSubstring;
  CHECK_THAT(header, ContainsSubstring(R"_("DEFAULT": [0.25, -2, 3])_"));
  CHECK_THAT(header, ContainsSubstring(R"_("DEFAULT": [-7, -7])_"));
}

TEST_CASE("a render pipeline refuses a DEFAULT", "[isf][default]")
{
  const std::string fs = R"_(/*{
  "ISFVSN": "2",
  "MODE": "RAW_RASTER_PIPELINE",
  "VERTEX_INPUTS": [ { "TYPE": "vec3", "NAME": "position" } ],
  "VERTEX_OUTPUTS": [],
  "FRAGMENT_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "color" } ],
  "INPUTS": [ { "NAME": "geo", "TYPE": "geometry", "ATTRIBUTES": [
    { "NAME": "w", "TYPE": "float", "ACCESS": "read_only", "REQUIRED": false, "DEFAULT": 1 } ] } ]
}*/
void main() { color = vec4(1.0); }
)_";
  const std::string vs = "void main() { isf_vertShaderInit(); gl_Position = vec4(position, 1.0); isf_vertShaderFinish(); }\n";
  std::string why = "accepted";
  try
  {
    ::isf::parser p{vs, fs, 450, ::isf::parser::ShaderType::RawRasterPipeline};
  }
  catch(const isf::invalid_file& e)
  {
    why = e.what();
  }
  CHECK_THAT(why, Catch::Matchers::ContainsSubstring("only supported in compute shaders"));
}
