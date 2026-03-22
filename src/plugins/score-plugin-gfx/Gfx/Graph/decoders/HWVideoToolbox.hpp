#pragma once
#if defined(__APPLE__)

#include <Gfx/Graph/decoders/ColorSpace.hpp>
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>
#include <Gfx/Graph/decoders/HWVideoToolbox_metal.hpp>
#include <Gfx/Graph/decoders/NV12.hpp>
#include <Gfx/Graph/decoders/P010.hpp>
#include <Video/GpuFormats.hpp>

#include <QtGui/private/qrhi_p.h>
#include <QtGui/private/qrhimetal_p.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
#if __has_include(<libavutil/hwcontext_videotoolbox.h>)
#include <libavutil/hwcontext_videotoolbox.h>
#define SCORE_HAS_VTB_HWCONTEXT 1
#endif
}

#if defined(SCORE_HAS_VTB_HWCONTEXT)

namespace score::gfx
{

// MTLPixelFormat values (avoid Metal header dependency)
enum
{
  ScoreMetalPixelFormatR8Unorm = 10,
  ScoreMetalPixelFormatR16Unorm = 20,
  ScoreMetalPixelFormatRG8Unorm = 30,
  ScoreMetalPixelFormatRG16Unorm = 60,
  ScoreMetalPixelFormatRGBA8Unorm = 70,
  ScoreMetalPixelFormatRGBA16Unorm = 110,
};

/// Fragment shader for packed AYUV/AYUV64 formats.
/// AYUV memory layout: A, Y, Cb, Cr per pixel.
/// When read as RGBA texture: R=A, G=Y, B=Cb(U), A=Cr(V).
static const constexpr auto ayuv_frag = R"_(#version 450

)_" SCORE_GFX_VIDEO_UNIFORMS R"_(

layout(binding=3) uniform sampler2D ayuv_tex;

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
  vec4 tex = texture(ayuv_tex, v_texcoord);
  float y = tex.g;   // Y is in green channel
  float u = tex.b;   // Cb is in blue channel
  float v = tex.a;   // Cr is in alpha channel
  float a = tex.r;   // Alpha is in red channel

  vec4 rgb = processTexture(vec4(y, u, v, 1.));
  fragColor = vec4(rgb.rgb, a);
})_";

/// Zero-copy VideoToolbox decoder for Metal RHI.
/// CVPixelBuffer → CVMetalTextureCache → Metal texture per plane.
///
/// Handles:
/// - Semi-planar (BiPlanar): NV12/P010 (4:2:0), NV16/P210 (4:2:2), NV24/P410/P416 (4:4:4)
/// - Packed AYUV/AYUV64: ProRes 4444 with alpha
struct HWVideoToolboxDecoder : GPUVideoDecoder
{
  Video::ImageFormat& decoder;
  PixelFormatInfo m_fmt;
  void* m_textureCache{}; // CVMetalTextureCacheRef

  // Retained CVMetalTextureRef handles from the current frame.
  // The Metal textures obtained via CVMetalTextureGetTexture are only valid
  // while these refs are alive. Qt's createFrom() does not retain the texture,
  // so we must keep these alive until the next frame replaces them.
  void* m_retainedCvTexY{};
  void* m_retainedCvTexUV{};

  static bool isAvailable(QRhi& rhi)
  {
    return rhi.backend() == QRhi::Metal;
  }

  explicit HWVideoToolboxDecoder(
      Video::ImageFormat& d, QRhi& rhi, PixelFormatInfo fmt)
      : decoder{d}
      , m_fmt{fmt}
  {
    auto* nh = static_cast<const QRhiMetalNativeHandles*>(rhi.nativeHandles());
    if(nh && nh->dev)
      m_textureCache = createMetalTextureCache(nh->dev);
  }

  ~HWVideoToolboxDecoder() override
  {
    releaseRetainedTextures();
    if(m_textureCache)
    {
      releaseMetalTextureCache(m_textureCache);
      m_textureCache = nullptr;
    }
  }

  void releaseRetainedTextures()
  {
    if(m_retainedCvTexY)
    {
      releaseMetalTextureRef(m_retainedCvTexY);
      m_retainedCvTexY = nullptr;
    }
    if(m_retainedCvTexUV)
    {
      releaseMetalTextureRef(m_retainedCvTexUV);
      m_retainedCvTexUV = nullptr;
    }
  }

