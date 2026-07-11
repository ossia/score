#pragma once
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>
extern "C" {
#include <libavformat/avformat.h>
}

namespace score::gfx
{
/**
 * @brief Decodes 10-bit RGB packed in r210 (DeckLink bmdFormat10BitRGB).
 *
 * One pixel per 32-bit BIG-ENDIAN word: (R << 20) | (G << 10) | B, full-scale
 * 0-1023 — the exact inverse of PackedRGBEncoder::r210be(). Rows are padded to
 * 256-byte multiples (((width + 63) / 64) * 256), so the input texture is
 * sized on the padded stride and indexed 1 texel : 1 pixel (padding sits past
 * the visible width).
 */
struct R210Decoder : GPUVideoDecoder
{
  // %1 = user filter
  static const constexpr auto frag = R"_(#version 450

)_" SCORE_GFX_VIDEO_UNIFORMS R"_(

layout(binding=3) uniform sampler2D u_tex;

layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;

vec4 processTexture(vec4 tex) {
  vec4 processed = tex;
  { %1 }
  return processed;
}

void main() {
  int x = int(floor(v_texcoord.x * mat.texSz.x));
  int y = int(floor(v_texcoord.y * mat.texSz.y));
  vec4 t = texelFetch(u_tex, ivec2(x, y), 0);
  uint b0 = uint(t.r * 255.0 + 0.5);
  uint b1 = uint(t.g * 255.0 + 0.5);
  uint b2 = uint(t.b * 255.0 + 0.5);
  uint b3 = uint(t.a * 255.0 + 0.5);
  uint w = (b0 << 24u) | (b1 << 16u) | (b2 << 8u) | b3; // big-endian word
  float r = float((w >> 20u) & 0x3FFu);
  float g = float((w >> 10u) & 0x3FFu);
  float b = float( w         & 0x3FFu);
  fragColor = processTexture(vec4(r, g, b, 1023.0) / 1023.0);
}
)_";

  R210Decoder(Video::ImageFormat& d)
      : decoder{d}
  {
  }

  Video::ImageFormat& decoder;

  static constexpr int rowBytesFor(int w) noexcept
  {
    return ((w + 63) / 64) * 256;
  }

  std::pair<QShader, QShader> init(RenderList& r) override
  {
    auto& rhi = *r.state.rhi;
    const auto w = decoder.width, h = decoder.height;

    // One RGBA8 texel per r210 word, on the padded wire stride.
    const int texW = rowBytesFor(w) / 4;
    {
      auto tex
          = rhi.newTexture(QRhiTexture::RGBA8, {texW, h}, 1, QRhiTexture::Flag{});
      tex->create();

      auto sampler = rhi.newSampler(
          QRhiSampler::Nearest, QRhiSampler::Nearest, QRhiSampler::None,
          QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
      sampler->create();

      samplers.push_back({sampler, tex});
    }

    return score::gfx::makeShaders(
        r.state, vertexShader(), QString(frag).arg(""));
  }

  void exec(RenderList&, QRhiResourceUpdateBatch& res, AVFrame& frame) override
  {
    const int texW = rowBytesFor(decoder.width) / 4;
    auto tex = samplers[0].texture;
    QRhiTextureUploadEntry entry{
        0, 0,
        createTextureUpload(
            frame.data[0], texW, decoder.height, /*bpp=*/4, frame.linesize[0])};
    QRhiTextureUploadDescription desc{entry};
    res.uploadTexture(tex, desc);
  }
};

} // namespace score::gfx
