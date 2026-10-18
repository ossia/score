// GLSL that libisf generates for a valid shader header must compile. Each case
// is a header the parser accepts; the generated stages are baked for every
// backend through ShaderCache, the same path a node uses, without a device.
//
//   * RAW_RASTER_PIPELINE with CLIP_DISTANCES / CULL_DISTANCES: the generated
//     gl_PerVertex redeclaration has to keep gl_PointSize, which the wrapper
//     main() writes (shader board R48).
//   * COMPUTE_SHADER image with FORMAT bgra8: bgra8 is a texture format, not a
//     GLSL image format qualifier (C14); nor is a misspelt format.
//   * VERTEX_SHADER_ART with storage / uniform / geometry inputs: VSA has no
//     runtime path for them and must not declare them (V10).
#include <Gfx/Graph/ShaderCache.hpp>

#include <isf.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <rhi/qshader.h>

#include <QString>

#include <string>
#include <utility>
#include <vector>

namespace
{
struct backend
{
  score::gfx::GraphicsApi api;
  QShaderVersion version;
};

backend any_backend()
{
  return GENERATE(
      backend{score::gfx::OpenGL, QShaderVersion(450)},
      backend{score::gfx::Vulkan, QShaderVersion(100)},
      backend{score::gfx::D3D11, QShaderVersion(50)},
      backend{score::gfx::D3D12, QShaderVersion(50)},
      backend{score::gfx::Metal, QShaderVersion(12)});
}

void require_bakes(
    const backend& b, const std::string& text, QShader::Stage stage)
{
  REQUIRE(!text.empty());
  const auto& [shader, error] = score::gfx::ShaderCache::get(
      b.api, b.version, QByteArray::fromStdString(text), stage);
  INFO("api=" << int(b.api) << "\n" << error.toStdString() << "\n" << text);
  CHECK(error.isEmpty());
  CHECK(shader.isValid());
}

std::string raw_fragment(const std::string& extra)
{
  return R"_(/*{
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "VERTEX_INPUTS": [ { "TYPE": "vec4", "NAME": "position" } ],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  )_" + extra + R"_(
  "INPUTS": []
}*/
void main() { isf_FragColor = vec4(1.0); }
)_";
}
}

TEST_CASE(
    "raw raster CLIP_DISTANCES / CULL_DISTANCES vertex stage bakes",
    "[gfx][isf][raw][R48]")
{
  const auto b = any_backend();
  const auto extra = GENERATE(
      std::string{R"_("CLIP_DISTANCES": 2,)_"},
      std::string{R"_("CULL_DISTANCES": 1,)_"},
      std::string{R"_("CLIP_DISTANCES": 1, "CULL_DISTANCES": 1,)_"});
  CAPTURE(extra);
  // The wrapper main() also writes gl_PointSize on every non-HLSL target.
  const std::string body = extra.find("CLIP") != std::string::npos
                               ? "void main()\n{\n  gl_Position = vec4(position.xyz, 1.0);\n"
                                 "  gl_ClipDistance[0] = position.x;\n}\n"
                               : "void main()\n{\n  gl_Position = vec4(position.xyz, 1.0);\n"
                                 "  gl_CullDistance[0] = position.x;\n}\n";
  ::isf::parser p{
      body, raw_fragment(extra), 450, ::isf::parser::ShaderType::RawRasterPipeline};
  require_bakes(b, p.vertex(), QShader::VertexStage);
  require_bakes(b, p.fragment(), QShader::FragmentStage);
}

