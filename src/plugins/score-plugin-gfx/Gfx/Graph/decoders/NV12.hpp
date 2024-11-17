#pragma once
#include <Gfx/Graph/decoders/ColorSpace.hpp>
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>
extern "C" {
#include <libavformat/avformat.h>
}

namespace score::gfx
{

/**
 * @brief Decodes NV12 videos.
 *
 * Mostly follow the YUV420 shader, things are just laid out differently in memory
 */
struct NV12Decoder : GPUVideoDecoder
{
  static const constexpr auto nv12_filter_prologue = R"_(#version 450

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
  float v = texture(uv_tex, v_texcoord).a;
)_";

  static const constexpr auto nv12_filter_epilogue = R"_(
  fragColor = processTexture(vec3(yuv, 1.));
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

    return score::gfx::makeShaders(
        r.state, vertexShader(), QString(frag).arg("").arg(colorMatrix(decoder)));
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

}
