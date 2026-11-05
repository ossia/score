// How libisf wraps an ISF-mode vertex shader, read from the code it generates.
//
// * The wrapping is decided on the code the compiler reads: a comment that
//   mentions isf_vertShaderInit() is no call, so it neither pulls in the quad
//   prelude and the clip-space entry point nor points IMG_THIS_PIXEL at an
//   isf_FragNormCoord nothing writes. A real call is found with blanks before
//   its parenthesis too.
// * A shader that multiplies by clipSpaceCorrMatrix itself gets only the
//   framebuffer Y flip after its main, not the matrix a second time, and a
//   warning.
// * A shader that writes isf_FragNormCoord after init gets the position init
//   wrote in a varying of its own, which IMG_THIS_PIXEL reads, at a location
//   none of its own varyings use: past the explicit ones and past those the
//   shader compiler numbers itself for varyings without a layout. One that
//   only reads isf_FragNormCoord (the library's Edges, Bloom...) gets none.
//
// The generated stages are also baked for every backend, so the wrapping is
// known to compile.
#include "IsfTestCommon.hpp"

#include <Gfx/Graph/ShaderCache.hpp>
#include <Gfx/ShaderProgram.hpp>
#include <Gfx/Settings/Model.hpp>

#include <isf.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <string>

namespace
{
const std::string kFragment = R"_(/*{
  "ISFVSN": "2.0",
  "INPUTS": [ { "NAME": "inputImage", "TYPE": "image" } ]
}*/
layout(location = 3) in vec2 user_varying;
void main()
{
  gl_FragColor = IMG_THIS_PIXEL(inputImage) + vec4(user_varying, 0.0, 0.0);
}
)_";

struct Generated
{
  std::string vertex, fragment;
  std::vector<std::string> warnings;
};

// The stages as the engine bakes them: generated, then given the locations of
// the varyings declared without a layout (Gfx::updateToGlsl45).
Generated generate(const std::string& vertex, const std::string& fragment = kFragment)
{
  ::isf::parser p{vertex, fragment, 450, ::isf::parser::ShaderType::ISF};
  Gfx::ShaderSource src{
      ::isf::parser::ShaderType::ISF, QString::fromStdString(p.vertex()),
      QString::fromStdString(p.fragment())};
  Gfx::updateToGlsl45(src);
  return {src.vertex.toStdString(), src.fragment.toStdString(), p.data().warnings};
}

bool contains(const std::string& s, const char* what)
{
  return s.find(what) != std::string::npos;
}

bool warns_about_clip_space(const Generated& g)
{
  for(const auto& w : g.warnings)
    if(contains(w, "clipSpaceCorrMatrix"))
      return true;
  return false;
}

// Bakes one stage for every backend, as the engine does; the error of the
// first that fails.
std::string bake(const std::string& src, QShader::Stage stage)
{
  for(auto api : {score::gfx::Vulkan, score::gfx::OpenGL, score::gfx::Metal,
                  score::gfx::D3D11, score::gfx::D3D12})
  {
    const auto version = api == score::gfx::OpenGL
                             ? QShaderVersion(450)
                             : Gfx::Settings::shaderVersionForAPI(api);
    const auto& [shader, error] = score::gfx::ShaderCache::get(
        api, version, QByteArray::fromStdString(src), stage);
    if(!error.isEmpty() || !shader.isValid())
      return std::string{score::test::gfx::backend_name(api)} + ": "
             + error.toStdString();
  }
  return {};
}

const std::string kVertexTail = R"_(
layout(location = 3) out vec2 user_varying;
)_";
}

TEST_CASE("a commented isf_vertShaderInit() is not a call", "[gfx][isf][vertex]")
{
  // Declares what the prelude would, as a shader with its own vertex stage
  // must, and writes clip space itself.
  const Generated g = generate(R"_(
layout(location = 0) in vec2 position;
layout(location = 0) out vec2 isf_FragNormCoord;
layout(location = 3) out vec2 user_varying;
// No isf_vertShaderInit() here: this shader writes its own clip space.
/* isf_vertShaderInit(); was dropped */
void main()
{
  isf_FragNormCoord = position * 0.5 + 0.5;
  user_varying = vec2(0.0);
  gl_Position = clipSpaceCorrMatrix * vec4(position, 0.0, 1.0);
}
)_");
  CAPTURE(g.vertex, g.fragment);
  CHECK_FALSE(contains(g.vertex, "isf_user_vertex_main"));
  CHECK_FALSE(contains(g.fragment, "#define ISF_THIS_PIXEL_FROM_NORM_COORD"));
  CHECK_FALSE(contains(g.fragment, "#define ISF_THIS_PIXEL_VARYING"));
  CHECK_FALSE(warns_about_clip_space(g));
}

TEST_CASE("an isf_vertShaderInit call is wrapped", "[gfx][isf][vertex]")
{
  const Generated g = generate(kVertexTail + R"_(
void main()
{
  isf_vertShaderInit ();
  user_varying = vec2(0.0);
  gl_Position.xy *= 0.5;
}
)_");
  CAPTURE(g.vertex, g.fragment);
  CHECK(contains(g.vertex, "#define main isf_user_vertex_main"));
  CHECK(contains(g.vertex, "isf_user_vertex_main();\n  isf_vertShaderApplyClipSpace();"));
  CHECK(contains(g.fragment, "#define ISF_THIS_PIXEL_FROM_NORM_COORD"));
  CHECK_FALSE(warns_about_clip_space(g));
  CHECK(bake(g.vertex, QShader::VertexStage) == std::string{});
  CHECK(bake(g.fragment, QShader::FragmentStage) == std::string{});
}

