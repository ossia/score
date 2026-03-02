#pragma once
#include <Gfx/Graph/decoders/ColorSpace.hpp>
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>

extern "C" {
#include <libavformat/avformat.h>
}

namespace score::gfx
{

/**
 * @brief Decodes NV24 / NV42 semi-planar 4:4:4 8-bit videos.
 *
 * Layout:
 * - Plane 0: Y (8-bit), full resolution
 * - Plane 1: UV interleaved (8-bit per component), full resolution
 *
 * NV42 has U and V swapped relative to NV24.
 */
struct NV24Decoder : GPUVideoDecoder
{
  static const constexpr auto frag_prologue = R"_(#version 450

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
)_";

  static const constexpr auto frag_epilogue = R"_(
  fragColor = processTexture(vec4(yuv, 1.));
})_";

  Video::ImageFormat& decoder;
  bool nv42{};

  NV24Decoder(Video::ImageFormat& d, bool inverted)
      : decoder{d}
      , nv42{inverted}
  {
  }

  std::pair<QShader, QShader> init(RenderList& r) override
  {
    auto& rhi = *r.state.rhi;
    const auto w = decoder.width, h = decoder.height;

    // Y plane: R8 at full resolution
    {
      auto tex = rhi.newTexture(QRhiTexture::R8, {w, h}, 1, QRhiTexture::Flag{});
      tex->create();

      auto sampler = rhi.newSampler(
          QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
          QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
      sampler->create();
      samplers.push_back({sampler, tex});
    }

    // UV plane: RG8 at full resolution
    {
      auto tex = rhi.newTexture(QRhiTexture::RG8, {w, h}, 1, QRhiTexture::Flag{});
      tex->create();

      auto sampler = rhi.newSampler(
          QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
          QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
      sampler->create();
      samplers.push_back({sampler, tex});
    }

    QString frag = frag_prologue;
    if(nv42)
      frag += "    vec3 yuv = vec3(y, v, u);\n";
    else
      frag += "    vec3 yuv = vec3(y, u, v);\n";
    frag += frag_epilogue;

    return score::gfx::makeShaders(
        r.state, vertexShader(), QString(frag).arg("").arg(colorMatrix(decoder)));
  }

  void exec(RenderList&, QRhiResourceUpdateBatch& res, AVFrame& frame) override
  {
    {
      const auto w = decoder.width, h = decoder.height;
      QRhiTextureUploadEntry entry{
          0, 0, createTextureUpload(frame.data[0], w, h, 1, frame.linesize[0])};
      res.uploadTexture(samplers[0].texture, {entry});
    }
    {
      const auto w = decoder.width, h = decoder.height;
      QRhiTextureUploadEntry entry{
          0, 0, createTextureUpload(frame.data[1], w, h, 2, frame.linesize[1])};
      res.uploadTexture(samplers[1].texture, {entry});
    }
  }
};

}
