#pragma once
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>

namespace score::gfx
{

/**
 * @brief NDI PA16: P216 with a full-resolution 16-bit alpha plane after it.
 *
 * Three planes, all the same size: 16-bit luma, interleaved 16-bit CbCr at
 * half width, 16-bit alpha. Like UYVA it has no AVPixelFormat -- P216LE
 * describes the first two planes and knows nothing of the third -- so it is
 * selected through ImageFormat::native_format.
 *
 * This is what "Best available (16-bit)" delivers for a source with alpha, so
 * mapping it to P216 and stopping, which is what the NDI input did before,
 * loses the alpha of exactly the sources most likely to have one.
 */
struct PA16Decoder : GPUVideoDecoder
{
  static const constexpr auto frag = R"_(#version 450

)_" SCORE_GFX_VIDEO_UNIFORMS R"_(

layout(binding=3) uniform sampler2D y_tex;
layout(binding=4) uniform sampler2D uv_tex;
layout(binding=5) uniform sampler2D a_tex;

layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;

%2

vec4 processTexture(vec4 tex) {
  vec4 processed = convert_to_rgb(tex);
  { %1 }
  return processed;
}

void main() {
  vec2 tc = score_tc(v_texcoord);
  float y = texture(y_tex, tc).r;
  float u = texture(uv_tex, tc).r;
  float v = texture(uv_tex, tc).g;
  float a = texture(a_tex, tc).r;

  vec4 rgb = processTexture(vec4(y, u, v, 1.));
  fragColor = vec4(rgb.rgb, a);
}
)_";

  explicit PA16Decoder(Video::ImageFormat& d)
      : decoder{d}
  {
  }

  Video::ImageFormat& decoder;

  std::pair<QShader, QShader> init(RenderList& r) override
  {
    auto& rhi = *r.state.rhi;
    const auto w = decoder.width, h = decoder.height;

    auto mkSampler = [&] {
      auto sampler = rhi.newSampler(
          QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
          QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
      sampler->create();
      return sampler;
    };
    auto plane = [&](QRhiTexture::Format fmt, QSize sz) {
      auto tex = rhi.newTexture(fmt, sz, 1, QRhiTexture::Flag{});
      tex->create();
      samplers.push_back({mkSampler(), tex});
    };

    plane(QRhiTexture::R16, {w, h});       // Y
    plane(QRhiTexture::RG16, {w / 2, h});  // interleaved CbCr
    plane(QRhiTexture::R16, {w, h});       // A

    return score::gfx::makeShaders(
        r.state, vertexShader(), QString(frag).arg("").arg(colorMatrix(decoder)));
  }

  void exec(RenderList&, QRhiResourceUpdateBatch& res, AVFrame& frame) override
  {
    const int widths[3] = {decoder.width, decoder.width / 2, decoder.width};
    const int bpt[3] = {2, 4, 2};
    for(int i = 0; i < 3; i++)
    {
      const auto [rows, offset] = planeRows(decoder, frame, decoder.height);
      auto sub = createTextureUpload(
          frame.data[i], widths[i], rows, bpt[i], frame.linesize[i]);
      sub.setDestinationTopLeft({0, offset});
      res.uploadTexture(samplers[i].texture, QRhiTextureUploadDescription{{0, 0, sub}});
    }
  }
};

}
