#pragma once

/**
 * @file BorrowedHostImportCapture.hpp
 * @brief Zero-copy capture from buffers the producer already owns, via
 *        VK_EXT_external_memory_host.
 *
 * For producers that hand out a fixed set of page-aligned host buffers for the
 * lifetime of a stream and can be told when the renderer has finished with one:
 * V4L2 MMAP (the driver's mmap'd buffers), and a DeckLink allocator provider
 * (buffers we hand the SDK so the card DMAs straight into them).
 *
 * The difference from CpuStagedCapture is where the frame comes from.
 * CpuStagedCapture owns its slots, so the producer copies each frame in;
 * on Vulkan the slot pages are imported once and the *upload* is free, but the
 * producer's memcpy remains. Here the producer's own buffers are the imported
 * pages, so that copy disappears too and the whole path is card -> buffer (DMA)
 * -> texture (DMA) with the CPU touching nothing.
 *
 * The cost is a lifetime contract: the renderer samples producer memory, so a
 * buffer must not be recycled while the GPU may still read it. That is what
 * BorrowedSlotTracker arbitrates -- `ingestFrame` publishes, `takeReturnedSlots`
 * tells the producer which buffers it may reuse, and a producer that ignores
 * the latter will starve its own queue.
 *
 * Two backends can point the GPU at existing host pages:
 * Vulkan through VK_EXT_external_memory_host, and D3D12 through
 * ID3D12Device3::OpenExistingHeapFromAddress. init() picks whichever the live
 * QRhi offers and returns false on the rest -- OpenGL has no equivalent, and
 * D3D11 has no public API to wrap application memory at all -- so the renderer
 * degrades to the CPU rung there.
 *
 * The two disagree on what they will accept, and the stricter of the pair wins:
 * D3D12 needs a 64 KB-granularity VirtualAlloc address (hence importableAlloc,
 * which Vulkan also accepts) and a 256-byte-aligned row pitch.
 */

#include <Gfx/Graph/interop/CaptureStrategyCommon.hpp>
#include <Gfx/Graph/interop/D3D12HostImportUpload.hpp>
#include <Gfx/Graph/interop/VideoCaptureStrategy.hpp>
#include <Gfx/Graph/interop/VkHostImportUpload.hpp>

#include <QtGui/private/qrhi_p.h>

