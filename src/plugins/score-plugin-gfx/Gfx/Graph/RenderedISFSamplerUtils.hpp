#pragma once
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/RenderedISFUtils.hpp>

#include <score/tools/Debug.hpp>

namespace score::gfx
{

namespace detail
{
inline QRhiSampler::Filter parseAudioFilter(const std::string& s)
{
  if(s.empty()) return QRhiSampler::Linear;
  std::string v = s;
  for(auto& c : v) c = (char)tolower(c);
  if(v == "nearest") return QRhiSampler::Nearest;
  return QRhiSampler::Linear;
}
inline QRhiSampler::AddressMode parseAudioWrap(const std::string& s)
{
  if(s.empty()) return QRhiSampler::ClampToEdge;
  std::string v = s;
  for(auto& c : v) c = (char)tolower(c);
  for(auto& c : v) if(c == '-') c = '_';
  if(v == "repeat")                           return QRhiSampler::Repeat;
  if(v == "mirror" || v == "mirrored_repeat") return QRhiSampler::Mirror;
  return QRhiSampler::ClampToEdge;
}
}

inline std::vector<Sampler>
initAudioTextures(RenderList& renderer, std::list<AudioTexture>& textures)
{
  std::vector<Sampler> samplers;
  QRhi& rhi = *renderer.state.rhi;
  for(auto& texture : textures)
  {
    const auto filter = detail::parseAudioFilter(texture.filter);
    const auto wrap = detail::parseAudioWrap(texture.wrap);
    auto sampler = rhi.newSampler(
        filter, filter, QRhiSampler::None, wrap, wrap);
    sampler->setName("ISFNode::initAudioTextures::sampler");
    sampler->create();

    samplers.push_back({sampler, nullptr});
    texture.samplers[&renderer] = {sampler, nullptr};
  }
  return samplers;
}

}
