#include <Gfx/Graph/ShaderCache.hpp>
#include <Gfx/Settings/Model.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{
const QByteArray kVertex = R"_(#version 450
layout(location = 0) in vec3 position;
void main() { gl_Position = vec4(position, 1.0); }
)_";

const QByteArray kMultiviewVertex = R"_(#version 450
#extension GL_EXT_multiview : require
layout(location = 0) in vec3 position;
void main() { gl_Position = vec4(position + vec3(float(gl_ViewIndex)), 1.0); }
)_";
}

TEST_CASE("a single-view shader is baked once whatever the renderer's multiview", "[gfx][shadercache]")
{
  using score::gfx::ShaderCache;
  for(auto api : {score::gfx::Vulkan, score::gfx::OpenGL, score::gfx::D3D11})
  {
    const auto version = api == score::gfx::OpenGL ? QShaderVersion(450)
                                                   : Gfx::Settings::shaderVersionForAPI(api);
    for(int views : {0, 1})
    {
      const auto* withMultiview
          = &ShaderCache::get(api, version, kVertex, QShader::VertexStage, views, true);
      const auto* without
          = &ShaderCache::get(api, version, kVertex, QShader::VertexStage, views, false);
      CHECK(withMultiview == without);
    }
  }
}

TEST_CASE("a multiview shader is baked apart for renderers with and without multiview", "[gfx][shadercache]")
{
  using score::gfx::ShaderCache;
  const auto version = Gfx::Settings::shaderVersionForAPI(score::gfx::Vulkan);
  const auto* native = &ShaderCache::get(
      score::gfx::Vulkan, version, kMultiviewVertex, QShader::VertexStage, 6, true);
  const auto* lowered = &ShaderCache::get(
      score::gfx::Vulkan, version, kMultiviewVertex, QShader::VertexStage, 6, false);
  CHECK(native != lowered);
  CHECK(native == &ShaderCache::get(
            score::gfx::Vulkan, version, kMultiviewVertex, QShader::VertexStage, 6, true));
}
