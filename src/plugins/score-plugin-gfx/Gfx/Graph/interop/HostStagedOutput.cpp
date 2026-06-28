#include <Gfx/Graph/interop/HostStagedOutput.hpp>

#include <Gfx/Graph/encoders/GPUVideoEncoder.hpp>

#include <QtGui/private/qrhi_p.h>

#include <cstring>

namespace score::gfx::interop
{

// Copy a tightly-packed, top-down readback into a (possibly wider-pitch)
// destination row by row. Single memcpy when the pitches match.
static void copyWithStride(
    const std::uint8_t* src, int srcRowBytes, std::uint8_t* dst, int dstRowBytes,
    int height) noexcept
{
  if(srcRowBytes == dstRowBytes)
  {
    std::memcpy(dst, src, std::size_t(srcRowBytes) * height);
    return;
  }
  const int copy = std::min(srcRowBytes, dstRowBytes);
  for(int y = 0; y < height; ++y)
  {
    std::memcpy(dst + y * dstRowBytes, src + y * srcRowBytes, copy);
    if(dstRowBytes > srcRowBytes)
      std::memset(dst + y * dstRowBytes + srcRowBytes, 0, dstRowBytes - srcRowBytes);
  }
}

struct HostStagedOutput::Slot
{
  HostStagedOutputConfig cfg;
  std::unique_ptr<score::gfx::GPUVideoEncoder> enc[2];
  int encIdx{0};

  std::vector<std::vector<std::uint8_t>> ring;
  int ringIdx{0};

  // Per-encoder page-locked readback pointer for the direct-DMA fast path.
  const void* lockedReadback[2]{nullptr, nullptr};
};

HostStagedOutput::HostStagedOutput() = default;
HostStagedOutput::~HostStagedOutput()
{
  release();
}

bool HostStagedOutput::init(
    HostStagedOutputConfig cfg,
    std::unique_ptr<score::gfx::GPUVideoEncoder> enc0,
    std::unique_ptr<score::gfx::GPUVideoEncoder> enc1)
{
  if(!enc0 || !enc1 || cfg.frameByteSize == 0 || cfg.planes.empty())
    return false;

  auto s = std::make_unique<Slot>();
  s->cfg = std::move(cfg);
  s->enc[0] = std::move(enc0);
  s->enc[1] = std::move(enc1);

  // Allocate and pin the staging ring once. Pinning (page-locking) keeps the
  // kernel from re-locking pages on every DMA - a real chunk of an 8K frame
  // budget. Addresses are stable for the helper's lifetime.
  const int slots = s->cfg.slotCount > 0 ? s->cfg.slotCount : 4;
  s->ring.resize(slots);
  for(auto& buf : s->ring)
  {
    buf.assign(s->cfg.frameByteSize, 0);
    if(s->cfg.registrar.registerSlot)
      s->cfg.registrar.registerSlot(buf.data(), s->cfg.frameByteSize);
  }

  m_state = std::move(s);
  return true;
}

bool HostStagedOutput::valid() const noexcept
{
  return m_state && m_state->enc[0] && m_state->enc[1];
}

void HostStagedOutput::encodeFrame(QRhiCommandBuffer& cb)
{
  if(!m_state)
    return;
  m_state->enc[m_state->encIdx]->exec(*m_state->cfg.rhi, cb);
}

void* HostStagedOutput::prepareNextFrame()
{
  if(!m_state)
    return nullptr;
  Slot& s = *m_state;

  // Consume the current encoder, then flip the double-buffer (unconditionally,
  // so a dropped/empty frame still advances - matches the simple alternation).
  const int cur = s.encIdx;
  s.encIdx ^= 1;

  auto& enc = *s.enc[cur];
  const QRhiReadbackResult& rb = enc.readback(0);
  if(rb.data.isEmpty() || rb.pixelSize.isEmpty())
    return nullptr;

  const auto* src = reinterpret_cast<const std::uint8_t*>(rb.data.constData());
  const int srcW = rb.pixelSize.width();
  const int srcH = rb.pixelSize.height();
  const int srcRowBytes = srcW * 4;
  const int dstRowBytes = s.cfg.planes[0].rowBytes;
  const int rows = std::min(srcH, s.cfg.visibleRows);

  // Fast path: single-plane readback already byte-identical to the framestore
  // -> page-lock it once and DMA straight from it (skip the ring memcpy).
  if(s.cfg.directDmaEnabled && enc.planeCount() == 1
     && srcRowBytes == dstRowBytes
     && rb.data.size() == static_cast<int>(s.cfg.frameByteSize))
  {
    const void* p = rb.data.constData();
    const void*& locked = s.lockedReadback[cur & 1];
    if(locked != p)
    {
      if(locked && s.cfg.registrar.releaseSlot)
        s.cfg.registrar.releaseSlot(const_cast<void*>(locked), s.cfg.frameByteSize);
      if(s.cfg.registrar.registerSlot)
        s.cfg.registrar.registerSlot(const_cast<void*>(p), s.cfg.frameByteSize);
      locked = p;
    }
    return const_cast<void*>(p);
  }

  // Round-robin into the next staging slot so the consumer never reads a buffer
  // the producer is about to overwrite.
  std::uint8_t* const dst = s.ring[s.ringIdx].data();
  s.ringIdx = (s.ringIdx + 1) % static_cast<int>(s.ring.size());

  // Planar: copy each plane at its contiguous framestore offset.
  if(enc.planeCount() > 1)
  {
    std::size_t dstOffset = 0;
    for(int p = 0; p < enc.planeCount(); ++p)
    {
      const QRhiReadbackResult& prb = enc.readback(p);
      if(prb.data.isEmpty() || prb.pixelSize.height() <= 0)
        return nullptr;
      const auto* psrc = reinterpret_cast<const std::uint8_t*>(prb.data.constData());
      const int planeRows = prb.pixelSize.height();
      const int planeSrcRow = static_cast<int>(prb.data.size() / planeRows);
      const auto& plane
          = (p < int(s.cfg.planes.size())) ? s.cfg.planes[p] : s.cfg.planes.back();
      copyWithStride(psrc, planeSrcRow, dst + dstOffset, plane.rowBytes, planeRows);
      dstOffset += plane.rasterBytes;
    }
    return dst;
  }

  // Single plane: vendor-specific staging first (e.g. v210 CPU pack), else a
  // plain row-stride copy.
  if(s.cfg.customStage && s.cfg.customStage(src, srcRowBytes, dst, dstRowBytes, rows))
    return dst;

  copyWithStride(src, srcRowBytes, dst, dstRowBytes, rows);
  return dst;
}

void HostStagedOutput::release()
{
  if(!m_state)
    return;
  Slot& s = *m_state;

  // Unlock the direct-DMA readback buffers before the encoders (and their
  // QByteArrays) are freed.
  for(auto& locked : s.lockedReadback)
  {
    if(locked && s.cfg.registrar.releaseSlot)
      s.cfg.registrar.releaseSlot(const_cast<void*>(locked), s.cfg.frameByteSize);
    locked = nullptr;
  }

  // Unpin and free the ring.
  for(auto& buf : s.ring)
  {
    if(!buf.empty() && s.cfg.registrar.releaseSlot)
      s.cfg.registrar.releaseSlot(buf.data(), s.cfg.frameByteSize);
  }
  s.ring.clear();

  for(auto& e : s.enc)
  {
    if(e)
    {
      e->release();
      e.reset();
    }
  }

  m_state.reset();
}

} // namespace score::gfx::interop
