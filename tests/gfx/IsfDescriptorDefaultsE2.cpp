// Pins the VSA BACKGROUND_COLOR default of isf::descriptor: transparent black,
// whatever the storage held before. The array was left uninitialised by
// `descriptor d;` in the parser, so a VSA without BACKGROUND_COLOR drew a
// garbage background over its target whenever the garbage alpha was positive
// (test_gfx_alpha_a4's VSA case on OpenGL).
#include <isf.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <memory>
#include <new>

TEST_CASE("isf::descriptor background_color defaults to transparent", "[isf][vsa]")
{
  alignas(isf::descriptor) unsigned char storage[sizeof(isf::descriptor)];
  std::memset(storage, 0x3f, sizeof(storage));
  auto* d = new(storage) isf::descriptor;
  for(double c : d->background_color)
    CHECK(c == 0.);
  std::destroy_at(d);
}

TEST_CASE("a parsed VSA without BACKGROUND_COLOR has a transparent background", "[isf][vsa]")
{
  const std::string src
      = "/*{\n  \"ISFVSN\": \"2\",\n  \"MODE\": \"VERTEX_SHADER_ART\",\n"
        "  \"POINT_COUNT\": 3,\n  \"INPUTS\": []\n}*/\n"
        "void main() { gl_Position = vec4(0.0); v_color = vec4(1.0); }\n";
  const auto d
      = isf::parser{src, {}, 450, isf::parser::ShaderType::VertexShaderArt}.data();
  CHECK(d.background_color[3] == 0.);
}
