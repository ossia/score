// EXTENSIONS entries were emitted verbatim as `#extension <entry> : require`:
// a blank or malformed entry reached the compiler, and an extension the engine
// already requires (or listed twice) was required twice (shader board B14).
// Entries are validated, duplicates dropped, and each stage requires an
// extension once.
#include <isf.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <string>

namespace
{
std::string raw(const std::string& extensions)
{
  return R"_(/*{
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "EXTENSIONS": )_" + extensions + R"_(,
  "VERTEX_INPUTS": [ { "TYPE": "vec4", "NAME": "position" } ],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "INPUTS": []
}*/
void main() { isf_FragColor = vec4(1.0); }
)_";
}

int count(const std::string& text, const std::string& what)
{
  int n = 0;
  for(auto p = text.find(what); p != std::string::npos; p = text.find(what, p + 1))
    n++;
  return n;
}

const std::string vs = "void main() { gl_Position = vec4(position.xyz, 1.0); }\n";
}

TEST_CASE("EXTENSIONS are required once per stage", "[gfx][isf][B14]")
{
  ::isf::parser p{
      vs,
      raw(R"_([ "GL_ARB_shader_draw_parameters", "GL_EXT_samplerless_texture_functions", "GL_EXT_samplerless_texture_functions" ])_"),
      450, ::isf::parser::ShaderType::RawRasterPipeline};
  CHECK(count(p.vertex(), "#extension GL_ARB_shader_draw_parameters") == 1);
  CHECK(count(p.vertex(), "#extension GL_EXT_samplerless_texture_functions") == 1);
  CHECK(count(p.fragment(), "#extension GL_EXT_samplerless_texture_functions") == 1);
  const auto& exts = p.data().extensions;
  CHECK(std::count(exts.begin(), exts.end(), "GL_EXT_samplerless_texture_functions") == 1);
}

TEST_CASE("a malformed EXTENSIONS entry is rejected", "[gfx][isf][B14]")
{
  const auto list = GENERATE(
      std::string{R"_([ "" ])_"}, std::string{R"_([ "GL_EXT_a : enable" ])_"},
      std::string{R"_([ 3 ])_"}, std::string{R"_("GL_EXT_samplerless_texture_functions")_"},
      std::string{R"_([ "GL_EXT_ok", "2bad" ])_"});
  CAPTURE(list);
  CHECK_THROWS_WITH(
      (::isf::parser{vs, raw(list), 450, ::isf::parser::ShaderType::RawRasterPipeline}),
      Catch::Matchers::ContainsSubstring("EXTENSIONS"));
}
