#pragma once

#include <Gfx/Graph/NodeRenderer.hpp>

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

  void processSpectral(
      AudioTexture& audio, QRhiResourceUpdateBatch& res, QRhiTexture* rhiTexture);

  [[nodiscard]] std::optional<Sampler> updateAudioTexture(
      AudioTexture& audio, RenderList& renderer, char* materialData,
      QRhiResourceUpdateBatch& res);

private:
  std::vector<float> m_scratchpad;
  ossia::fft m_fft;
};

inline void storeTextureRectUniform(char* buffer, int& cur_pos, QSize texSize)
{
  while(cur_pos % 16 != 0)
  {
    cur_pos += 4;
  }

  *(float*)(buffer + cur_pos + 0) = 0.0f;
  *(float*)(buffer + cur_pos + 4) = 0.0f;
  *(float*)(buffer + cur_pos + 8) = texSize.width();
  *(float*)(buffer + cur_pos + 12) = texSize.height();

  cur_pos += 16;
}

}
