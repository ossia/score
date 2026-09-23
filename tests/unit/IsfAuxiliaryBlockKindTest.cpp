// An AUXILIARY block must be generated from the shader's declared kind, never
// from what the upstream producer publishes: a UBO bound to a block generated
// as `readonly buffer` reads correctly on OpenGL but not on Vulkan. The parser
// sets `ar.is_uniform = (kind == aux_kind::Ubo)`, consumed by emit_aux_block.
//
// Pure CPU: parse, generate, inspect the GLSL. No QRhi, no display.
#include <isf.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>

namespace
{
std::string fragmentFor(const char* auxType)
{
  const std::string header = std::string(R"_(/*{
  "CREDIT": "test",
  "ISFVSN": "2",
  "MODE": "RAW_RASTER_PIPELINE",
  "DESCRIPTION": "auxiliary block kind",
  "CATEGORIES": ["test"],
  "VERTEX_INPUTS": [ { "TYPE": "vec3", "NAME": "position" } ],
  "VERTEX_OUTPUTS": [],
  "FRAGMENT_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "INPUTS": [],
  "AUXILIARY": [
    {
      "NAME": "scene_counts",
      "TYPE": ")_")
      + auxType + R"_(",
      "ACCESS": "read_only",
      "LAYOUT": [ { "NAME": "light_count", "TYPE": "uint" } ]
    }
  ]
}*/

void main()
{
    isf_FragColor = vec4(float(scene_counts.light_count), 0.0, 0.0, 1.0);
}
)_";
  return header;
}

const char* k_vert = R"_(
void main()
{
    isf_vertShaderInit();
    gl_Position = vec4(position, 1.0);
    isf_vertShaderFinish();
}
)_";

std::string generate(const char* auxType)
{
  isf::parser p{
      std::string(k_vert), fragmentFor(auxType), 450, isf::parser::ShaderType::RawRasterPipeline};
  return p.fragment();
}
}

TEST_CASE("a uniform AUXILIARY generates a std140 uniform block", "[isf]")
{
  const auto frag = generate("uniform");
  INFO(frag);

  // The block must be a UBO, not a storage-declared block.
  CHECK(frag.find("uniform scene_counts_buf") != std::string::npos);
  CHECK(frag.find("std140") != std::string::npos);

  CHECK(frag.find("buffer scene_counts_buf") == std::string::npos);
}

TEST_CASE("a storage AUXILIARY generates a std430 storage block", "[isf]")
{
  const auto frag = generate("storage");
  INFO(frag);

  CHECK(frag.find("buffer scene_counts_buf") != std::string::npos);
  CHECK(frag.find("std430") != std::string::npos);

  // The converse: a storage declaration must not become a UBO either, or the
  // runtime would bind an SSBO to a uniform block.
  CHECK(frag.find("uniform scene_counts_buf") == std::string::npos);
}
