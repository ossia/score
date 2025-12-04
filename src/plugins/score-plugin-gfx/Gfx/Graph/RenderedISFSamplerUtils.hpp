#pragma once
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/RenderedISFUtils.hpp>

#include <score/tools/Debug.hpp>

namespace score::gfx
{

inline std::vector<Sampler> initInputSamplers(
    const ProcessNode& node, RenderList& renderer, const std::vector<Port*>& ports,
    ossia::small_flat_map<const Port*, TextureRenderTarget, 2>& m_rts)
{
  std::vector<Sampler> samplers;
  QRhi& rhi = *renderer.state.rhi;

  int cur_port = 0;
  for(Port* in : ports)
  {
    switch(in->type)
    {
      case Types::Image: {
        auto spec = node.resolveRenderTargetSpecs(cur_port, renderer);
        auto sampler = rhi.newSampler(
            spec.mag_filter, spec.min_filter, spec.mipmap_mode, spec.address_u,
            spec.address_v, spec.address_w);
        sampler->setName("ISFNode::initInputSamplers::sampler");
        SCORE_ASSERT(sampler->create());

        auto rt = score::gfx::createRenderTarget(
            renderer.state, spec.format, spec.size, renderer.samples(),
            renderer.requiresDepth(*in));
        auto texture = rt.texture;
        samplers.push_back({sampler, texture});

        m_rts[in] = std::move(rt);
        break;
      }

      default:
        break;
    }
    cur_port++;
  }
  return samplers;
}

inline std::vector<Sampler>
initAudioTextures(RenderList& renderer, std::list<AudioTexture>& textures)
{
  std::vector<Sampler> samplers;
  QRhi& rhi = *renderer.state.rhi;
  for(auto& texture : textures)
  {
    auto sampler = rhi.newSampler(
        QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
        QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    sampler->setName("ISFNode::initAudioTextures::sampler");
    sampler->create();

    samplers.push_back({sampler, &renderer.emptyTexture()});
    texture.samplers[&renderer] = {sampler, nullptr};
  }
  return samplers;
}

}
