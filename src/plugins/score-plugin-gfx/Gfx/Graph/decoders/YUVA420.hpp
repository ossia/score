#pragma once
#include <Gfx/Graph/decoders/ColorSpace.hpp>
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>

extern "C" {
#include <libavformat/avformat.h>
}

namespace score::gfx
{

/**
 * @brief Decodes YUVA420P videos.
 *
 * YUV420 with an additional alpha plane at full resolution.
 * Used by VP9/AV1 alpha channel encoding, ProRes 4444 with transparency,
 * and WebM transparency.
 */
struct YUVA420Decoder : GPUVideoDecoder
{
  static const constexpr auto frag = R"_(#version 450

)_" SCORE_GFX_VIDEO_UNIFORMS R"_(

layout(binding=3) uniform sampler2D y_tex;
layout(binding=4) uniform sampler2D u_tex;
layout(binding=5) uniform sampler2D v_tex;
layout(binding=6) uniform sampler2D a_tex;

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
  float y = texture(y_tex, v_texcoord).r;
  float u = texture(u_tex, v_texcoord).r;
  float v = texture(v_tex, v_texcoord).r;
  float a = texture(a_tex, v_texcoord).r;

  vec4 rgb = processTexture(vec4(y,u,v, 1.));
  fragColor = vec4(rgb.rgb, a);
})_";

  explicit YUVA420Decoder(Video::ImageFormat& d)
      : decoder{d}
  {
  }

  Video::ImageFormat& decoder;

  std::pair<QShader, QShader> init(RenderList& r) override
  {
    auto& rhi = *r.state.rhi;
    const auto w = decoder.width, h = decoder.height;

    // Y plane (full res)
    {
      auto tex = rhi.newTexture(QRhiTexture::R8, {w, h}, 1, QRhiTexture::Flag{});
      tex->create();

      auto sampler = rhi.newSampler(
          QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
          QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
      sampler->create();
      samplers.push_back({sampler, tex});
    }

    // U plane (half res)
    {
      auto tex
          = rhi.newTexture(QRhiTexture::R8, {w / 2, h / 2}, 1, QRhiTexture::Flag{});
      tex->create();

      auto sampler = rhi.newSampler(
          QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
          QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
      sampler->create();
      samplers.push_back({sampler, tex});
    }

    // V plane (half res)
    {
      auto tex
          = rhi.newTexture(QRhiTexture::R8, {w / 2, h / 2}, 1, QRhiTexture::Flag{});
      tex->create();

      auto sampler = rhi.newSampler(
          QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
          QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
      sampler->create();
      samplers.push_back({sampler, tex});
    }

    // A plane (full res)
    {
      auto tex = rhi.newTexture(QRhiTexture::R8, {w, h}, 1, QRhiTexture::Flag{});
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

    // Y
    {
      QRhiTextureUploadEntry entry{
          0, 0, createTextureUpload(frame.data[0], w, h, 1, frame.linesize[0])};
      QRhiTextureUploadDescription desc{entry};
      res.uploadTexture(samplers[0].texture, desc);
    }
    // U
    {
      QRhiTextureUploadEntry entry{
          0, 0,
          createTextureUpload(frame.data[1], w / 2, h / 2, 1, frame.linesize[1])};
      QRhiTextureUploadDescription desc{entry};
      res.uploadTexture(samplers[1].texture, desc);
    }
    // V
    {
      QRhiTextureUploadEntry entry{
          0, 0,
          createTextureUpload(frame.data[2], w / 2, h / 2, 1, frame.linesize[2])};
      QRhiTextureUploadDescription desc{entry};
      res.uploadTexture(samplers[2].texture, desc);
    }
    // A
    {
      QRhiTextureUploadEntry entry{
          0, 0, createTextureUpload(frame.data[3], w, h, 1, frame.linesize[3])};
      QRhiTextureUploadDescription desc{entry};
      res.uploadTexture(samplers[3].texture, desc);
    }
  }
};

}
