#pragma once
#include <Gfx/Graph/decoders/ColorSpace.hpp>
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>

extern "C" {
#include <libavformat/avformat.h>
}

namespace score::gfx
{

/**
 * @brief Decodes P016 videos.
 *
 * P016 is a semi-planar 16-bit 4:2:0 format, output by
 * DXVA2/D3D11 hardware decoders on Windows for 10/12-bit content.
 *
 * Layout:
 * - Plane 0: Y (16-bit per sample), full resolution
 * - Plane 1: UV interleaved (16-bit per component), half resolution
 */
struct P016Decoder : GPUVideoDecoder
{
  static const constexpr auto frag = R"_(#version 450

)_" SCORE_GFX_VIDEO_UNIFORMS R"_(

layout(binding=3) uniform sampler2D y_tex;
layout(binding=4) uniform sampler2D uv_tex;

layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;

%2

vec4 processTexture(vec4 tex) {
  vec4 processed = convert_to_rgb(tex);
  { %1 }
  return processed;
}

void main()
{
  float y = texture(y_tex, v_texcoord).r;
  float u = texture(uv_tex, v_texcoord).r;
  float v = texture(uv_tex, v_texcoord).g;

  fragColor = processTexture(vec4(y, u, v, 1.));
})_";

  Video::ImageFormat& decoder;

  explicit P016Decoder(Video::ImageFormat& d)
      : decoder{d}
  {
  }

  std::pair<QShader, QShader> init(RenderList& r) override
  {
    auto& rhi = *r.state.rhi;
    const auto w = decoder.width, h = decoder.height;

    // Y plane: R16 at full resolution
    {
      auto tex = rhi.newTexture(QRhiTexture::R16, {w, h}, 1, QRhiTexture::Flag{});
      tex->create();

      auto sampler = rhi.newSampler(
          QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
          QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
      sampler->create();
      samplers.push_back({sampler, tex});
    }

    // UV plane: RG16 at half resolution
    {
      auto tex
          = rhi.newTexture(QRhiTexture::RG16, {w / 2, h / 2}, 1, QRhiTexture::Flag{});
      tex->create();

      auto sampler = rhi.newSampler(
          QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
          QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
      sampler->create();
      samplers.push_back({sampler, tex});
    }

    return score::gfx::makeShaders(
        r.state, vertexShader(), QString(frag).arg("").arg(colorMatrix(decoder)));
  }

  void exec(RenderList&, QRhiResourceUpdateBatch& res, AVFrame& frame) override
  {
    {
      const auto w = decoder.width, h = decoder.height;
      QRhiTextureUploadEntry entry{
          0, 0, createTextureUpload(frame.data[0], w, h, 2, frame.linesize[0])};
      res.uploadTexture(samplers[0].texture, {entry});
    }
    {
      const auto w = decoder.width / 2, h = decoder.height / 2;
      QRhiTextureUploadEntry entry{
          0, 0, createTextureUpload(frame.data[1], w, h, 4, frame.linesize[1])};
      res.uploadTexture(samplers[1].texture, {entry});
    }
  }
};

}
