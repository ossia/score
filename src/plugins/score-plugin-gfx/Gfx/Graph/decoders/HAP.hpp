#pragma once
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>

#include <hap/source/hap.h>

#include <snappy.h>

extern "C" {
#include <libavformat/avformat.h>
}

namespace score::gfx
{
#include <Gfx/Qt5CompatPush> // clang-format: keep
/**
 * @brief Base class for HAP ((c) Vidvox) decoding
 */
struct HAPDecoder : GPUVideoDecoder
{
  struct HAPSection
  {
    static HAPSection read(const uint8_t* bytes);

    uint32_t type{};
    uint32_t size{};
    const uint8_t* data{};
  };

  void exec(RenderList&, QRhiResourceUpdateBatch& res, AVFrame& frame) override;
  void setPixels_noEncoding(
      QRhiResourceUpdateBatch& res, const uint8_t* data_start, std::size_t size);
  void setPixels_snappy(
      QRhiResourceUpdateBatch& res, const uint8_t* data_start, std::size_t size);

  static constexpr int buffer_size = 1024 * 1024 * 16;
  std::unique_ptr<char[]> m_buffer = std::make_unique<char[]>(1024 * 1024 * 16);
};

/**
 * @brief Decodes HAP basic format.
 */
struct HAPDefaultDecoder : HAPDecoder
{
  static inline const QString fragment = QStringLiteral(R"_(#version 450
layout(std140, binding = 0) uniform renderer_t {
mat4 clipSpaceCorrMatrix;
vec2 texcoordAdjust;

vec2 renderSize;
} renderer;

layout(binding=3) uniform sampler2D y_tex;

layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;

vec4 processYCoCg(vec4 CoCgSY) {
  const vec4 offsets = vec4(-0.50196078431373, -0.50196078431373, 0.0, 0.0);
  CoCgSY += offsets;
  float scale = ( CoCgSY.z * ( 255.0 / 8.0 ) ) + 1.0;

  float Co = CoCgSY.x / scale;
  float Cg = CoCgSY.y / scale;
  float Y = CoCgSY.w;

  vec4 rgba = vec4(Y + Co - Cg, Y + Cg, Y - Co - Cg, 1.0);
  return rgba;
}

vec4 processTexture(vec4 tex) {
  vec4 processed = tex;
  { %1 }
  return processed;
}

void main ()
{
  fragColor = processTexture(texture(y_tex, v_texcoord));
})_");

  static inline const QString ycocg_filter
      = QStringLiteral("processed = processYCoCg(processed);\n");

  HAPDefaultDecoder(QRhiTexture::Format fmt, Video::ImageFormat& d, QString f = "");
  QRhiTexture::Format format;
  Video::ImageFormat& decoder;
  QString filter;

  std::pair<QShader, QShader> init(RenderList& r) override;
};

/**
 * @brief Decodes HAP-M (HAP + alpha channel)
 */
struct HAPMDecoder : HAPDecoder
{
  static inline const QString fragment = QStringLiteral(R"_(#version 450
layout(std140, binding = 0) uniform renderer_t {
mat4 clipSpaceCorrMatrix;
vec2 texcoordAdjust;

vec2 renderSize;
} renderer;

layout(binding=3) uniform sampler2D y_tex;
layout(binding=4) uniform sampler2D alpha_tex;

layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;

vec4 processYCoCg(vec4 CoCgSY, vec4 alpha) {
  const vec4 offsets = vec4(-0.50196078431373, -0.50196078431373, 0.0, 0.0);
  CoCgSY += offsets;
  float scale = ( CoCgSY.z * ( 255.0 / 8.0 ) ) + 1.0;

  float Co = CoCgSY.x / scale;
  float Cg = CoCgSY.y / scale;
  float Y = CoCgSY.w;

  vec4 rgba = vec4(Y + Co - Cg, Y + Cg, Y - Co - Cg, alpha.r);
  return rgba;
}

vec4 processTexture(vec4 tex) {
  vec4 processed = tex;
  { %1 }
  return processed;
}

void main ()
{
  vec4 ycocg = texture(y_tex, v_texcoord);
  vec4 alpha = texture(alpha_tex, v_texcoord);
  fragColor = processTexture(processYCoCg(ycocg, alpha));
})_");

  HAPMDecoder(Video::ImageFormat& d, QString f = "");
  Video::ImageFormat& decoder;
  QString filter;
  std::pair<QShader, QShader> init(RenderList& r) override;

  void exec(RenderList&, QRhiResourceUpdateBatch& res, AVFrame& frame) override;

  static void setPixels(
      QRhiResourceUpdateBatch& res, QRhiTexture* tex, const uint8_t* ycocg_start,
      std::size_t ycocg_size);

  std::unique_ptr<char[]> m_alphaBuffer = std::make_unique<char[]>(1024 * 1024 * 16);
};

#include <Gfx/Qt5CompatPop> // clang-format: keep
}
