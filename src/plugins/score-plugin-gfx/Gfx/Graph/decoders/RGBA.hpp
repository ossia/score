#pragma once
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>
extern "C"
{
#include <libavformat/avformat.h>
}

namespace score::gfx
{
#include <Gfx/Qt5CompatPush> // clang-format: keep
struct PackedDecoder : GPUVideoDecoder
{
  static const constexpr auto rgb_filter = R"_(#version 450
    layout(std140, binding = 0) uniform renderer_t {
    mat4 clipSpaceCorrMatrix;
    vec2 texcoordAdjust;

    vec2 renderSize;
    } renderer;

    layout(binding=3) uniform sampler2D y_tex;

    layout(location = 0) in vec2 v_texcoord;
    layout(location = 0) out vec4 fragColor;

    vec4 processTexture(vec4 tex) {
      vec4 processed = tex;
      { %1 }
      return processed;
    }

    void main ()
    {
      fragColor = processTexture(texture(y_tex, v_texcoord));
    })_";

  PackedDecoder(
      QRhiTexture::Format fmt,
      int bytes_per_pixel,
      Video::VideoMetadata& d,
      QString f = "")
      : format{fmt}
      , bytes_per_pixel{bytes_per_pixel}
      , decoder{d}
      , filter{std::move(f)}
  {
  }
  QRhiTexture::Format format;
  int bytes_per_pixel{}; // bpp/8 !
  Video::VideoMetadata& decoder;
  QString filter;

  std::pair<QShader, QShader> init(RenderList& r) override
  {
    auto& rhi = *r.state.rhi;
    const auto w = decoder.width, h = decoder.height;

    {
      // Create a texture
      auto tex = rhi.newTexture(format, QSize{w, h}, 1, QRhiTexture::Flag{});
      tex->create();

      // Create a sampler
      auto sampler = rhi.newSampler(
          QRhiSampler::Linear,
          QRhiSampler::Linear,
          QRhiSampler::None,
          QRhiSampler::ClampToEdge,
          QRhiSampler::ClampToEdge);
      sampler->create();

      // Store both
      samplers.push_back({sampler, tex});
    }

    return score::gfx::makeShaders(vertexShader(), QString(rgb_filter).arg(filter));
  }

  void exec(
      RenderList&,
      QRhiResourceUpdateBatch& res,
      AVFrame& frame) override
  {
    // Nothing particular, we just upload the whole buffer
    setPixels(res, frame.data[0], frame.linesize[0]);
  }

  void setPixels(
      QRhiResourceUpdateBatch& res,
      uint8_t* pixels,
      int stride) const noexcept
  {
    const auto w = decoder.width, h = decoder.height;
    auto y_tex = samplers[0].texture;

    QRhiTextureUploadEntry entry{
        0, 0, createTextureUpload(pixels, w, h, bytes_per_pixel, stride)};

    QRhiTextureUploadDescription desc{entry};
    res.uploadTexture(y_tex, desc);
  }
};
}