TEST_CASE(
    "a shader multiplying by clipSpaceCorrMatrix gets it once", "[gfx][isf][vertex]")
{
  const Generated g = generate(kVertexTail + R"_(
void main()
{
  isf_vertShaderInit();
  user_varying = vec2(0.0);
  gl_Position = clipSpaceCorrMatrix * gl_Position;
  isf_vertShaderFinish();
}
)_");
  CAPTURE(g.vertex);
  CHECK(contains(g.vertex, "isf_user_vertex_main();\n  isf_vertShaderApplyFramebufferY();"));
  CHECK_FALSE(contains(g.vertex, "isf_user_vertex_main();\n  isf_vertShaderApplyClipSpace();"));
  CHECK(warns_about_clip_space(g));
  CHECK(bake(g.vertex, QShader::VertexStage) == std::string{});
}

TEST_CASE(
    "IMG_THIS_PIXEL reads init's position when the shader moves isf_FragNormCoord",
    "[gfx][isf][vertex]")
{
  const Generated g = generate(kVertexTail + R"_(
void main()
{
  isf_vertShaderInit();
  user_varying = vec2(0.0);
  isf_FragNormCoord = vec2(0.0);
}
)_");
  CAPTURE(g.vertex, g.fragment);
  // Past the shader's own location 3.
  CHECK(contains(g.vertex, "layout(location = 4) out vec2 isf_ThisPixelNormCoord;"));
  CHECK(contains(g.fragment, "layout(location = 4) in vec2 isf_ThisPixelNormCoord;"));
  CHECK(contains(g.fragment, "#define ISF_THIS_PIXEL_VARYING"));
  CHECK_FALSE(contains(g.fragment, "#define ISF_THIS_PIXEL_FROM_NORM_COORD"));
  CHECK(bake(g.vertex, QShader::VertexStage) == std::string{});
  CHECK(bake(g.fragment, QShader::FragmentStage) == std::string{});
}

namespace
{
// The library's Edges filter: varyings without a layout, numbered by the
// shader compiler, and isf_FragNormCoord only read.
const std::string kUnqualifiedFragment = R"_(/*{
  "ISFVSN": "2.0",
  "INPUTS": [ { "NAME": "inputImage", "TYPE": "image" } ]
}*/
in vec2 left_coord;
in vec2 right_coord;
in vec2 above_coord;
in vec2 below_coord;
in vec2 taps[3];
void main()
{
  gl_FragColor = IMG_THIS_PIXEL(inputImage) + IMG_NORM_PIXEL(inputImage, left_coord)
                 + IMG_NORM_PIXEL(inputImage, right_coord + above_coord + below_coord)
                 + vec4(taps[0] + taps[1] + taps[2], 0.0, 0.0);
}
)_";

const std::string kUnqualifiedVertexDecls = R"_(
out vec2 left_coord;
out vec2 right_coord;
out vec2 above_coord;
out vec2 below_coord;
out vec2 taps[3];
void set_coords()
{
  vec2 texc = isf_FragNormCoord;
  left_coord = texc; right_coord = texc; above_coord = texc; below_coord = texc;
  taps[0] = texc; taps[1] = texc; taps[2] = texc;
}
)_";
}

TEST_CASE(
    "a shader reading isf_FragNormCoord beside unqualified varyings gets no "
    "extra varying",
    "[gfx][isf][vertex]")
{
  const Generated g = generate(kUnqualifiedVertexDecls + R"_(
void main()
{
  isf_vertShaderInit();
  set_coords();
  isf_vertShaderFinish();
}
)_", kUnqualifiedFragment);
  CAPTURE(g.vertex, g.fragment);
  CHECK_FALSE(contains(g.vertex, "out vec2 isf_ThisPixelNormCoord"));
  CHECK(contains(g.fragment, "#define ISF_THIS_PIXEL_FROM_NORM_COORD"));
  CHECK(bake(g.vertex, QShader::VertexStage) == std::string{});
  CHECK(bake(g.fragment, QShader::FragmentStage) == std::string{});
}

TEST_CASE(
    "the captured position does not overlap varyings the compiler numbers",
    "[gfx][isf][vertex]")
{
  const Generated g = generate(kUnqualifiedVertexDecls + R"_(
void main()
{
  isf_vertShaderInit();
  set_coords();
  isf_FragNormCoord.y = 1.0 - isf_FragNormCoord.y;
  isf_vertShaderFinish();
}
)_", kUnqualifiedFragment);
  CAPTURE(g.vertex, g.fragment);
  // Location 0, then seven locations without a layout, numbered from 1.
  CHECK(contains(g.vertex, "layout(location = 8) out vec2 isf_ThisPixelNormCoord;"));
  CHECK(contains(g.fragment, "layout(location = 8) in vec2 isf_ThisPixelNormCoord;"));
  CHECK(bake(g.vertex, QShader::VertexStage) == std::string{});
  CHECK(bake(g.fragment, QShader::FragmentStage) == std::string{});
}
