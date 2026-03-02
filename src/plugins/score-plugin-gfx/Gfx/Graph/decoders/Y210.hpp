#pragma once
#include <Gfx/Graph/decoders/ColorSpace.hpp>
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>

extern "C" {
#include <libavformat/avformat.h>
}

namespace score::gfx
{

/**
 * @brief Decodes Y210 packed 4:2:2 10-bit videos.
 *
 * Packed as Y0 U Y1 V, each 16-bit with data in the high bits.
 * 8 bytes per macropixel (2 pixels).
 * Uploaded as RGBA16F at {w/2, h}.
 * tex.r=Y0, tex.g=U, tex.b=Y1, tex.a=V.
 *
 * Data in high bits means R16 unorm already returns [0,1] normalized values.
 */
struct Y210Decoder : GPUVideoDecoder
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
  // Y0 U Y1 V packed as RGBA16, one texel = 2 pixels
  // Input texture is half the width of the output
  float colIndex = floor(v_texcoord.x * mat.texSz.x);
  float oddCol = mod(colIndex, 2.0);

  // Offset by half an input pixel to sample the correct texel center
  vec2 dxInput = 0.5 * vec2(1.0 / mat.texSz.x, 0.0);

  float oddY = texture(u_tex, v_texcoord - dxInput).z;
  float evenY = texture(u_tex, v_texcoord + dxInput).x;
  float y = mix(evenY, oddY, oddCol);

  vec2 uv = texture(u_tex, v_texcoord).yw;

  fragColor = processTexture(vec4(y, uv.x, uv.y, 1.));
})_";

  Video::ImageFormat& decoder;

  explicit Y210Decoder(Video::ImageFormat& d)
      : decoder{d}
  {
  }

  std::pair<QShader, QShader> init(RenderList& r) override
  {
    auto& rhi = *r.state.rhi;
    const auto w = decoder.width, h = decoder.height;

    {
      auto tex
          = rhi.newTexture(QRhiTexture::RGBA16F, {w / 2, h}, 1, QRhiTexture::Flag{});
      tex->create();

      auto sampler = rhi.newSampler(
          QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
          QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
      sampler->create();
      samplers.push_back({sampler, tex});
    }

    return score::gfx::makeShaders(
        r.state, vertexShader(),
        QString(frag).arg("").arg(colorMatrix(decoder)));
  }

  void exec(RenderList&, QRhiResourceUpdateBatch& res, AVFrame& frame) override
  {
    const auto w = decoder.width, h = decoder.height;
    auto pixels = frame.data[0];
    auto stride = frame.linesize[0];

    // w/2 macropixels, each 8 bytes (4 x 16-bit)
    QRhiTextureUploadEntry entry{
        0, 0, createTextureUpload(pixels, w / 2, h, 8, stride)};
    res.uploadTexture(samplers[0].texture, {entry});
  }
};

}
