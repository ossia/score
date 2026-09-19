#pragma once
#include <Gfx/Graph/decoders/ColorSpace.hpp>
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>

extern "C" {
#include <libavformat/avformat.h>
}

namespace score::gfx
{

/**
 * @brief Decodes P210 and P216 semi-planar 4:2:2 videos.
 *
 * One layout, two bit depths:
 * - Plane 0: Y (16-bit words), full resolution
 * - Plane 1: UV interleaved (16-bit per component), half width, full height
 *
 * P210 puts 10 bits in the HIGH bits of each word, so R16 unorm returns a value
 * that is short of 1.0 at full scale and has to be renormalized -- that is what
 * the scale is for. P216 uses all 16 bits and wants no scaling at all, which is
 * the only difference between the two and why they share this decoder.
 *
 * P216 is what NDI delivers for a 16-bit source, and NDI delivers it as
 * individual FIELDS: half-height frames at twice the rate. Those are uploaded
 * into the two halves of one full-height texture (see planeRows) and resolved
 * by score_tc, so nothing here weaves anything on the CPU.
 */
struct P210Decoder : GPUVideoDecoder
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
  const float s = %3;
  float y = s * texture(y_tex, score_tc(v_texcoord)).r;
  float u = s * texture(uv_tex, score_tc(v_texcoord)).r;
  float v = s * texture(uv_tex, score_tc(v_texcoord)).g;

  fragColor = processTexture(vec4(y, u, v, 1.));
})_";

  Video::ImageFormat& decoder;

  /// "1.0" for P216, which uses every bit; the MSB-aligned renormalization for
  /// P210, whose ten bits sit at the top of a 16-bit word.
  const char* scale{SCORE_GFX_MSB_ALIGNED_SCALE};

  explicit P210Decoder(
      Video::ImageFormat& d, const char* sampleScale = SCORE_GFX_MSB_ALIGNED_SCALE)
      : decoder{d}
      , scale{sampleScale}
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

    // UV plane: RG16 at half width, full height
    {
      auto tex
          = rhi.newTexture(QRhiTexture::RG16, {w / 2, h}, 1, QRhiTexture::Flag{});
      tex->create();

      auto sampler = rhi.newSampler(
          QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
          QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
      sampler->create();
      samplers.push_back({sampler, tex});
    }

    return score::gfx::makeShaders(
        r.state, vertexShader(),
        QString(frag).arg("").arg(colorMatrix(decoder)).arg(scale));
  }

  void exec(RenderList&, QRhiResourceUpdateBatch& res, AVFrame& frame) override
  {
    // Both planes are full height in 4:2:2, so a field halves both of them.
    {
      const auto w = decoder.width;
      const auto [rows, offset] = planeRows(decoder, frame, decoder.height);
      auto sub = createTextureUpload(frame.data[0], w, rows, 2, frame.linesize[0]);
      sub.setSourceSize({w, rows});
      sub.setDestinationTopLeft({0, offset});
      res.uploadTexture(samplers[0].texture, {QRhiTextureUploadEntry{0, 0, sub}});
    }
    {
      const auto w = decoder.width / 2;
      const auto [rows, offset] = planeRows(decoder, frame, decoder.height);
      auto sub = createTextureUpload(frame.data[1], w, rows, 4, frame.linesize[1]);
      sub.setSourceSize({w, rows});
      sub.setDestinationTopLeft({0, offset});
      res.uploadTexture(samplers[1].texture, {QRhiTextureUploadEntry{0, 0, sub}});
    }
  }
};

}
