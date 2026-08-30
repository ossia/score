#pragma once
#include <Gfx/Graph/decoders/ColorSpace.hpp>
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>

extern "C" {
#include <libavformat/avformat.h>
}

namespace score::gfx
{

/**
 * @brief Decodes Y210 packed 4:2:2 10-bit videos.
 *
 * Packed as Y0 Cb Y1 Cr, each a 16-bit little-endian word with the 10 bits of
 * data in the high end. 8 bytes per macropixel (2 pixels), so 4 * width bytes
 * per row.
 *
 * QRhi has no four-channel 16-bit UNORM format, so the samples are uploaded
 * into an RG16 texture at {w, h}: 4 bytes per texel is exactly one luma plus
 * one chroma sample, and texel x holds (Y of pixel x, Cb on even columns / Cr
 * on odd columns). The shader takes the luma of its own texel and the two
 * chroma samples of the macropixel it belongs to.
 */
struct Y210Decoder : GPUVideoDecoder
{
  static const constexpr auto frag = R"_(#version 450

)_" SCORE_GFX_VIDEO_UNIFORMS R"_(

layout(binding=3) uniform sampler2D u_tex;

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
  int x = int(floor(v_texcoord.x * mat.texSz.x));
  int y = int(floor(v_texcoord.y * mat.texSz.y));
  int cx = (x / 2) * 2;

  const float s = )_" SCORE_GFX_MSB_ALIGNED_SCALE R"_(;
  float luma = texelFetch(u_tex, ivec2(x, y), 0).r * s;
  float cb = texelFetch(u_tex, ivec2(cx, y), 0).g * s;
  float cr = texelFetch(u_tex, ivec2(cx + 1, y), 0).g * s;

  fragColor = processTexture(vec4(luma, cb, cr, 1.));
})_";

  Video::ImageFormat& decoder;

  explicit Y210Decoder(Video::ImageFormat& d)
      : decoder{d}
  {
  }

  std::pair<QShader, QShader> init(RenderList& r) override
  {
    auto& rhi = *r.state.rhi;
    const auto w = decoder.width, h = decoder.height;

    {
      auto tex = rhi.newTexture(QRhiTexture::RG16, {w, h}, 1, QRhiTexture::Flag{});
      tex->create();

      auto sampler = rhi.newSampler(
          QRhiSampler::Nearest, QRhiSampler::Nearest, QRhiSampler::None,
          QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
      sampler->create();
      samplers.push_back({sampler, tex});
    }

    return score::gfx::makeShaders(
        r.state, vertexShader(),
        QString(frag).arg("").arg(colorMatrix(decoder)));
  }

  void exec(RenderList&, QRhiResourceUpdateBatch& res, AVFrame& frame) override
  {
    const auto w = decoder.width, h = decoder.height;
    auto pixels = frame.data[0];
    auto stride = frame.linesize[0];

    // One RG16 texel per pixel: 4 bytes = one 16-bit luma + one 16-bit chroma
    QRhiTextureUploadEntry entry{0, 0, createTextureUpload(pixels, w, h, 4, stride)};
    res.uploadTexture(samplers[0].texture, {entry});
  }
};

}