#include <QDebug>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace score::gfx::interop
{

/// One producer-owned frame buffer.
struct BorrowedHostBuffer
{
  void* host{};
  std::size_t bytes{};

  /// Explicit plane layout, when the producer knows it. `planeCount == 0`
  /// derives it assuming tight packing, which is what a V4L2-style producer
  /// wants; an allocator that aligns plane offsets (NvBufSurface rounds each
  /// to 64 KB) must state them or chroma is read from the wrong address.
  std::uint32_t planeCount{0};
  struct Plane
  {
    std::uint32_t offset{};
    std::uint32_t pitch{};
  } planes[3]{};
};

struct BorrowedHostImportCapture final : VideoCaptureStrategy
{
  BorrowedHostImportCapture(
      const char* tag, std::vector<BorrowedHostBuffer> buffers)
      : m_name{std::string(tag ? tag : "?") + "-hostimport-borrowed"}
      , m_buffers{std::move(buffers)}
  {
  }

  const char* name() const noexcept override { return m_name.c_str(); }

  bool init(const VideoCaptureStrategyConfig& c) override
  {
    cfg = c;
    if(!cfg.rhi || !cfg.outputTexture || cfg.frameByteSize == 0)
      return false;

    // Planar layouts upload one plane per texture out of the one contiguous
    // producer buffer. The split comes from the decoder's own plane textures,
    // the same derivation CpuStagedCapture uses, so the rungs cannot disagree
    // about where chroma begins.
    m_planeUploads.clear();
    const bool explicitLayout
        = !m_buffers.empty() && m_buffers[0].planeCount >= cfg.planes.size();
    if(cfg.planes.size() > 1)
    {
      std::size_t off = 0;
      for(std::size_t i = 0; i < cfg.planes.size(); ++i)
      {
        auto* tex = cfg.planes[i];
        if(!tex)
        {
          qDebug() << m_name.c_str() << ": decoder plane texture is null";
          return false;
        }
        const auto psz = tex->pixelSize();
        const auto bytes = std::size_t(psz.width()) * texelBytes(tex->format())
                           * std::size_t(psz.height());
        const std::size_t planeOff
            = explicitLayout ? m_buffers[0].planes[i].offset : off;
        if(planeOff + bytes > cfg.frameByteSize)
        {
          qDebug() << m_name.c_str() << ": plane overruns the frame (" << planeOff
                   << "+" << bytes << ">" << cfg.frameByteSize << ")";
          return false;
        }
        m_planeUploads.push_back(
            PlaneUpload{tex, planeOff, psz.width(), psz.height()});
        off = planeOff + bytes;
      }
    }

    if(m_buffers.size() < 2)
    {
      qDebug() << m_name.c_str() << ": only" << m_buffers.size() << "buffers";
      return false;
    }
    if(m_buffers.size() > BorrowedSlotTracker::kMaxSlots)
      m_buffers.resize(BorrowedSlotTracker::kMaxSlots);

    const std::size_t vkAlign = VkHostImportUpload::requiredAlignment(*cfg.rhi);
    const std::size_t d3dAlign = D3D12HostImportUpload::requiredAlignment(*cfg.rhi);
    const std::size_t align = vkAlign ? vkAlign : d3dAlign;
    if(align == 0)
    {
      qDebug() << m_name.c_str()
               << ": backend has no host-import path (needs Vulkan with "
                  "VK_EXT_external_memory_host, or D3D12)";
      return false;
    }

    std::vector<void*> ptrs;
    ptrs.reserve(m_buffers.size());
    for(const auto& b : m_buffers)
    {
      if(!b.host || b.bytes < cfg.frameByteSize)
      {
        qDebug() << m_name.c_str() << ": buffer of" << b.bytes
                 << "cannot hold a frame of" << cfg.frameByteSize;
        return false;
      }
      if(reinterpret_cast<std::uintptr_t>(b.host) % align != 0)
      {
        qDebug() << m_name.c_str() << ": buffer is not aligned to" << align;
        return false;
      }
      ptrs.push_back(b.host);
    }

    m_tracker.retireDepth
        = static_cast<std::size_t>(cfg.rhi->resourceLimit(QRhi::FramesInFlight))
          + 1u;
    // The producer needs buffers left to fill while the renderer holds its own.
    if(m_tracker.retireDepth + 2u > m_buffers.size())
    {
      qDebug() << m_name.c_str() << ": producer ring of" << m_buffers.size()
               << "is too shallow for" << m_tracker.retireDepth
               << "frames in flight plus the device's own queue";
      return false;
    }

    // The producer's stride; D3D12 builds its copy footprint from it.
    const std::size_t rowPitch
        = cfg.height > 0 ? cfg.frameByteSize / std::size_t(cfg.height) : 0;

    const bool ok = vkAlign ? m_hostImport.init(*cfg.rhi, ptrs, cfg.frameByteSize)
                            : m_d3dImport.init(
                                  *cfg.rhi, ptrs, cfg.frameByteSize, rowPitch);
    if(!ok)
    {
      // Expected on drivers whose mmap does not hand back ordinary pinnable
      // host pages: VK_EXT_external_memory_host can only import memory
      // vkGetMemoryHostPointerPropertiesEXT accepts, and a mapping of the
      // device's own DMA buffers frequently is not that.
      qDebug() << m_name.c_str() << ": the producer's buffers cannot be imported "
                                    "as host memory; staying on the CPU rung";
      m_hostImport.release();
      m_d3dImport.release();
      return false;
    }
    m_tracker.reset();
    qDebug() << m_name.c_str() << ": engaged over" << m_buffers.size()
             << "producer-owned buffers (zero-copy)";
    return true;
  }

  void release() override
  {
    m_hostImport.release();
    m_d3dImport.release();
    m_tracker.reset();
  }

  std::size_t slotCount() const noexcept override { return m_buffers.size(); }

  void* slotBuffer(std::size_t i) const noexcept override
  {
    return i < m_buffers.size() ? m_buffers[i].host : nullptr;
  }

  /// Producer thread. Slot @p i becomes render-owned.
  bool ingestFrame(std::size_t i) override
  {
    if(i >= m_buffers.size())
      return false;
    m_tracker.ingest(i);
    return true;
  }

  /// Producer thread. Bitmask of buffers the renderer has finished with; the
  /// producer must give each of them back to its device and no others.
  std::uint32_t takeReturnedSlots() noexcept { return m_tracker.takeReturned(); }

  QRhiTexture* outputTexture() const noexcept override { return cfg.outputTexture; }

  void acquireForRender() override { }
  void acquireForRender(QRhiResourceUpdateBatch& res) override
  {
    acquireForRender(res, nullptr);
  }

  void acquireForRender(QRhiResourceUpdateBatch&, QRhiCommandBuffer* cb) override
  {
    if(!cb)
      return;
    const int slot = m_tracker.acquire();
    if(slot < 0)
      return;
    const auto s = static_cast<std::size_t>(slot);
    auto upload = [&](QRhiTexture& t, int w, int h, std::size_t off) {
      return m_hostImport.valid() ? m_hostImport.copyToTexture(*cb, t, s, w, h, off)
                                  : m_d3dImport.copyToTexture(*cb, t, s, w, h, off);
    };

    if(!m_planeUploads.empty())
    {
      for(const auto& p : m_planeUploads)
        upload(*p.tex, p.width, p.height, p.offset);
      return;
    }

    const auto sz = cfg.outputTexture->pixelSize();
    upload(*cfg.outputTexture, sz.width(), sz.height(), 0);
  }

  void releaseAfterRender() override { }

private:
  /// Per-plane destination for a planar layout; empty for single-plane, which
  /// keeps the common path a single copy with no indirection.
  struct PlaneUpload
  {
    QRhiTexture* tex{};
    std::size_t offset{}; ///< from the start of the producer buffer
    int width{};
    int height{};
  };

  /// Texel size of the formats a planar decoder allocates. Matches
  /// CpuStagedCapture::texelBytes -- the two must agree or the rungs would
  /// place chroma at different offsets.
  static std::uint32_t texelBytes(QRhiTexture::Format f) noexcept
  {
    switch(f)
    {
      case QRhiTexture::R8:
      case QRhiTexture::RED_OR_ALPHA8:
        return 1;
      case QRhiTexture::RG8:
      case QRhiTexture::R16:
        return 2;
      case QRhiTexture::RG16:
        return 4;
      case QRhiTexture::RGBA8:
      case QRhiTexture::BGRA8:
        return 4;
      default:
        return 4;
    }
  }

  std::vector<PlaneUpload> m_planeUploads;
  std::string m_name;
  std::vector<BorrowedHostBuffer> m_buffers;
  VideoCaptureStrategyConfig cfg{};
  VkHostImportUpload m_hostImport;
  D3D12HostImportUpload m_d3dImport;
  BorrowedSlotTracker m_tracker;
};

} // namespace score::gfx::interop
