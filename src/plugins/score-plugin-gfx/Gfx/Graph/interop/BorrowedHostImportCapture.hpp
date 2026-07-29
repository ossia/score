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
 * Vulkan only: VK_EXT_external_memory_host is the only portable way to point
 * the GPU at existing host pages. init() returns false on every other backend
 * (and when the extension is missing, or a pointer is not aligned to
 * minImportedHostPointerAlignment), so the renderer degrades to the CPU rung.
 */

#include <Gfx/Graph/interop/CaptureStrategyCommon.hpp>
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

    // One sampled texture, so a multi-plane wire layout cannot be expressed.
    if(cfg.planes.size() > 1)
    {
      qDebug() << m_name.c_str() << ": refusing a" << cfg.planes.size()
               << "plane format";
      return false;
    }

    if(m_buffers.size() < 2)
    {
      qDebug() << m_name.c_str() << ": only" << m_buffers.size() << "buffers";
      return false;
    }
    if(m_buffers.size() > BorrowedSlotTracker::kMaxSlots)
      m_buffers.resize(BorrowedSlotTracker::kMaxSlots);

    const std::size_t align = VkHostImportUpload::requiredAlignment(*cfg.rhi);
    if(align == 0)
    {
      qDebug() << m_name.c_str()
               << ": not Vulkan, or no VK_EXT_external_memory_host";
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

    if(!m_hostImport.init(*cfg.rhi, ptrs, cfg.frameByteSize))
    {
      // Expected on drivers whose mmap does not hand back ordinary pinnable
      // host pages: VK_EXT_external_memory_host can only import memory
      // vkGetMemoryHostPointerPropertiesEXT accepts, and a mapping of the
      // device's own DMA buffers frequently is not that.
      qDebug() << m_name.c_str() << ": the driver's mappings cannot be imported "
                                    "as host memory; staying on the CPU rung";
      m_hostImport.release();
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
    const auto sz = cfg.outputTexture->pixelSize();
    m_hostImport.copyToTexture(
        *cb, *cfg.outputTexture, static_cast<std::size_t>(slot), sz.width(),
        sz.height());
  }

  void releaseAfterRender() override { }

private:
  std::string m_name;
  std::vector<BorrowedHostBuffer> m_buffers;
  VideoCaptureStrategyConfig cfg{};
  VkHostImportUpload m_hostImport;
  BorrowedSlotTracker m_tracker;
};

} // namespace score::gfx::interop
