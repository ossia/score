#pragma once
#include <Gfx/Graph/decoders/ColorSpace.hpp>
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>

extern "C" {
#include <libavformat/avformat.h>
}

namespace score::gfx
{

/**
 * @brief Decodes VUYA / VUYX packed 4:4:4 8-bit videos.
 *
 * Packed as V U Y A per pixel (32bpp).
 * Uploaded as RGBA8, so tex.r=V, tex.g=U, tex.b=Y, tex.a=A.
 *
 * Used by D3D11/DXVA2 on Windows for 4:4:4 output.
 */
struct VUYADecoder : GPUVideoDecoder
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
  vec4 vuya = texture(u_tex, v_texcoord);
  float y = vuya.b;
  float u = vuya.g;
  float v = vuya.r;

  vec4 rgb = processTexture(vec4(y, u, v, 1.));
  fragColor = vec4(rgb.rgb, %3);
})_";

  Video::ImageFormat& decoder;
  bool opaque{};

  VUYADecoder(Video::ImageFormat& d, bool opaque_)
      : decoder{d}
      , opaque{opaque_}
  {
  }

  std::pair<QShader, QShader> init(RenderList& r) override
  {
    auto& rhi = *r.state.rhi;
    const auto w = decoder.width, h = decoder.height;

    {
      auto tex = rhi.newTexture(QRhiTexture::RGBA8, {w, h}, 1, QRhiTexture::Flag{});
      tex->create();

      auto sampler = rhi.newSampler(
          QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
          QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
      sampler->create();
      samplers.push_back({sampler, tex});
    }

    // VUYX has undefined alpha, use 1.0; VUYA preserves alpha
    QString alpha = opaque ? "1.0" : "vuya.a";

    return score::gfx::makeShaders(
        r.state, vertexShader(),
        QString(frag).arg("").arg(colorMatrix(decoder)).arg(alpha));
  }

  void exec(RenderList&, QRhiResourceUpdateBatch& res, AVFrame& frame) override
  {
    auto pixels = frame.data[0];
    auto stride = frame.linesize[0];
    const auto w = decoder.width, h = decoder.height;

    QRhiTextureUploadEntry entry{
        0, 0, createTextureUpload(pixels, w, h, 4, stride)};
    res.uploadTexture(samplers[0].texture, {entry});
  }
};

}
