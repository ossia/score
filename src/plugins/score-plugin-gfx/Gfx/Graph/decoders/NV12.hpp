#pragma once
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>
extern "C" {
#include <libavformat/avformat.h>
}

namespace score::gfx
{
#include <Gfx/Qt5CompatPush> // clang-format: keep

/**
 * @brief Decodes NV12 videos.
 *
 * Mostly follow the YUV420 shader, things are just laid out differently in memory
 */
struct NV12Decoder : GPUVideoDecoder
{
  static const constexpr auto nv12_filter_prologue = R"_(#version 450

  layout(std140, binding = 0) uniform renderer_t {
  mat4 clipSpaceCorrMatrix;
  vec2 texcoordAdjust;

  vec2 renderSize;
  } renderer;

  layout(binding=3) uniform sampler2D y_tex;
  layout(binding=4) uniform sampler2D uv_tex;

  layout(location = 0) in vec2 v_texcoord;
  layout(location = 0) out vec4 fragColor;

  const vec3 R_cf = vec3(1.164383,  0.000000,  1.596027);
  const vec3 G_cf = vec3(1.164383, -0.391762, -0.812968);
  const vec3 B_cf = vec3(1.164383,  2.017232,  0.000000);
  const vec3 offset = vec3(-0.0625, -0.5, -0.5);

  void main ()
  {
    float y = texture(y_tex, v_texcoord).r;
    float u = texture(uv_tex, v_texcoord).r;
    float v = texture(uv_tex, v_texcoord).a;

)_";

  static const constexpr auto nv12_filter_epilogue = R"_(

    yuv += offset;
    fragColor = vec4(0.0, 0.0, 0.0, 1.0);
    fragColor.r = dot(yuv, R_cf);
    fragColor.g = dot(yuv, G_cf);
    fragColor.b = dot(yuv, B_cf);
  })_";

  Video::ImageFormat& decoder;
  bool nv21{};

  NV12Decoder(Video::ImageFormat& d, bool inverted)
      : decoder{d}
      , nv21{inverted}
  {
  }

  std::pair<QShader, QShader> init(RenderList& r) override
  {
    auto& rhi = *r.state.rhi;
    const auto w = decoder.width, h = decoder.height;

    // Y
    {
      auto tex = rhi.newTexture(QRhiTexture::R8, {w, h}, 1, QRhiTexture::Flag{});
      tex->create();

      auto sampler = rhi.newSampler(
          QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
          QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
      sampler->create();
      samplers.push_back({sampler, tex});
    }

    // UV
    {
      auto tex
          = rhi.newTexture(QRhiTexture::RGBA8, {w / 4, h / 2}, 1, QRhiTexture::Flag{});
      tex->create();

      auto sampler = rhi.newSampler(
          QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
          QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
      sampler->create();
      samplers.push_back({sampler, tex});
    }

    QString frag = nv12_filter_prologue;
    if(nv21)
      frag += "    vec3 yuv = vec3(y, v, u);\n";
    else
      frag += "    vec3 yuv = vec3(y, u, v);\n";
    frag += nv12_filter_epilogue;

    return score::gfx::makeShaders(r.state, vertexShader(), frag);
  }

  void exec(RenderList&, QRhiResourceUpdateBatch& res, AVFrame& frame) override
  {
    setYPixels(res, frame.data[0], frame.linesize[0]);
    setUVPixels(res, frame.data[1], frame.linesize[1]);
  }

  void
  setYPixels(QRhiResourceUpdateBatch& res, uint8_t* pixels, int stride) const noexcept
  {
    const auto w = decoder.width, h = decoder.height;
    auto y_tex = samplers[0].texture;

    QRhiTextureUploadEntry entry{0, 0, createTextureUpload(pixels, w, h, 1, stride)};
    QRhiTextureUploadDescription desc{entry};

    res.uploadTexture(y_tex, desc);
  }

  void
  setUVPixels(QRhiResourceUpdateBatch& res, uint8_t* pixels, int stride) const noexcept
  {
    const auto w = decoder.width / 4, h = decoder.height / 2;
    auto uv_tex = samplers[1].texture;

    QRhiTextureUploadEntry entry{0, 0, createTextureUpload(pixels, w, h, 4, stride)};
    QRhiTextureUploadDescription desc{entry};

    res.uploadTexture(uv_tex, desc);
  }
};

#include <Gfx/Qt5CompatPop> // clang-format: keep
}
