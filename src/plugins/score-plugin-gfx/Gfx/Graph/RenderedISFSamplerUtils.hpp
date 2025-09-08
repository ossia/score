#pragma once
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/RenderedISFUtils.hpp>

#include <score/tools/Debug.hpp>

namespace score::gfx
{

inline std::pair<std::vector<Sampler>, int> initInputSamplers(
    const ProcessNode& node, RenderList& renderer, const std::vector<Port*>& ports,
    ossia::small_flat_map<const Port*, TextureRenderTarget, 2>& m_rts,
    char* materialData)
{
  std::vector<Sampler> samplers;
  QRhi& rhi = *renderer.state.rhi;
  int cur_pos = 0;

  int cur_port = 0;
  for(Port* in : ports)
  {
    switch(in->type)
    {
      case Types::Empty:
        break;
      case Types::Int:
      case Types::Float:
        cur_pos += 4;
        break;
      case Types::Vec2:
        cur_pos += 8;
        if(cur_pos % 8 != 0)
          cur_pos += 4;
        break;
      case Types::Vec3:
        while(cur_pos % 16 != 0)
        {
          cur_pos += 4;
        }
        cur_pos += 12;
        break;
      case Types::Vec4:
        while(cur_pos % 16 != 0)
        {
          cur_pos += 4;
        }
        cur_pos += 16;
        break;
      case Types::Image: {
        auto spec = node.resolveRenderTargetSpecs(cur_port, renderer);
        auto sampler = rhi.newSampler(
            spec.mag_filter, spec.min_filter, spec.mipmap_mode, spec.address_u,
            spec.address_v, spec.address_w);
        sampler->setName("ISFNode::initInputSamplers::sampler");
        SCORE_ASSERT(sampler->create());

        auto rt = score::gfx::createRenderTarget(
            renderer.state, spec.format, spec.size, renderer.samples(),
            renderer.requiresDepth());
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
  return {samplers, cur_pos};
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
