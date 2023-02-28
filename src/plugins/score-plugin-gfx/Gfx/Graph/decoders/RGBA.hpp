#pragma once
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>
extern "C" {
#include <libavformat/avformat.h>
}

namespace score::gfx
{
#include <Gfx/Qt5CompatPush> // clang-format: keep
struct PackedDecoder : GPUVideoDecoder
{
  static const constexpr auto rgb_filter = R"_(#version 450
    layout(std140, binding = 0) uniform renderer_t {
    mat4 clipSpaceCorrMatrix;
    vec2 texcoordAdjust;

    vec2 renderSize;
    } renderer;

    layout(binding=3) uniform sampler2D y_tex;

    layout(location = 0) in vec2 v_texcoord;
    layout(location = 0) out vec4 fragColor;

    vec4 processTexture(vec4 tex) {
      vec4 processed = tex;
      { %1 }
      return processed;
    }

    void main ()
    {
      fragColor = processTexture(texture(y_tex, v_texcoord));
    })_";

  PackedDecoder(
      QRhiTexture::Format fmt, int bytes_per_pixel, Video::ImageFormat& d,
      QString f = "")
      : format{fmt}
      , bytes_per_pixel{bytes_per_pixel}
      , decoder{d}
      , filter{std::move(f)}
  {
  }
  QRhiTexture::Format format;
  int bytes_per_pixel{}; // bpp/8 !
  Video::ImageFormat& decoder;
  QString filter;

  std::pair<QShader, QShader> init(RenderList& r) override
  {
    auto& rhi = *r.state.rhi;
    const auto w = decoder.width, h = decoder.height;

    {
      // Create a texture
      auto tex = rhi.newTexture(format, QSize{w, h}, 1, QRhiTexture::Flag{});
      tex->create();

      // Create a sampler
      auto sampler = rhi.newSampler(
          QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
          QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
      sampler->create();

      // Store both
      samplers.push_back({sampler, tex});
    }

    return score::gfx::makeShaders(
        r.state, vertexShader(), QString(rgb_filter).arg(filter));
  }

  void exec(RenderList&, QRhiResourceUpdateBatch& res, AVFrame& frame) override
  {
    // Nothing particular, we just upload the whole buffer
    setPixels(res, frame.data[0], frame.linesize[0]);
  }

  void
  setPixels(QRhiResourceUpdateBatch& res, uint8_t* pixels, int stride) const noexcept
  {
    const auto w = decoder.width, h = decoder.height;
    auto y_tex = samplers[0].texture;

    QRhiTextureUploadEntry entry{
        0, 0, createTextureUpload(pixels, w, h, bytes_per_pixel, stride)};

    QRhiTextureUploadDescription desc{entry};
    res.uploadTexture(y_tex, desc);
  }
};

struct PlanarDecoder : GPUVideoDecoder
{
  static const constexpr auto rgb_filter = R"_(#version 450
    layout(std140, binding = 0) uniform renderer_t {
    mat4 clipSpaceCorrMatrix;
    vec2 texcoordAdjust;

    vec2 renderSize;
    } renderer;

    %1
    layout(location = 0) in vec2 v_texcoord;
    layout(location = 0) out vec4 fragColor;

    vec4 processTexture(vec4 tex) {
      vec4 processed = tex;
      { %2 }
      return processed;
    }

    void main ()
    {
      vec4 tex = vec4(1.);
      %3
      fragColor = processTexture(tex);
    })_";

  PlanarDecoder(
      QRhiTexture::Format fmt, int bytes_per_pixel, QString planes,
      Video::ImageFormat& d, QString f = "")
      : format{fmt}
      , bytes_per_pixel{bytes_per_pixel}
      , planes{planes}
      , decoder{d}
      , filter{std::move(f)}
  {
  }
  QRhiTexture::Format format;
  int bytes_per_pixel{}; // bpp/8 !
  QString planes{};
  Video::ImageFormat& decoder;
  QString filter;