TEST_CASE(
    "CSF image with FORMAT bgra8 or an unknown format bakes", "[gfx][isf][csf][C14]")
{
  const auto b = any_backend();
  const auto format = GENERATE(std::string{"BGRA8"}, std::string{"rgba16x"});
  CAPTURE(format);
  const auto access = GENERATE(
      std::string{"write_only"}, std::string{"read_write"});
  CAPTURE(access);
  const std::string cs = R"_(/*{
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "RESOURCES": [
    { "NAME": "outputImage", "TYPE": "image", "ACCESS": ")_"
                         + access + R"_(", "FORMAT": ")_" + format + R"_(", "WIDTH": "16", "HEIGHT": "16" }
  ],
  "PASSES": [ { "LOCAL_SIZE": [8, 8, 1], "EXECUTION_MODEL": { "TYPE": "2D_IMAGE", "TARGET": "outputImage" } } ]
}*/
void main()
{
  ivec2 p = ivec2(gl_GlobalInvocationID.xy);
  IMG_STORE(outputImage, p, vec4(1.0, 0.0, 0.0, 1.0));
}
)_";
  ::isf::parser p{{}, cs, 450, ::isf::parser::ShaderType::CSF};
  // RenderedCSFNode substitutes each pass's LOCAL_SIZE before compiling.
  auto src = QString::fromStdString(p.compute_shader());
  src.replace("ISF_LOCAL_SIZE_X", "8");
  src.replace("ISF_LOCAL_SIZE_Y", "8");
  src.replace("ISF_LOCAL_SIZE_Z", "1");
  require_bakes(b, src.toStdString(), QShader::ComputeStage);
}

TEST_CASE(
    "VSA with storage / uniform / geometry inputs bakes", "[gfx][isf][vsa][V10]")
{
  const auto b = any_backend();
  const auto input = GENERATE(
      std::string{
          R"_({ "NAME": "buf", "TYPE": "storage", "ACCESS": "read_only", "LAYOUT": [ { "NAME": "v", "TYPE": "float[]" } ] })_"},
      std::string{
          R"_({ "NAME": "ubo", "TYPE": "uniform", "LAYOUT": [ { "NAME": "k", "TYPE": "float" } ] })_"},
      std::string{
          R"_({ "NAME": "geo", "TYPE": "geometry", "ATTRIBUTES": [ { "NAME": "position", "SEMANTIC": "position", "TYPE": "vec4", "ACCESS": "read_only" } ] })_"});
  CAPTURE(input);
  const std::string vs = R"_(/*{
  "ISFVSN": "2",
  "MODE": "VERTEX_SHADER_ART",
  "POINT_COUNT": 3,
  "PRIMITIVE_MODE": "TRIANGLES",
  "INPUTS": [ )_" + input + R"_( ]
}*/
void main() {
  gl_Position = vec4(vertexId - 1.0, 0.0, 0.0, 1.0);
  v_color = vec4(1.0);
}
)_";
  ::isf::parser p{vs, {}, 450, ::isf::parser::ShaderType::VertexShaderArt};
  require_bakes(b, p.vertex(), QShader::VertexStage);
  require_bakes(b, p.fragment(), QShader::FragmentStage);
}

// A GLSL image format QRhi cannot allocate (snorm, rg16f / rg32f, 8 / 16-bit
// integers, r11f_g11f_b10f) was emitted as is and allocated as RGBA8: the shader
// and the texture disagreed (shader board C16). Such a FORMAT falls back to
// rgba8 in the declaration too, with a warning, like an unknown one.
TEST_CASE("a CSF image FORMAT QRhi cannot allocate is declared rgba8", "[gfx][isf][csf][C16]")
{
  const auto format = GENERATE(
      std::string{"rgba8_snorm"}, std::string{"rg16f"}, std::string{"rg32f"},
      std::string{"r16ui"}, std::string{"rgba16"}, std::string{"r11f_g11f_b10f"});
  CAPTURE(format);
  const std::string cs = R"_(/*{
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "RESOURCES": [
    { "NAME": "outputImage", "TYPE": "image", "ACCESS": "write_only", "FORMAT": ")_"
                         + format + R"_(", "WIDTH": "16", "HEIGHT": "16" }
  ],
  "PASSES": [ { "LOCAL_SIZE": [8, 8, 1], "EXECUTION_MODEL": { "TYPE": "2D_IMAGE", "TARGET": "outputImage" } } ]
}*/
void main() { }
)_";
  ::isf::parser p{{}, cs, 450, ::isf::parser::ShaderType::CSF};
  const auto src = p.compute_shader();
  INFO(src);
  CHECK(src.find(format) == std::string::npos);
  CHECK(src.find(", rgba8)") != std::string::npos);
}
