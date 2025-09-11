#pragma once

#include <Gfx/Graph/Utils.hpp>

#include <ossia/audio/fft.hpp>
#include <ossia/detail/small_flat_map.hpp>
#include <ossia/detail/variant.hpp>
#include <ossia/math/math_expression.hpp>

namespace score::gfx
{
struct Pass
{
  TextureRenderTarget renderTarget;
  Pipeline p;
  QRhiBuffer* processUBO{};
};

struct PersistSampler
{
  QRhiSampler* sampler{};
  QRhiTexture* textures[2]{nullptr, nullptr};
};
using PassOutput = ossia::variant<PersistSampler, TextureRenderTarget>;

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