  std::pair<QShader, QShader> init(RenderList& r) override
  {
    auto& rhi = *r.state.rhi;
    const auto w = decoder.width, h = decoder.height;

    QString samplers_code;
    QString read_texture_code;

    const int binding_orig = 3;
    for(int i = 0; i < planes.size(); i++)
    {
      samplers_code += QString("    layout(binding=%1) uniform sampler2D t%2;\n")
                           .arg(binding_orig + i)
                           .arg(i);
      read_texture_code += QString("      tex.%1 = texture(t%2, v_texcoord).r;\n")
                               .arg(planes[i])
                               .arg(i);

      // Create a texture
      auto tex = rhi.newTexture(format, QSize{w, h}, 1, QRhiTexture::Flag{});
      tex->create();

      // Create a sampler
      auto sampler = rhi.newSampler(
          QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
          QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
      sampler->create();

      // Store both
      samplers.push_back({sampler, tex});
    }

    return score::gfx::makeShaders(
        r.state, vertexShader(),
        QString(rgb_filter).arg(samplers_code).arg(filter).arg(read_texture_code));
  }

  void exec(RenderList&, QRhiResourceUpdateBatch& res, AVFrame& frame) override
  {
    // Nothing particular, we just upload the whole buffer
    for(int i = 0; i < planes.size(); i++)
    {
      setPixels(res, samplers[i].texture, frame.data[i], frame.linesize[i]);
    }
  }

  void setPixels(
      QRhiResourceUpdateBatch& res, QRhiTexture* tex, uint8_t* pixels,
      int stride) const noexcept
  {
    const auto w = decoder.width, h = decoder.height;

    QRhiTextureUploadEntry entry{
        0, 0, createTextureUpload(pixels, w, h, bytes_per_pixel, stride)};

    QRhiTextureUploadDescription desc{entry};
    res.uploadTexture(tex, desc);
  }
};

struct PackedRectDecoder : GPUVideoDecoder
{
  static const constexpr auto rgb_filter = R"_(#version 450
    layout(std140, binding = 0) uniform renderer_t {
    mat4 clipSpaceCorrMatrix;
    vec2 texcoordAdjust;

    vec2 renderSize;
    } renderer;

    layout(binding=3) uniform sampler2DRect y_tex;

    layout(location = 0) in vec2 v_texcoord;
    layout(location = 0) out vec4 fragColor;

    vec4 processTexture(vec4 tex) {
      vec4 processed = tex;
      { %1 }
      return processed;
    }

    void main ()
    {
      fragColor = processTexture(texture(y_tex, v_texcoord));
    })_";

  static constexpr const char* vertex = R"_(#version 450
layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texcoord;

layout(location = 0) out vec2 v_texcoord;

layout(std140, binding = 0) uniform renderer_t {
  mat4 clipSpaceCorrMatrix;
  vec2 texcoordAdjust;

  vec2 renderSize;
} renderer;

layout(std140, binding = 2) uniform material_t {
  vec2 scale;
  vec2 textureSize;
} mat;

out gl_PerVertex { vec4 gl_Position; };

void main()
{
  v_texcoord = texcoord * mat.textureSize;
  gl_Position = renderer.clipSpaceCorrMatrix * vec4(position.x * mat.scale.x, position.y * mat.scale.y, 0.0, 1.);
}
)_";

  PackedRectDecoder(
      QRhiTexture::Format fmt, int bytes_per_pixel, Video::ImageFormat& d,
      QString f = "")
      : format{fmt}
      , bytes_per_pixel{bytes_per_pixel}
      , decoder{d}
      , filter{std::move(f)}
  {
  }
  QRhiTexture::Format format;
  int bytes_per_pixel{}; // bpp/8 !
  Video::ImageFormat& decoder;
  QString filter;

  std::pair<QShader, QShader> init(RenderList& r) override
  {
    auto& rhi = *r.state.rhi;
    const auto w = decoder.width, h = decoder.height;

    {
      // Create a texture
      auto tex = rhi.newTexture(format, QSize{w, h}, 1, QRhiTexture::Flag{});
      tex->create();

      // Create a sampler
      auto sampler = rhi.newSampler(
          QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
          QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
      sampler->create();

      // Store both
      samplers.push_back({sampler, tex});
    }

    return score::gfx::makeShaders(r.state, vertex, QString(rgb_filter).arg(filter));
  }

  void exec(RenderList&, QRhiResourceUpdateBatch& res, AVFrame& frame) override
  {
    // Nothing particular, we just upload the whole buffer
    setPixels(res, frame.data[0], frame.linesize[0]);
  }

  void
  setPixels(QRhiResourceUpdateBatch& res, uint8_t* pixels, int stride) const noexcept
  {
    const auto w = decoder.width, h = decoder.height;
    auto y_tex = samplers[0].texture;

    QRhiTextureUploadEntry entry{
        0, 0, createTextureUpload(pixels, w, h, bytes_per_pixel, stride)};

    QRhiTextureUploadDescription desc{entry};
    res.uploadTexture(y_tex, desc);
  }
};
#include <Gfx/Qt5CompatPop> // clang-format: keep
}
