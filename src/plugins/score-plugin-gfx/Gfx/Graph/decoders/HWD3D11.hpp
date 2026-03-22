#pragma once
#if defined(_WIN32)

#include <Gfx/Graph/decoders/ColorSpace.hpp>
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>
#include <Gfx/Graph/decoders/NV12.hpp>
#include <Gfx/Graph/decoders/P010.hpp>
#include <Video/GpuFormats.hpp>

#include <QtGui/private/qrhid3d11_p.h>

extern "C" {
#include <libavformat/avformat.h>
#if __has_include(<libavutil/hwcontext_d3d11va.h>)
#include <libavutil/hwcontext_d3d11va.h>
#define SCORE_HAS_D3D11_HWCONTEXT 1
#endif
}

#if defined(SCORE_HAS_D3D11_HWCONTEXT)

#include <d3d11.h>

namespace score::gfx
{

/// Zero-copy D3D11VA decoder.
/// Copies the decoded texture array slice to a standalone NV12 texture via
/// GPU-side CopySubresourceRegion, then wraps it via QRhiTexture::createFrom().
///
/// The R8 SRV views the Y plane and the R8G8 SRV views the UV plane of the
/// NV12 copy texture — D3D11 selects the plane based on the view format.
///
/// Supports semi-planar NV12-family formats output by D3D11VA:
///   NV12 (8-bit), P010 (10-bit), P012 (12-bit), P016 (16-bit).
struct HWD3D11Decoder : GPUVideoDecoder
{
  Video::ImageFormat& decoder;
  PixelFormatInfo m_fmt;

  ID3D11Device* m_dev{};
  ID3D11DeviceContext* m_ctx{};
  ID3D11Texture2D* m_nv12Tex{}; // Single NV12/P010 copy target
  bool m_ready{false};

  static bool isAvailable(QRhi& rhi)
  {
    return rhi.backend() == QRhi::D3D11;
  }

  explicit HWD3D11Decoder(
      Video::ImageFormat& d, QRhi& rhi, PixelFormatInfo fmt)
      : decoder{d}
      , m_fmt{fmt}
  {
    auto* nh = static_cast<const QRhiD3D11NativeHandles*>(rhi.nativeHandles());
    m_dev = static_cast<ID3D11Device*>(nh->dev);
    m_dev->GetImmediateContext(&m_ctx);
  }

  ~HWD3D11Decoder() override
  {
    if(m_nv12Tex)
      m_nv12Tex->Release();
    if(m_ctx)
      m_ctx->Release();
  }

  bool setupTextures()
  {
    const int w = decoder.width, h = decoder.height;
    DXGI_FORMAT nv12Fmt = m_fmt.is10bit() ? DXGI_FORMAT_P010 : DXGI_FORMAT_NV12;

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = w;
    desc.Height = h;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = nv12Fmt;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    if(FAILED(m_dev->CreateTexture2D(&desc, nullptr, &m_nv12Tex)))
    {
      qDebug() << "HWD3D11Decoder: failed to create NV12 copy texture"
               << w << "x" << h << "fmt:" << nv12Fmt;
      return false;
    }

    m_ready = true;
    return true;
  }

  std::pair<QShader, QShader> init(RenderList& r) override
  {
    auto& rhi = *r.state.rhi;
    const int w = decoder.width, h = decoder.height;
    auto texFmt = m_fmt.is10bit() ? QRhiTexture::R16 : QRhiTexture::R8;
    auto uvTexFmt = m_fmt.is10bit() ? QRhiTexture::RG16 : QRhiTexture::RG8;

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
      auto tex = rhi.newTexture(uvTexFmt, {w / 2, h / 2}, 1, QRhiTexture::Flag{});
      tex->create();
      auto sampler = rhi.newSampler(
          QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
          QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
      sampler->create();
      samplers.push_back({sampler, tex});
    }

    setupTextures();

    if(m_fmt.is10bit())
      return score::gfx::makeShaders(
          r.state, vertexShader(),
          QString(P010Decoder::frag).arg("").arg(colorMatrix(decoder)));

    QString frag = NV12Decoder::nv12_filter_prologue;
    frag += "    vec3 yuv = vec3(y, u, v);\n";
    frag += NV12Decoder::nv12_filter_epilogue;
    return score::gfx::makeShaders(
        r.state, vertexShader(), frag.arg("").arg(colorMatrix(decoder)));
  }

  void exec(RenderList& r, QRhiResourceUpdateBatch& res, AVFrame& frame) override
  {
    if(!m_ready || !Video::formatIsHardwareDecoded(
           static_cast<AVPixelFormat>(frame.format)))
      return;

    // D3D11VA: data[0] = ID3D11Texture2D* (array), data[1] = array index
    auto* srcTex = reinterpret_cast<ID3D11Texture2D*>(frame.data[0]);
    auto srcIdx = static_cast<UINT>(reinterpret_cast<intptr_t>(frame.data[1]));
    if(!srcTex)
      return;

    const int w = decoder.width, h = decoder.height;

    // Same-format NV12→NV12 copy using the array index as subresource.
    // This copies both Y and UV planes in one operation (matching FFmpeg's
    // own transfer pattern in hwcontext_d3d11va.c).
    D3D11_BOX box = {0, 0, 0, (UINT)w, (UINT)h, 1};
    m_ctx->CopySubresourceRegion(
        m_nv12Tex, 0, 0, 0, 0,
        srcTex, srcIdx, &box);

    // Wrap the NV12 copy texture into QRhi.
    // D3D11 selects the plane based on the SRV format:
    //   R8/R16   SRV → Y plane (luminance)
    //   R8G8/R16G16 SRV → UV plane (chrominance)
    samplers[0].texture->createFrom(
        QRhiTexture::NativeTexture{quint64(m_nv12Tex), 0});
    samplers[1].texture->createFrom(
        QRhiTexture::NativeTexture{quint64(m_nv12Tex), 0});
  }
};

} // namespace score::gfx

#endif // SCORE_HAS_D3D11_HWCONTEXT
#endif // _WIN32
