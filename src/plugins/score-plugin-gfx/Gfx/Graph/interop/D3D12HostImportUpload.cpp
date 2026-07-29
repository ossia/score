#include "D3D12HostImportUpload.hpp"

#include <QtGui/private/qrhi_p.h>

#include <QDebug>
#include <QtGlobal>

// The D3D12 RHI backend (and qrhid3d12_p.h) arrived in Qt 6.6. This is not a
// QT_CONFIG feature, so it has to be a version check.
#if defined(_WIN32) && QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
#define SCORE_GFX_HAS_D3D12_HOST_IMPORT 1
#include <QtGui/private/qrhid3d12_p.h>

#include <d3d12.h>
#endif

namespace score::gfx::interop
{

#if defined(SCORE_GFX_HAS_D3D12_HOST_IMPORT)

struct D3D12HostImportUpload::Impl
{
  ID3D12Device3* dev{};
  std::vector<ID3D12Heap*> heaps;
  std::size_t rowPitch{};
};

namespace
{
/// The DXGI format the copy footprint must describe. CopyTextureRegion needs
/// the source footprint's format to match the destination resource's; hardcoding
/// RGBA8 made every BGRA8 capture fail (the input stopped locking entirely on
/// D3D12 while the same format passed on Vulkan).
DXGI_FORMAT dxgiFormatOf(QRhiTexture::Format f) noexcept
{
  switch(f)
  {
    case QRhiTexture::BGRA8:
      return DXGI_FORMAT_B8G8R8A8_UNORM;
    case QRhiTexture::R8:
    case QRhiTexture::RED_OR_ALPHA8:
      return DXGI_FORMAT_R8_UNORM;
    case QRhiTexture::RG8:
      return DXGI_FORMAT_R8G8_UNORM;
    case QRhiTexture::R16:
      return DXGI_FORMAT_R16_UNORM;
    case QRhiTexture::RG16:
      return DXGI_FORMAT_R16G16_UNORM;
    case QRhiTexture::RGBA16F:
      return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case QRhiTexture::RGBA32F:
      return DXGI_FORMAT_R32G32B32A32_FLOAT;
    default:
      return DXGI_FORMAT_R8G8B8A8_UNORM;
  }
}

ID3D12Device3* deviceOf(QRhi& rhi) noexcept
{
  if(rhi.backend() != QRhi::D3D12)
    return nullptr;
  const auto* nh = static_cast<const QRhiD3D12NativeHandles*>(rhi.nativeHandles());
  if(!nh || !nh->dev)
    return nullptr;
  // QRhi hands out an ID3D12Device; OpenExistingHeapFromAddress arrived in
  // ID3D12Device3, so it has to be queried for rather than assumed.
  ID3D12Device3* d3 = nullptr;
  auto* base = static_cast<ID3D12Device*>(nh->dev);
  if(FAILED(base->QueryInterface(__uuidof(ID3D12Device3), reinterpret_cast<void**>(&d3))))
    return nullptr;
  // The QueryInterface reference is dropped immediately: QRhi owns the device
  // and outlives this object, so holding a second count would only risk keeping
  // it alive past QRhi's own teardown.
  d3->Release();
  return d3;
}
}

D3D12HostImportUpload::~D3D12HostImportUpload()
{
  release();
}

std::size_t D3D12HostImportUpload::requiredAlignment(QRhi& rhi) noexcept
{
  if(!deviceOf(rhi))
    return 0;
  SYSTEM_INFO si{};
  ::GetSystemInfo(&si);
  return std::size_t(si.dwAllocationGranularity);
}

bool D3D12HostImportUpload::pitchUsable(std::size_t rowPitch) noexcept
{
  if(rowPitch == 0)
    return false;
  if(rowPitch % D3D12_TEXTURE_DATA_PITCH_ALIGNMENT == 0)
    return true;
  // Measured working on AMD's Windows driver, but undocumented: the runtime is
  // free to reject or mis-stride it, so it stays behind an explicit opt-in.
  return qEnvironmentVariableIsSet("SCORE_GFX_D3D12_ALLOW_UNALIGNED_PITCH");
}

bool D3D12HostImportUpload::init(
    QRhi& rhi, const std::vector<void*>& slots, std::size_t bytes,
    std::size_t rowPitch)
{
  release();
  auto* dev = deviceOf(rhi);
  if(!dev || slots.empty() || bytes == 0)
    return false;
  if(!pitchUsable(rowPitch))
  {
    qDebug() << "D3D12 host-import: row pitch" << rowPitch
             << "is not a multiple of" << int(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT)
             << "- refusing the rung (set SCORE_GFX_D3D12_ALLOW_UNALIGNED_PITCH "
                "to use it anyway)";
    return false;
  }

  m_d = new Impl;
  m_d->dev = dev;
  m_d->rowPitch = rowPitch;

  for(void* p : slots)
  {
    ID3D12Heap* heap = nullptr;
    HRESULT hr = dev->OpenExistingHeapFromAddress(
        p, __uuidof(ID3D12Heap), reinterpret_cast<void**>(&heap));
    if(FAILED(hr) || !heap)
    {
      qDebug() << "D3D12 host-import: OpenExistingHeapFromAddress failed"
               << Qt::hex << quint32(hr)
               << "- the slot must come from importableAlloc (VirtualAlloc)";
      release();
      return false;
    }

    D3D12_RESOURCE_DESC bd{};
    bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bd.Width = bytes;
    bd.Height = 1;
    bd.DepthOrArraySize = 1;
    bd.MipLevels = 1;
    bd.Format = DXGI_FORMAT_UNKNOWN;
    bd.SampleDesc.Count = 1;
    bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    // Required: the opened heap is SHARED_CROSS_ADAPTER, and a placed resource
    // that does not opt in is refused with E_INVALIDARG whatever initial state
    // it asks for.
    bd.Flags = D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER;

    ID3D12Resource* buf = nullptr;
    hr = dev->CreatePlacedResource(
        heap, 0, &bd, D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr,
        __uuidof(ID3D12Resource), reinterpret_cast<void**>(&buf));
    if(FAILED(hr) || !buf)
    {
      qDebug() << "D3D12 host-import: CreatePlacedResource failed" << Qt::hex
               << quint32(hr);
      heap->Release();
      release();
      return false;
    }
    m_d->heaps.push_back(heap);
    m_buffers.push_back(buf);
  }
  return true;
}

void D3D12HostImportUpload::release()
{
  for(void* b : m_buffers)
    if(b)
      static_cast<ID3D12Resource*>(b)->Release();
  m_buffers.clear();
  if(m_d)
  {
    for(auto* h : m_d->heaps)
      if(h)
        h->Release();
    delete m_d;
    m_d = nullptr;
  }
}

bool D3D12HostImportUpload::copyToTexture(
    QRhiCommandBuffer& cb, QRhiTexture& tex, std::size_t slot, int width,
    int height, std::size_t srcOffset) noexcept
{
  if(!m_d || slot >= m_buffers.size() || width <= 0 || height <= 0)
    return false;
  auto nt = tex.nativeTexture();
  if(!nt.object)
    return false;
  auto* dstRes = reinterpret_cast<ID3D12Resource*>(nt.object);
  auto* srcRes = static_cast<ID3D12Resource*>(m_buffers[slot]);

  // Hands us the live command list after QRhi flushes its own pending work.
  cb.beginExternal();
  const auto* nh
      = static_cast<const QRhiD3D12CommandBufferNativeHandles*>(cb.nativeHandles());
  if(!nh || !nh->commandList)
  {
    cb.endExternal();
    return false;
  }
  auto* cl = static_cast<ID3D12GraphicsCommandList*>(nh->commandList);

  const auto barrier = [&](D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to) {
    if(from == to)
      return;
    D3D12_RESOURCE_BARRIER b{};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource = dstRes;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b.Transition.StateBefore = from;
    b.Transition.StateAfter = to;
    cl->ResourceBarrier(1, &b);
  };

  // nt.layout carries QRhi's current D3D12_RESOURCE_STATES for this texture.
  const auto before = nt.layout ? D3D12_RESOURCE_STATES(nt.layout)
                                : D3D12_RESOURCE_STATE_COMMON;
  barrier(before, D3D12_RESOURCE_STATE_COPY_DEST);

  D3D12_TEXTURE_COPY_LOCATION src{};
  src.pResource = srcRes;
  src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
  src.PlacedFootprint.Offset = UINT64(srcOffset);
  src.PlacedFootprint.Footprint.Format = dxgiFormatOf(tex.format());
  src.PlacedFootprint.Footprint.Width = UINT(width);
  src.PlacedFootprint.Footprint.Height = UINT(height);
  src.PlacedFootprint.Footprint.Depth = 1;
  src.PlacedFootprint.Footprint.RowPitch = UINT(m_d->rowPitch);

  D3D12_TEXTURE_COPY_LOCATION dst{};
  dst.pResource = dstRes;
  dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  dst.SubresourceIndex = 0;

  cl->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

  barrier(
      D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  cb.endExternal();

  // QRhi tracks the state itself; without this its next barrier would use a
  // stale StateBefore and the runtime would reject the transition.
  tex.setNativeLayout(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  return true;
}

#else // no D3D12

struct D3D12HostImportUpload::Impl
{
};
D3D12HostImportUpload::~D3D12HostImportUpload() = default;
std::size_t D3D12HostImportUpload::requiredAlignment(QRhi&) noexcept
{
  return 0;
}
bool D3D12HostImportUpload::pitchUsable(std::size_t) noexcept
{
  return false;
}
bool D3D12HostImportUpload::init(QRhi&, const std::vector<void*>&, std::size_t, std::size_t)
{
  return false;
}
void D3D12HostImportUpload::release() { }
bool D3D12HostImportUpload::copyToTexture(
    QRhiCommandBuffer&, QRhiTexture&, std::size_t, int, int, std::size_t) noexcept
{
  return false;
}

#endif

} // namespace score::gfx::interop
