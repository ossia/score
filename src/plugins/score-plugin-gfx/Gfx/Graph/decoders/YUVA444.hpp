#pragma once
#include <Gfx/Graph/decoders/ColorSpace.hpp>
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>

extern "C" {
#include <libavformat/avformat.h>
}

namespace score::gfx
{

/**
 * @brief Decodes YUVA444P videos.
 *
 * Full chroma resolution planar YUV (4:4:4) with an alpha plane.
 * All four planes are at full resolution, 8-bit.
 */
struct YUVA444Decoder : GPUVideoDecoder
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

  explicit YUVA444Decoder(Video::ImageFormat& d)
      : decoder{d}
  {
  }

  Video::ImageFormat& decoder;

  std::pair<QShader, QShader> init(RenderList& r) override
  {
    auto& rhi = *r.state.rhi;
    const auto w = decoder.width, h = decoder.height;
    const auto fmt = QRhiTexture::R8;

    // Y, U, V, A - all at full resolution
    for(int i = 0; i < 4; i++)
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
    for(int i = 0; i < 4; i++)
    {
      QRhiTextureUploadEntry entry{
          0, 0, createTextureUpload(frame.data[i], w, h, 1, frame.linesize[i])};
      QRhiTextureUploadDescription desc{entry};
      res.uploadTexture(samplers[i].texture, desc);
    }
  }
};

/**
 * @brief Decodes YUVA444P10 videos.
 *
 * Full chroma resolution planar YUV (4:4:4) with alpha at 10-bit depth.
 * All four planes are at full resolution, stored as 16-bit words.
 * This is the primary output format for ProRes 4444.
 */
struct YUVA444P10Decoder : GPUVideoDecoder
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
  float y = 64. * texture(y_tex, v_texcoord).r;
  float u = 64. * texture(u_tex, v_texcoord).r;
  float v = 64. * texture(v_tex, v_texcoord).r;
  float a = 64. * texture(a_tex, v_texcoord).r;

  vec4 rgb = processTexture(vec4(y,u,v, 1.));
  fragColor = vec4(rgb.rgb, a);
})_";

  explicit YUVA444P10Decoder(Video::ImageFormat& d)
      : decoder{d}
  {
  }

  Video::ImageFormat& decoder;

  std::pair<QShader, QShader> init(RenderList& r) override
  {
    auto& rhi = *r.state.rhi;
    const auto w = decoder.width, h = decoder.height;
    const auto fmt = QRhiTexture::R16;

    // Y, U, V, A - all at full resolution
    for(int i = 0; i < 4; i++)
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
    for(int i = 0; i < 4; i++)
    {
      QRhiTextureUploadEntry entry{
          0, 0, createTextureUpload(frame.data[i], w, h, 2, frame.linesize[i])};
      QRhiTextureUploadDescription desc{entry};
      res.uploadTexture(samplers[i].texture, desc);
    }
  }
};

}
