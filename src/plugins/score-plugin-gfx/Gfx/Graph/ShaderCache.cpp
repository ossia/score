#include "ShaderCache.hpp"

#include <Gfx/Graph/RenderState.hpp>

#include <ossia/detail/algorithms.hpp>
#include <ossia/detail/mutex.hpp>

namespace score::gfx
{

const std::pair<QShader, QString>& ShaderCache::get(
    GraphicsApi api, const QShaderVersion& version, const QByteArray& shader,
    QShader::Stage stage, int multiViewCount)
{
  static std::mutex mut;
  static ShaderCache self TS_GUARDED_BY(mut);

  std::lock_guard<std::mutex> m{mut};

  // The view count is part of the baker's identity, not just of one bake:
  // it is baked into the GLSL (`layout(num_views = N) in;`) and into the
  // SPIR-V, so the same source at two view counts is two different shaders.
  auto ver_it = ossia::find_if(self.m_bakers, [&](const auto& p) {
    return p->api == api && p->version == version
           && p->multiViewCount == multiViewCount;
  });
  Baker* bb{};
  if(ver_it == self.m_bakers.end())
  {
    self.m_bakers.push_back(std::make_unique<Baker>(api, version, multiViewCount));
    bb = self.m_bakers.back().get();
  }
  else
  {
    bb = ver_it->get();
  }

  Baker& b = *bb;
  if(auto it = b.shaders.find(shader); it != b.shaders.end())
    return it->second;

  b.baker.setSourceString(shader, stage);
  b.baker.setPerTargetCompilation(true);

  // FIXME serialize / deserialize
  QShader baked = b.baker.bake();
  auto res = b.shaders.insert({shader, {std::move(baked), b.baker.errorMessage()}});
  return res.first->second;
}

const std::pair<QShader, QString>& ShaderCache::get(
    const RenderState& v, const QByteArray& shader, QShader::Stage stage,
    int multiViewCount)
{
  return ShaderCache::get(v.api, v.version, shader, stage, multiViewCount);
}

ShaderCache::ShaderCache() { }

ShaderCache::Baker::Baker(
    GraphicsApi api, const QShaderVersion& version, int multiViewCount)
    : api{api}
    , version{version}
    , multiViewCount{multiViewCount}
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
    case GraphicsApi::D3D12:
      baker.setGeneratedShaders({{QShader::HlslShader, version}});
      break;
    case GraphicsApi::Metal:
      baker.setGeneratedShaders({{QShader::MslShader, version}});
      break;
  }
  baker.setGeneratedShaderVariants({{}});

  // Mandatory for any shader using gl_ViewIndex: without it the GLSL target
  // refuses to translate (`ovr_multiview_view_count must be non-zero when
  // using GL_OVR_multiview2`) and the SPIR-V target has no num_views to
  // emit. QShaderBaker gained this in 6.7, the same release as the QRhi
  // multiview API.
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
  if(multiViewCount >= 2)
    baker.setMultiViewCount(multiViewCount);
#endif
}
}
