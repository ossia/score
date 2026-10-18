// VERTEX_SHADER_ART header handling (shader board V01, V03):
//   * no / non-positive POINT_COUNT: the Vertex Count control defaults to 3, one
//     triangle, not 10000;
//   * PRIMITIVE_MODE is matched without regard to case; an unknown one stays
//     TRIANGLES (with a warning);
//   * the authored DESCRIPTION is kept, "Vertex Shader Art" is only the default.
#include <isf.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>

namespace
{
std::string vsa(const std::string& header)
{
  return "/*{ \"ISFVSN\": \"2\", \"MODE\": \"VERTEX_SHADER_ART\"" + header
         + ", \"INPUTS\": [] }*/\nvoid main() { gl_Position = vec4(0.0); v_color = "
           "vec4(1.0); }\n";
}

const isf::input* find(const isf::descriptor& d, const char* name)
{
  for(const auto& in : d.inputs)
    if(in.name == name)
      return &in;
  return nullptr;
}

double vertex_count_default(const std::string& header)
{
  isf::parser p{vsa(header), {}, 450, isf::parser::ShaderType::VertexShaderArt};
  const auto d = p.data();
  const auto* in = find(d, "vertexCount");
  REQUIRE(in);
  return ossia::get<isf::float_input>(in->data).def;
}

int64_t primitive_default(const std::string& header)
{
  isf::parser p{vsa(header), {}, 450, isf::parser::ShaderType::VertexShaderArt};
  const auto d = p.data();
  const auto* in = find(d, "primitiveType");
  REQUIRE(in);
  return ossia::get<isf::long_input>(in->data).def;
}
}

TEST_CASE("VSA vertex count defaults to 3 without POINT_COUNT", "[isf][vsa][V01]")
{
  CHECK(vertex_count_default("") == 3.0);
  CHECK(vertex_count_default(", \"POINT_COUNT\": 0") == 3.0);
  CHECK(vertex_count_default(", \"POINT_COUNT\": -5") == 3.0);
  CHECK(vertex_count_default(", \"POINT_COUNT\": 1234") == 1234.0);
}

TEST_CASE("VSA PRIMITIVE_MODE ignores case, unknown stays TRIANGLES", "[isf][vsa][V01]")
{
  // POINTS, LINE_STRIP, LINE_LOOP, LINES, TRI_STRIP, TRI_FAN, TRIANGLES
  CHECK(primitive_default(", \"PRIMITIVE_MODE\": \"LINE_LOOP\"") == 2);
  CHECK(primitive_default(", \"PRIMITIVE_MODE\": \"line_loop\"") == 2);
  CHECK(primitive_default(", \"PRIMITIVE_MODE\": \"Points\"") == 0);
  CHECK(primitive_default(", \"PRIMITIVE_MODE\": \"TRIANGLE_FAN\"") == 6);
  CHECK(primitive_default("") == 6);
}

TEST_CASE("VSA keeps the authored DESCRIPTION", "[isf][vsa][V03]")
{
  {
    isf::parser p{
        vsa(", \"DESCRIPTION\": \"Spiral of points\""), {}, 450,
        isf::parser::ShaderType::VertexShaderArt};
    CHECK(p.data().description == "Spiral of points");
  }
  {
    isf::parser p{vsa(""), {}, 450, isf::parser::ShaderType::VertexShaderArt};
    CHECK(p.data().description == "Vertex Shader Art");
  }
  {
    isf::parser p{
        vsa(", \"CATEGORIES\": [\"VSA\", \"spiral\"]"), {}, 450,
        isf::parser::ShaderType::VertexShaderArt};
    const auto cats = p.data().categories;
    CHECK(std::count(cats.begin(), cats.end(), "VSA") == 1);
  }
}
