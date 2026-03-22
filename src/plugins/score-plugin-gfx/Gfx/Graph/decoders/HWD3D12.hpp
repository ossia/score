#pragma once
#if defined(_WIN32)

#include <Gfx/Graph/decoders/ColorSpace.hpp>
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>
#include <Gfx/Graph/decoders/NV12.hpp>
#include <Gfx/Graph/decoders/P010.hpp>
#include <Video/GpuFormats.hpp>

#include <QtGui/private/qrhid3d12_p.h>

extern "C" {
#include <libavformat/avformat.h>
#if __has_include(<libavutil/hwcontext_d3d12va.h>)
#include <libavutil/hwcontext_d3d12va.h>
#define SCORE_HAS_D3D12_HWCONTEXT 1
#endif
}

#if defined(SCORE_HAS_D3D12_HWCONTEXT)

#include <d3d12.h>

namespace score::gfx
{

/// Zero-copy D3D12VA decoder.
/// Copies the decoded texture to standalone plane textures via GPU-side
/// CopyTextureRegion, then wraps them via QRhiTexture::createFrom().
///
/// Supports semi-planar NV12-family formats output by D3D12VA:
///   NV12 (8-bit), P010 (10-bit), P012 (12-bit), P016 (16-bit).
struct HWD3D12Decoder : GPUVideoDecoder
{
  Video::ImageFormat& decoder;
  PixelFormatInfo m_fmt;

  ID3D12Device* m_dev{};
  ID3D12CommandQueue* m_cmdQueue{};
  ID3D12CommandAllocator* m_cmdAlloc{};
  ID3D12GraphicsCommandList* m_cmdList{};
  ID3D12Fence* m_fence{};
  HANDLE m_fenceEvent{};
  UINT64 m_fenceValue{0};

  ID3D12Resource* m_yTex{};
  ID3D12Resource* m_uvTex{};
  bool m_ready{false};

  static bool isAvailable(QRhi& rhi)
  {
    return rhi.backend() == QRhi::D3D12;
  }

  explicit HWD3D12Decoder(
      Video::ImageFormat& d, QRhi& rhi, PixelFormatInfo fmt)
      : decoder{d}
      , m_fmt{fmt}
  {
    auto* nh = static_cast<const QRhiD3D12NativeHandles*>(rhi.nativeHandles());
    m_dev = static_cast<ID3D12Device*>(nh->dev);
    m_cmdQueue = static_cast<ID3D12CommandQueue*>(nh->commandQueue);
  }

  ~HWD3D12Decoder() override
  {
    if(m_yTex)
      m_yTex->Release();
    if(m_uvTex)
      m_uvTex->Release();
    if(m_cmdList)
      m_cmdList->Release();
    if(m_cmdAlloc)
      m_cmdAlloc->Release();
    if(m_fence)
      m_fence->Release();
    if(m_fenceEvent)
      CloseHandle(m_fenceEvent);
  }

  bool setupTextures()
  {
    const int w = decoder.width, h = decoder.height;
    DXGI_FORMAT yFmt = m_fmt.is10bit() ? DXGI_FORMAT_R16_UNORM : DXGI_FORMAT_R8_UNORM;
    DXGI_FORMAT uvFmt
        = m_fmt.is10bit() ? DXGI_FORMAT_R16G16_UNORM : DXGI_FORMAT_R8G8_UNORM;

    auto makeTexture = [&](DXGI_FORMAT fmt, UINT tw, UINT th,
                           ID3D12Resource** out) -> bool {
      D3D12_HEAP_PROPERTIES heap{};
      heap.Type = D3D12_HEAP_TYPE_DEFAULT;

      D3D12_RESOURCE_DESC desc{};
      desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
      desc.Width = tw;
      desc.Height = th;
      desc.DepthOrArraySize = 1;
      desc.MipLevels = 1;
      desc.Format = fmt;
      desc.SampleDesc.Count = 1;
      desc.Flags = D3D12_RESOURCE_FLAG_NONE;

      return SUCCEEDED(m_dev->CreateCommittedResource(
          &heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COMMON,
          nullptr, IID_PPV_ARGS(out)));
    };

    if(!makeTexture(yFmt, w, h, &m_yTex))
      return false;
    if(!makeTexture(uvFmt, w / 2, h / 2, &m_uvTex))
      return false;

    // Command allocator + list
    if(FAILED(m_dev->CreateCommandAllocator(
           D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_cmdAlloc))))
      return false;