  std::pair<QShader, QShader> init(RenderList& r) override
  {
    auto& rhi = *r.state.rhi;
    const int w = decoder.width, h = decoder.height;

    if(m_fmt.hasAlpha)
    {
      // Packed AYUV: single RGBA texture.
      // QRhi doesn't have RGBA16 unorm, but createFrom() replaces the native
      // texture — Metal reads the actual MTLPixelFormatRGBA16Unorm format.
      // We use RGBA16F as a size-compatible placeholder (both are 8 bytes/pixel).
      auto qrhiFmt = m_fmt.is10bit() ? QRhiTexture::RGBA16F : QRhiTexture::RGBA8;
      auto tex = rhi.newTexture(qrhiFmt, {w, h}, 1, QRhiTexture::Flag{});
      tex->create();
      auto sampler = rhi.newSampler(
          QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
          QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
      sampler->create();
      samplers.push_back({sampler, tex});

      return score::gfx::makeShaders(
          r.state, vertexShader(),
          QString(ayuv_frag).arg("").arg(colorMatrix(decoder)));
    }

    // Semi-planar (BiPlanar): Y + UV textures.
    auto texFmt = m_fmt.is10bit() ? QRhiTexture::R16 : QRhiTexture::R8;
    auto uvFmt = m_fmt.is10bit() ? QRhiTexture::RG16 : QRhiTexture::RG8;
    const int uvW = w >> m_fmt.log2ChromaW;
    const int uvH = h >> m_fmt.log2ChromaH;

    {
      auto tex = rhi.newTexture(texFmt, {w, h}, 1, QRhiTexture::Flag{});
      tex->create();
      auto sampler = rhi.newSampler(
          QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
          QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
      sampler->create();
      samplers.push_back({sampler, tex});
    }
    {
      auto tex = rhi.newTexture(uvFmt, {uvW, uvH}, 1, QRhiTexture::Flag{});
      tex->create();
      auto sampler = rhi.newSampler(
          QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
          QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
      sampler->create();
      samplers.push_back({sampler, tex});
    }

    if(m_fmt.is10bit())
    {
      return score::gfx::makeShaders(
          r.state, vertexShader(),
          QString(P010Decoder::frag).arg("").arg(colorMatrix(decoder)));
    }

    QString frag = NV12Decoder::nv12_filter_prologue;
    frag += "    vec3 yuv = vec3(y, u, v);\n";
    frag += NV12Decoder::nv12_filter_epilogue;
    return score::gfx::makeShaders(
        r.state, vertexShader(), frag.arg("").arg(colorMatrix(decoder)));
  }

  void exec(RenderList& r, QRhiResourceUpdateBatch& res, AVFrame& frame) override
  {
    if(!m_textureCache)
    {
      failed = true;
      return;
    }
    if(!Video::formatIsHardwareDecoded(static_cast<AVPixelFormat>(frame.format)))
    {
      failed = true;
      return;
    }

    auto pixbuf = (CVPixelBufferRef)frame.data[3];
    if(!pixbuf)
    {
      failed = true;
      return;
    }

    releaseRetainedTextures();

    if(m_fmt.hasAlpha)
    {
      execPackedAYUV(pixbuf);
      return;
    }

    execSemiPlanar(pixbuf);
  }

private:
  void execPackedAYUV(CVPixelBufferRef pixbuf)
  {
    // AYUV/AYUV64: single non-planar CVPixelBuffer → one RGBA texture.
    // planeIndex=0 for non-planar buffers.
    unsigned mtlFmt = m_fmt.is10bit()
        ? ScoreMetalPixelFormatRGBA16Unorm
        : ScoreMetalPixelFormatRGBA8Unorm;

    auto tex = createMetalTextureFromPixelBuffer(
        m_textureCache, pixbuf, 0, mtlFmt);
    if(!tex.mtlTexture)
    {
      failed = true;
      return;
    }

    QSize sz(tex.width, tex.height);
    if(samplers[0].texture->pixelSize() != sz)
      samplers[0].texture->setPixelSize(sz);
    samplers[0].texture->createFrom(
        QRhiTexture::NativeTexture{quint64(tex.mtlTexture), 0});
    m_retainedCvTexY = tex.cvMetalTexture; // reuse Y slot for the single texture
  }

  void execSemiPlanar(CVPixelBufferRef pixbuf)
  {
    if(getPixelBufferPlaneCount(pixbuf) < 2)
    {
      failed = true;
      return;
    }

    unsigned yFmt = m_fmt.is10bit() ? ScoreMetalPixelFormatR16Unorm : ScoreMetalPixelFormatR8Unorm;
    unsigned uvFmt = m_fmt.is10bit() ? ScoreMetalPixelFormatRG16Unorm : ScoreMetalPixelFormatRG8Unorm;

    // Y plane
    auto yTex = createMetalTextureFromPixelBuffer(m_textureCache, pixbuf, 0, yFmt);
    if(!yTex.mtlTexture)
    {
      failed = true;
      return;
    }
    {
      QSize ySize(yTex.width, yTex.height);
      if(samplers[0].texture->pixelSize() != ySize)
        samplers[0].texture->setPixelSize(ySize);
      samplers[0].texture->createFrom(
          QRhiTexture::NativeTexture{quint64(yTex.mtlTexture), 0});
      m_retainedCvTexY = yTex.cvMetalTexture;
    }

    // UV plane
    auto uvTex = createMetalTextureFromPixelBuffer(m_textureCache, pixbuf, 1, uvFmt);
    if(!uvTex.mtlTexture)
    {
      failed = true;
      return;
    }
    {
      QSize uvSize(uvTex.width, uvTex.height);
      if(samplers[1].texture->pixelSize() != uvSize)
        samplers[1].texture->setPixelSize(uvSize);
      samplers[1].texture->createFrom(
          QRhiTexture::NativeTexture{quint64(uvTex.mtlTexture), 0});
      m_retainedCvTexUV = uvTex.cvMetalTexture;
    }
  }
};

} // namespace score::gfx

#endif // SCORE_HAS_VTB_HWCONTEXT
#endif // __APPLE__
