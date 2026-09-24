#pragma once

#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/Utils.hpp>

#include <ossia/audio/fft.hpp>
#include <ossia/detail/small_flat_map.hpp>
#include <ossia/detail/variant.hpp>
#include <ossia/math/math_expression.hpp>

namespace score::gfx
{
struct PersistSampler
{
  QRhiSampler* sampler{};
  QRhiTexture* textures[2]{nullptr, nullptr};
};
using PassOutput = ossia::variant<PersistSampler, TextureRenderTarget>;

//! Advances a shader's FRAMEINDEX once per RenderList frame, however many
//! output edges update the node in it; a fresh node starts at 0. Returns
//! whether this is the node's first update of `frame`.
inline bool
advanceFrameIndex(int32_t& frameIndex, int64_t& lastFrame, int64_t frame) noexcept
{
  if(lastFrame == frame)
    return false;
  if(lastFrame >= 0 || frameIndex > 0)
    frameIndex++;
  lastFrame = frame;
  return true;
}

struct AudioTextureUpload
{
  explicit AudioTextureUpload();

  void
  process(AudioTexture& audio, QRhiResourceUpdateBatch& res, QRhiTexture* rhiTexture);

  void processTemporal(
      AudioTexture& audio, QRhiResourceUpdateBatch& res, QRhiTexture* rhiTexture);

  void processHistogram(
      AudioTexture& audio, QRhiResourceUpdateBatch& res, QRhiTexture* rhiTexture);

  void processSpectral(
      AudioTexture& audio, QRhiResourceUpdateBatch& res, QRhiTexture* rhiTexture);

  [[nodiscard]] std::optional<Sampler> updateAudioTexture(
      AudioTexture& audio, RenderList& renderer, char* materialData,
      QRhiResourceUpdateBatch& res);

private:
  std::vector<float> m_scratchpad;
  ossia::fft m_fft;
};

}
