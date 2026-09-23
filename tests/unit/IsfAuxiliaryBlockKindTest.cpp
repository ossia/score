// UNIT — an AUXILIARY block must be generated from the *shader's* declared
// kind, never from whatever the upstream producer happens to publish.
//
// This is the prerequisite G4 identified and that sank the first attempt at it.
// A `uniform` companion buffer was handed to a consumer whose block the engine
// had generated as `readonly buffer` (because the producer published a storage
// buffer), so a UBO was bound to a storage block. It read correctly on OpenGL
// and regressed sponza-debug-dynamiclights on Vulkan from 0.243/0.396 to
// 0.008/0.001, and was reverted in 7548838841 with the note that generation
// must follow the declaration before any retry.
//
// Generation does follow the declaration — `ar.is_uniform = (kind ==
// aux_kind::Ubo)` in the parser, consumed by emit_aux_block — but nothing
// pinned it, so a refactor could quietly reintroduce the mismatch and the only
// symptom would be a backend-specific misread. This test pins it.
//
// Pure CPU: parse, generate, inspect the GLSL. No QRhi, no display.
//
// Registration: tests/CMakeLists.txt, alongside IsfUniformInputUsageTest.
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

  // The block must be a UBO. Binding a uniform buffer to a storage-declared
  // block is the mismatch that regressed Vulkan in 7184cbcd77.
  CHECK(frag.find("uniform scene_counts_buf") != std::string::npos);
  CHECK(frag.find("std140") != std::string::npos);

  // And must NOT be a storage block.
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
