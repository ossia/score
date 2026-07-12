#pragma once

/**
 * @file PackedBitfieldYUV.hpp
 * @brief YUV packed into sub-byte bit fields (V4L2 Y444 / YUVO / YUVP).
 *
 * These are the same containers as the RGB bitfield formats in
 * PackedBitfield.hpp -- 4-4-4-4, 1-5-5-5 and 5-6-5 in 16 bits -- carrying
 * Y, U, V instead of R, G, B. The only reason they need their own decoder is
 * the colour conversion: PackedDecoder's shader hands the filter's result
 * straight to the output, while YUV has to go through convert_to_rgb with the
 * frame's own matrix.
 *
 * NEAREST sampling, for the same reason as the RGB bitfield formats: linear
 * filtering blends the container's bytes with their neighbours before the
 * fields can be extracted.
 */

#include <Gfx/Graph/decoders/ColorSpace.hpp>
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>
#include <Gfx/Graph/decoders/PackedBitfield.hpp>

namespace score::gfx
{

/// Field placement for a packed YUV container. V4L2 documents these as
/// "A/XYUV", i.e. alpha in the high bits, then Y, U, V downwards.
struct PackedBitfieldYUVLayout
{
  BitField y, u, v, a;
};

struct PackedBitfieldYUVDecoder : GPUVideoDecoder
{
  static const constexpr auto frag = R"_(#version 450

)_" SCORE_GFX_VIDEO_UNIFORMS R"_(

layout(binding=3) uniform sampler2D u_tex;

layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;

%2

vec4 processTexture(vec4 tex) {
  vec4 processed = tex;
  { %1 }
  return processed;
}

void main()
{
  vec4 texel = texture(u_tex, v_texcoord);
  uint w = (uint(texel.g * 255.0 + 0.5) << 8) | uint(texel.r * 255.0 + 0.5);
  float y = %3;
  float u = %4;
  float v = %5;
  vec4 rgb = convert_to_rgb(vec4(y, u, v, 1.0));
  fragColor = processTexture(vec4(rgb.rgb, %6));
}
)_";

  PackedBitfieldYUVDecoder(Video::ImageFormat& d, PackedBitfieldYUVLayout l)
      : decoder{d}
      , layout{l}
  {
  }

  Video::ImageFormat& decoder;
  PackedBitfieldYUVLayout layout;

  static QString field(const BitField& f)
  {
    if(f.width <= 0)
      return "1.0";
    const unsigned mask = (1u << f.width) - 1u;
    return QString("float((w >> %1) & %2u) / %3.0")
        .arg(f.offset)
        .arg(mask)
        .arg(mask);
  }

  std::pair<QShader, QShader> init(RenderList& r) override
  {
    auto& rhi = *r.state.rhi;
    const auto w = decoder.width, h = decoder.height;

    auto tex = rhi.newTexture(QRhiTexture::RG8, {w, h}, 1, QRhiTexture::Flag{});
    tex->create();
    auto sampler = rhi.newSampler(
        QRhiSampler::Nearest, QRhiSampler::Nearest, QRhiSampler::None,
        QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    sampler->create();
    samplers.push_back({sampler, tex});

    return score::gfx::makeShaders(
        r.state, vertexShader(),
        QString(frag)
            .arg("")
            .arg(colorMatrix(decoder))
            .arg(field(layout.y))
            .arg(field(layout.u))
            .arg(field(layout.v))
            .arg(field(layout.a)));
  }

  void exec(RenderList&, QRhiResourceUpdateBatch& res, AVFrame& frame) override
  {
    setPixels(res, frame);
  }

  void setPixels(QRhiResourceUpdateBatch& res, AVFrame& frame) const noexcept
  {
    setYPixels(res, samplers[0].texture, frame.data[0], frame.linesize[0]);
  }

  void setYPixels(
      QRhiResourceUpdateBatch& res, QRhiTexture* tex, uint8_t* pixels,
      int stride) const noexcept
  {
    const auto w = decoder.width, h = decoder.height;
    QRhiTextureUploadEntry entry{
        0, 0, createTextureUpload(pixels, w, h, 2, stride)};
    res.uploadTexture(tex, QRhiTextureUploadDescription{entry});
  }
};

} // namespace score::gfx
