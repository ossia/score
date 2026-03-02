#pragma once
#include <Gfx/Graph/decoders/ColorSpace.hpp>
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>

extern "C" {
#include <libavformat/avformat.h>
}

namespace score::gfx
{

/**
 * @brief Decodes YUV444P10 videos.
 *
 * Full chroma resolution planar YUV (4:4:4) at 10-bit depth.
 * All three planes are at full resolution, stored as 16-bit words.
 */
struct YUV444P10Decoder : GPUVideoDecoder
{
  static const constexpr auto frag = R"_(#version 450

)_" SCORE_GFX_VIDEO_UNIFORMS R"_(

layout(binding=3) uniform sampler2D y_tex;
layout(binding=4) uniform sampler2D u_tex;
layout(binding=5) uniform sampler2D v_tex;

layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;

%2

vec4 processTexture(vec4 tex) {
  vec4 processed = convert_to_rgb(tex);
  { %1 }
  return processed;
}

void main ()
{
  float y = 64. * texture(y_tex, v_texcoord).r;
  float u = 64. * texture(u_tex, v_texcoord).r;
  float v = 64. * texture(v_tex, v_texcoord).r;

  fragColor = processTexture(vec4(y,u,v, 1.));
})_";

  explicit YUV444P10Decoder(Video::ImageFormat& d)
      : decoder{d}
  {
  }

  Video::ImageFormat& decoder;

  std::pair<QShader, QShader> init(RenderList& r) override
  {
    auto& rhi = *r.state.rhi;
    const auto w = decoder.width, h = decoder.height;
    const auto fmt = QRhiTexture::R16;

    // Y, U, V - all at full resolution
    for(int i = 0; i < 3; i++)
    {
      auto tex = rhi.newTexture(fmt, {w, h}, 1, QRhiTexture::Flag{});
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
    const auto w = decoder.width, h = decoder.height;
    for(int i = 0; i < 3; i++)
    {
      QRhiTextureUploadEntry entry{
          0, 0, createTextureUpload(frame.data[i], w, h, 2, frame.linesize[i])};
      QRhiTextureUploadDescription desc{entry};
      res.uploadTexture(samplers[i].texture, desc);
    }
  }
};

}
