#pragma once
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>

extern "C" {
#include <libavformat/avformat.h>
}

namespace score::gfx
{

/**
 * @brief Decodes 8-bit palettized video (AV_PIX_FMT_PAL8): GIF, QuickTime and
 * MS RLE, and the screen and game codecs of the 90s.
 *
 * Each pixel is an index into a 256-entry palette that the frame carries in
 * data[1] and that may change from one frame to the next, so it is uploaded
 * with every frame into a 256x1 texture. The entries are native-endian
 * 0xAARRGGBB words, i.e. B, G, R, A bytes on every platform score runs on:
 * a BGRA8 texture reads them as they are, and the alpha is the palette's own
 * (GIF transparency).
 */
struct PAL8Decoder : GPUVideoDecoder
{
  static const constexpr auto frag = R"_(#version 450

)_" SCORE_GFX_VIDEO_UNIFORMS R"_(

layout(binding=3) uniform sampler2D y_tex;
layout(binding=4) uniform sampler2D p_tex;

layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;

vec4 processTexture(vec4 tex) {
  vec4 processed = tex;
  { %1 }
  return processed;
}

void main ()
{
  float index = texture(y_tex, score_tc(v_texcoord)).r;
  fragColor = processTexture(texelFetch(p_tex, ivec2(int(index * 255.0 + 0.5), 0), 0));
})_";

  explicit PAL8Decoder(Video::ImageFormat& d, QString f = "")
      : decoder{d}
      , filter{std::move(f)}
  {
  }

  Video::ImageFormat& decoder;
  QString filter;

  std::pair<QShader, QShader> init(RenderList& r) override
  {
    auto& rhi = *r.state.rhi;
    const auto w = decoder.width, h = decoder.height;

    // Indices are not values: interpolating between two of them picks an
    // unrelated palette entry.
    {
      auto tex = rhi.newTexture(QRhiTexture::R8, {w, h}, 1, QRhiTexture::Flag{});
      tex->create();
      auto sampler = rhi.newSampler(
          QRhiSampler::Nearest, QRhiSampler::Nearest, QRhiSampler::None,
          QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
      sampler->create();
      samplers.push_back({sampler, tex});
    }
    {
      auto tex = rhi.newTexture(QRhiTexture::BGRA8, {256, 1}, 1, QRhiTexture::Flag{});
      tex->create();
      auto sampler = rhi.newSampler(
          QRhiSampler::Nearest, QRhiSampler::Nearest, QRhiSampler::None,
          QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
      sampler->create();
      samplers.push_back({sampler, tex});
    }

    return score::gfx::makeShaders(r.state, vertexShader(), QString(frag).arg(filter));
  }

  void exec(RenderList&, QRhiResourceUpdateBatch& res, AVFrame& frame) override
  {
    const auto w = decoder.width, h = decoder.height;
    {
      QRhiTextureUploadEntry entry{
          0, 0, createTextureUpload(frame.data[0], w, h, 1, frame.linesize[0])};
      QRhiTextureUploadDescription desc{entry};
      res.uploadTexture(samplers[0].texture, desc);
    }
    if(frame.data[1])
    {
      QRhiTextureUploadEntry entry{
          0, 0, createTextureUpload(frame.data[1], 256, 1, 4, 256 * 4)};
      QRhiTextureUploadDescription desc{entry};
      res.uploadTexture(samplers[1].texture, desc);
    }
  }
};

}
