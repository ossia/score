#pragma once
#include <Gfx/Graph/decoders/ColorSpace.hpp>
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>
extern "C" {
#include <libavformat/avformat.h>
}

namespace score::gfx
{
/**
 * @brief Decodes YUYV422 video.
 *
 * Core idea taken from https://gist.github.com/roxlu/7872352
 */
struct YUYV422Decoder : GPUVideoDecoder
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

void main() {
  // YUYV packs Y0 U Y1 V into RGBA as x=Y0 y=U z=Y1 w=V
  // Input texture is half the width of the output (one RGBA texel = 2 pixels)
  float colIndex = floor(v_texcoord.x * mat.texSz.x);
  float oddCol = mod(colIndex, 2.0);

  // Offset by half an input pixel to sample the correct texel center
  vec2 dxInput = 0.5 * vec2(1.0 / mat.texSz.x, 0.0);

  float oddY = texture(u_tex, v_texcoord - dxInput).z;
  float evenY = texture(u_tex, v_texcoord + dxInput).x;
  float y = mix(evenY, oddY, oddCol);

  vec2 uv = texture(u_tex, v_texcoord).yw;

  fragColor = processTexture(vec4(y, uv.x, uv.y, 1.));
}
)_";

  explicit YUYV422Decoder(Video::ImageFormat& d)
      : decoder{d}
  {
  }

  Video::ImageFormat& decoder;
  std::pair<QShader, QShader> init(RenderList& r) override
  {
    auto& rhi = *r.state.rhi;

    const auto w = decoder.width, h = decoder.height;
    // Y
    {
      auto tex = rhi.newTexture(QRhiTexture::RGBA8, {w / 2, h}, 1, QRhiTexture::Flag{});
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
    setYPixels(res, frame.data[0], frame.linesize[0]);
  }

  void
  setYPixels(QRhiResourceUpdateBatch& res, uint8_t* pixels, int stride) const noexcept
  {
    const auto w = decoder.width, h = decoder.height;
    auto y_tex = samplers[0].texture;

    // Texture is RGBA8 at {w/2, h}: 4 bytes per texel = one YUYV macropixel
    QRhiTextureUploadEntry entry{0, 0, createTextureUpload(pixels, w / 2, h, 4, stride)};

    QRhiTextureUploadDescription desc{entry};
    res.uploadTexture(y_tex, desc);
  }
};

/**
 * @brief Decodes UYVY422 video, mostly used for NDI.
 *
 * The code and matrix coefficients are adapted from QtMultimedia.
 */
struct UYVY422Decoder : GPUVideoDecoder
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

void main() {
   // UYVY packs U0 Y0 V0 Y1 into RGBA as x=U0 y=Y0 z=V0 w=Y1
   // Input texture is half the width of the output (one RGBA texel = 2 pixels)
   float colIndex = floor(v_texcoord.x * mat.texSz.x);
   float oddCol = mod(colIndex, 2.0);

   // Offset by half an input pixel to sample the correct texel center
   vec2 dxInput = 0.5 * vec2(1.0 / mat.texSz.x, 0.0);

   float oddY = texture(u_tex, v_texcoord - dxInput).w;
   float evenY = texture(u_tex, v_texcoord + dxInput).y;
   float y = mix(evenY, oddY, oddCol);

   vec2 uv = texture(u_tex, v_texcoord).xz;

  fragColor = processTexture(vec4(y, uv.x, uv.y, 1.));
}
)_";

  UYVY422Decoder(Video::ImageFormat& d)
      : decoder{d}
  {
  }

  Video::ImageFormat& decoder;
  std::pair<QShader, QShader> init(RenderList& r) override
  {
    auto& rhi = *r.state.rhi;

    const auto w = decoder.width, h = decoder.height;
    // Y
    {
      auto tex = rhi.newTexture(QRhiTexture::RGBA8, {w / 2, h}, 1, QRhiTexture::Flag{});
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
    auto y_tex = samplers[0].texture;

    auto pixels = frame.data[0];
    auto stride = frame.linesize[0];
    // Texture is RGBA8 at {w/2, h}: 4 bytes per texel = one UYVY macropixel
    QRhiTextureUploadEntry entry{
        0, 0, createTextureUpload(pixels, frame.width / 2, frame.height, 4, stride)};

    QRhiTextureUploadDescription desc{entry};
    res.uploadTexture(y_tex, desc);
  }
};

}
