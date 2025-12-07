#pragma once
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/RenderedISFUtils.hpp>

#include <score/tools/Debug.hpp>

namespace score::gfx
{

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