    if(FAILED(m_dev->CreateCommandList(
           0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_cmdAlloc, nullptr,
           IID_PPV_ARGS(&m_cmdList))))
      return false;

    // Close immediately — we reset before each use
    m_cmdList->Close();

    // Fence for GPU sync
    if(FAILED(m_dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence))))
      return false;

    m_fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if(!m_fenceEvent)
      return false;

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

    // D3D12VA: data[0] = AVD3D12VAFrame*
    auto* d3d12Frame = reinterpret_cast<AVD3D12VAFrame*>(frame.data[0]);
    if(!d3d12Frame || !d3d12Frame->texture)
      return;

    ID3D12Resource* srcTex = d3d12Frame->texture;
    const int w = decoder.width, h = decoder.height;

    // Wait for FFmpeg's decode to complete
    auto& sync = d3d12Frame->sync_ctx;
    if(sync.fence && sync.fence_value > 0)
    {
      if(sync.fence->GetCompletedValue() < sync.fence_value)
      {
        sync.fence->SetEventOnCompletion(sync.fence_value, m_fenceEvent);
        WaitForSingleObject(m_fenceEvent, INFINITE);
      }
    }

    // Reset and record copy commands
    m_cmdAlloc->Reset();
    m_cmdList->Reset(m_cmdAlloc, nullptr);

    // Barriers: dst textures COMMON → COPY_DEST
    D3D12_RESOURCE_BARRIER barriers[2]{};
    barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[0].Transition.pResource = m_yTex;
    barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
    barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
    barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[1].Transition.pResource = m_uvTex;
    barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
    barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
    barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    m_cmdList->ResourceBarrier(2, barriers);

    // Copy Y plane (subresource 0 of NV12/P010 texture)
    {
      D3D12_TEXTURE_COPY_LOCATION dst{};
      dst.pResource = m_yTex;
      dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
      dst.SubresourceIndex = 0;

      D3D12_TEXTURE_COPY_LOCATION src{};
      src.pResource = srcTex;
      src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
      src.SubresourceIndex = 0;

      D3D12_BOX box = {0, 0, 0, (UINT)w, (UINT)h, 1};
      m_cmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, &box);
    }

    // Copy UV plane (subresource 1 of NV12/P010 texture)
    {
      D3D12_TEXTURE_COPY_LOCATION dst{};
      dst.pResource = m_uvTex;
      dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
      dst.SubresourceIndex = 0;

      D3D12_TEXTURE_COPY_LOCATION src{};
      src.pResource = srcTex;
      src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
      src.SubresourceIndex = 1;

      D3D12_BOX box = {0, 0, 0, (UINT)(w / 2), (UINT)(h / 2), 1};
      m_cmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, &box);
    }

    // Barriers: dst textures COPY_DEST → COMMON
    barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
    barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;

    m_cmdList->ResourceBarrier(2, barriers);
    m_cmdList->Close();

    // Execute and wait
    ID3D12CommandList* lists[] = {m_cmdList};
    m_cmdQueue->ExecuteCommandLists(1, lists);

    m_fenceValue++;
    m_cmdQueue->Signal(m_fence, m_fenceValue);
    if(m_fence->GetCompletedValue() < m_fenceValue)
    {
      m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent);
      WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    // Wrap our standalone textures into QRhi
    samplers[0].texture->createFrom(
        QRhiTexture::NativeTexture{quint64(m_yTex), 0});
    samplers[1].texture->createFrom(
        QRhiTexture::NativeTexture{quint64(m_uvTex), 0});
  }
};

} // namespace score::gfx

#endif // SCORE_HAS_D3D12_HWCONTEXT
#endif // _WIN32
