#include "ShaderCache.hpp"
#include <Gfx/Graph/RenderState.hpp>
#include <ossia/detail/algorithms.hpp>

namespace score::gfx
{

const std::pair<QShader, QString>& ShaderCache::get(
    GraphicsApi api, const QShaderVersion& version, const QByteArray& shader, QShader::Stage stage)
{
  static std::mutex mut;
  static ShaderCache self;

  std::lock_guard<std::mutex> m{mut};

  auto ver_it = ossia::find_if(self.m_bakers, [&] (const auto& p) {
    return p->api == api && p->version == version;
  });
  Baker* bb{};
  if (ver_it == self.m_bakers.end())
  {
    self.m_bakers.push_back(std::make_unique<Baker>(api, version));
    bb = self.m_bakers.back().get();
  }
  else
  {
    bb = ver_it->get();
  }

  Baker& b = *bb;
  if (auto it = b.shaders.find(shader); it != b.shaders.end())
    return it->second;

  b.baker.setSourceString(shader, stage);
  auto res = b.shaders.insert(
               {shader, {b.baker.bake(), b.baker.errorMessage()}});
  return res.first->second;
}

const std::pair<QShader, QString>& ShaderCache::get(
    const RenderState& v, const QByteArray& shader, QShader::Stage stage)
{
  return ShaderCache::get(v.api, v.version, shader, stage);
}

ShaderCache::ShaderCache()
{
}

ShaderCache::Baker::Baker(GraphicsApi api, const QShaderVersion& version)
  : api{api}
  , version{version}
{
  switch(api)
  {
    case GraphicsApi::Null:
      baker.setGeneratedShaders({{QShader::SpirvShader, version}});
      break;
    case GraphicsApi::OpenGL:
      baker.setGeneratedShaders({{QShader::GlslShader, version}});
      break;
    case GraphicsApi::Vulkan:
      baker.setGeneratedShaders({{QShader::SpirvShader, version}});
      break;
    case GraphicsApi::D3D11:
      baker.setGeneratedShaders({{QShader::HlslShader, version}});
      break;
    case GraphicsApi::Metal:
      baker.setGeneratedShaders({{QShader::MslShader, version}});
      break;
  }
  baker.setGeneratedShaderVariants({ {} });
}

/*
    QShaderVersion::Flags glFlag = m_caps.type == QSurfaceFormat::OpenGLES
            ? QShaderVersion::GlslEs
            : QShaderVersion::Flag{};
    baker.setGeneratedShaders({
                                  {QShader::SpirvShader, 100},
                                  {QShader::GlslShader, QShaderVersion(m_caps.shaderVersion, glFlag)},
                              #if defined(_WIN32)
                                  {QShader::HlslShader, QShaderVersion(50)},
                              #endif
                              #if defined(__APPLE__)
                                  {QShader::MslShader, QShaderVersion(12)},
                                  {QShader::GlslShader, QShaderVersion(120, QShaderVersion::Flag{})} // For syphon
                              #endif
                              });

    baker.setGeneratedShaderVariants({
                                         QShader::Variant{}, QShader::Variant{},
                                     #if defined(_WIN32)
                                         QShader::Variant{},
                                     #endif
                                     #if defined(__APPLE__)
                                         QShader::Variant{},
                                         QShader::Variant{}
                                     #endif
                                     });
}
*/
}
