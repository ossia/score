#pragma once
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>
extern "C" {
#include <libavformat/avformat.h>
}

namespace score::gfx
{
#include <Gfx/Qt5CompatPush> // clang-format: keep
/**
 * @brief Decodes YUYV422 video.
 *
 * Core idea taken from https://gist.github.com/roxlu/7872352
 */
struct YUYV422Decoder : GPUVideoDecoder
{
  static const constexpr auto filter = R"_(#version 450

layout(std140, binding = 0) uniform renderer_t {
mat4 clipSpaceCorrMatrix;
vec2 texcoordAdjust;

vec2 renderSize;
} renderer;

layout(binding=3) uniform sampler2D u_tex;

layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;

const vec3 R_cf = vec3(1.164383,  0.000000,  1.596027);
const vec3 G_cf = vec3(1.164383, -0.391762, -0.812968);
const vec3 B_cf = vec3(1.164383,  2.017232,  0.000000);
const vec3 offset = vec3(-0.0625, -0.5, -0.5);

void main() {
  vec4 tex = texture(u_tex, v_texcoord);
  float y = tex.r;
  float u = tex.g - 0.5;
  float v = tex.a - 0.5;
  fragColor.r = y + 1.13983 * v;
  fragColor.g = y - 0.39465 * u - 0.58060 * v;
  fragColor.b = y + 2.03211 * u;
  fragColor.a = 1.0;
}
)_";

  YUYV422Decoder(Video::ImageFormat& d)
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

    return score::gfx::makeShaders(r.state, vertexShader(), filter);
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

    QRhiTextureUploadEntry entry{0, 0, createTextureUpload(pixels, w, h, 2, stride)};

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
  static const constexpr auto filter = R"_(#version 450

layout(std140, binding = 0) uniform renderer_t {
mat4 clipSpaceCorrMatrix;
vec2 texcoordAdjust;

vec2 renderSize;
} renderer;

layout(binding=3) uniform sampler2D u_tex;

layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;

const mat4 bt601 = mat4(
                    1.164f,  1.164f,  1.164f,   0.0f,
                    0.000f, -0.392f,  2.017f,   0.0f,
                    1.596f, -0.813f,  0.000f,   0.0f,
                    -0.8708f, 0.5296f, -1.081f, 1.0f);
const mat4 bt709 = mat4(
                    1.164f,  1.164f,  1.164f,   0.0f,
                    0.000f, -0.534f,  2.115,    0.0f,
                    1.793f, -0.213f,  0.000f,   0.0f,
                    -0.5727f, 0.3007f, -1.1302, 1.0f);

void main() {
   // For U0 Y0 V0 Y1 macropixel, lookup Y0 or Y1 based on whether
   // the original texture x coord is even or odd.
   vec4 uyvy = texture(u_tex, v_texcoord);
   float Y;
   if (fract(floor(v_texcoord.x * renderer.renderSize.x + 0.5) / 2.0) > 0.0)
       Y = uyvy.a;       // odd so choose Y1
   else
       Y = uyvy.g;       // even so choose Y0
   float Cb = uyvy.r;
   float Cr = uyvy.b;

   vec4 color = vec4(Y, Cb, Cr, 1.0);
   fragColor = bt601 * color;
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

    return score::gfx::makeShaders(r.state, vertexShader(), filter);
  }

  void exec(RenderList&, QRhiResourceUpdateBatch& res, AVFrame& frame) override
  {
    auto y_tex = samplers[0].texture;

    auto pixels = frame.data[0];
    auto stride = frame.linesize[0];
    QRhiTextureUploadEntry entry{
        0, 0, createTextureUpload(pixels, frame.width, frame.height, 2, stride)};

    QRhiTextureUploadDescription desc{entry};
    res.uploadTexture(y_tex, desc);
  }
};

#include <Gfx/Qt5CompatPop> // clang-format: keep
}
