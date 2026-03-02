#pragma once
#include <Gfx/Graph/decoders/ColorSpace.hpp>
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>

extern "C" {
#include <libavformat/avformat.h>
}

namespace score::gfx
{

/**
 * @brief Decodes P010 videos.
 *
 * P010 is a semi-planar 10-bit 4:2:0 format, commonly output by
 * hardware decoders (VAAPI, NVDEC, DXVA2) for 10-bit HEVC/AV1 content.
 *
 * Layout:
 * - Plane 0: Y (16-bit per sample, 10 bits used, left-shifted), full resolution
 * - Plane 1: UV interleaved (16-bit per component), half resolution in both dimensions
 */
struct P010Decoder : GPUVideoDecoder
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
  // P010 stores 10-bit data in the high bits of 16-bit words,
  // so R16 unorm already returns values normalized to [0, 1].
  float y = texture(y_tex, v_texcoord).r;
  float u = texture(uv_tex, v_texcoord).r;
  float v = texture(uv_tex, v_texcoord).g;

  fragColor = processTexture(vec4(y, u, v, 1.));
})_";

  Video::ImageFormat& decoder;

  explicit P010Decoder(Video::ImageFormat& d)
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

    // UV plane: RG16 (two 16-bit channels interleaved) at half resolution
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
    setYPixels(res, frame.data[0], frame.linesize[0]);
    setUVPixels(res, frame.data[1], frame.linesize[1]);
  }

  void
  setYPixels(QRhiResourceUpdateBatch& res, uint8_t* pixels, int stride) const noexcept
  {
    const auto w = decoder.width, h = decoder.height;
    auto y_tex = samplers[0].texture;

    QRhiTextureUploadEntry entry{0, 0, createTextureUpload(pixels, w, h, 2, stride)};
    QRhiTextureUploadDescription desc{entry};

    res.uploadTexture(y_tex, desc);
  }

  void
  setUVPixels(QRhiResourceUpdateBatch& res, uint8_t* pixels, int stride) const noexcept
  {
    const auto w = decoder.width / 2, h = decoder.height / 2;
    auto uv_tex = samplers[1].texture;

    QRhiTextureUploadEntry entry{0, 0, createTextureUpload(pixels, w, h, 4, stride)};
    QRhiTextureUploadDescription desc{entry};

    res.uploadTexture(uv_tex, desc);
  }
};

}
