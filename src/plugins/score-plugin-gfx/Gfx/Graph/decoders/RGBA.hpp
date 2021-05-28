#pragma once
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>

namespace score::gfx
{
#include <Gfx/Qt5CompatPush> // clang-format: keep
struct RGB0Decoder : GPUVideoDecoder
{
  static const constexpr auto rgb_filter = R"_(#version 450
    layout(std140, binding = 0) uniform buf {
    mat4 clipSpaceCorrMatrix;
    vec2 texcoordAdjust;
    } tbuf;

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
      vec2 texcoord = vec2(v_texcoord.x, tbuf.texcoordAdjust.y + tbuf.texcoordAdjust.x * v_texcoord.y);
      fragColor = processTexture(texture(y_tex, texcoord));
    })_";

  RGB0Decoder(
      QRhiTexture::Format fmt,
      Video::VideoInterface& d,
      QString f = "")
      : format{fmt}
      , decoder{d}
      , filter{f}
  {
  }
  QRhiTexture::Format format;
  Video::VideoInterface& decoder;
  QString filter;

  std::pair<QShader, QShader> init(RenderList& r) override
  {
    auto& rhi = *r.state.rhi;
    const auto w = decoder.width, h = decoder.height;

    {
      auto tex = rhi.newTexture(format, QSize{w, h}, 1, QRhiTexture::Flag{});
      tex->create();

      auto sampler = rhi.newSampler(
          QRhiSampler::Linear,
          QRhiSampler::Linear,
          QRhiSampler::None,
          QRhiSampler::ClampToEdge,
          QRhiSampler::ClampToEdge);
      sampler->create();
      m_samplers.push_back({sampler, tex});
    }

    return score::gfx::makeShaders(TexturedTriangle::instance().defaultVertexShader(), QString(rgb_filter).arg(filter));
  }

  void exec(
      RenderList&,
      QRhiResourceUpdateBatch& res,
      AVFrame& frame) override
  {
    setPixels(res, frame.data[0], frame.linesize[0]);
  }

  void setPixels(
      QRhiResourceUpdateBatch& res,
      uint8_t* pixels,
      int stride) const noexcept
  {
    const auto w = decoder.width, h = decoder.height;
    auto y_tex = m_samplers[0].texture;

    QRhiTextureUploadEntry entry{
        0, 0, createTextureUpload(pixels, w, h, 4, stride)};

    QRhiTextureUploadDescription desc{entry};
    res.uploadTexture(y_tex, desc);
  }
};
}
