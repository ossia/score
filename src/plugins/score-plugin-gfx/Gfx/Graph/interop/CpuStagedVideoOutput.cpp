#include <Gfx/Graph/interop/CpuStagedVideoOutput.hpp>

#include <Gfx/Graph/RhiTextureReadback.hpp>
#include <Gfx/Graph/encoders/GPUVideoEncoder.hpp>
#include <Gfx/Graph/interop/GpuCapabilities.hpp>
#include <Gfx/Graph/interop/HostPinnedRing.hpp>
#include <Gfx/Graph/interop/VideoPixelFormat.hpp>

#include <QDebug>
#include <QtGui/private/qrhi_p.h>

#include <algorithm>
#include <cstring>
#include <utility>

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

struct CpuStagedVideoOutput::Slot
{
  CpuStagedVideoOutputConfig cfg;
  std::unique_ptr<score::gfx::GPUVideoEncoder> enc[2];
  int encIdx{0};

  std::vector<std::vector<std::uint8_t>> ring;
  int ringIdx{0};

  // Per-encoder page-locked readback pointer for the direct-DMA fast path.
  const void* lockedReadback[2]{nullptr, nullptr};

  // GPU-direct (DVP) download path: when engaged, the encoder output texture is
  // DMA'd straight into this sysmem ring (vendor-registered) and the std::vector
  // ring above is unused. Only set when preferGpuDownload + a GPU-direct backend.
  HostPinnedRing pinnedRing;
  bool dvpMode{false};
  int dvpDownloadFailures{};
  int dvpRingIdx{0};

  // Direct-readback mode: the GPU writes each encoded frame straight into the
  // vendor frame memory cfg.frameMemory.acquire() returns. One ReadbackTarget
  // per distinct vendor allocation, cached for the session (failures cached
  // as nullptr so a bad region is not re-probed every frame).
  bool directReadback{false};
  bool readbackPinAlways{false};
  bool readbackPinUnmet{false};
  bool readbackPinUnavailable{false};
  std::size_t directAlignment{};
  std::size_t directOffsetAlignment{};
  std::vector<std::pair<void*, score::gfx::ReadbackTarget*>> readbackTargets;
  void* pendingDst{};
  score::gfx::ReadbackTarget* pendingTarget{};
  int directFailures{0};

  score::gfx::ReadbackTarget* targetFor(const VendorFrameMemory& mem)
  {
    for(auto& [p, t] : readbackTargets)
      if(p == mem.regionBase)
        return t;
    score::gfx::ReadbackTarget* t = nullptr;
    const auto base = reinterpret_cast<std::uintptr_t>(mem.regionBase);
    const std::size_t offset = static_cast<const char*>(mem.bytes)
                               - static_cast<const char*>(mem.regionBase);
    if(directAlignment != 0 && (base % directAlignment) == 0
       && (mem.regionBytes % directAlignment) == 0
       && offset + cfg.frameByteSize <= mem.regionBytes
       && directOffsetAlignment != 0 && (offset % directOffsetAlignment) == 0)
      t = score::gfx::createReadbackTarget(
          *cfg.rhi, mem.regionBase, mem.regionBytes);
    else
      qDebug() << "CpuStagedVideoOutput: vendor allocation" << mem.regionBase
               << "size" << mem.regionBytes << "frame offset" << offset
               << "misses the" << directAlignment << "/"
               << directOffsetAlignment << "byte alignment contract";
    readbackTargets.emplace_back(mem.regionBase, t);
    return t;
  }
};

CpuStagedVideoOutput::CpuStagedVideoOutput() = default;
CpuStagedVideoOutput::~CpuStagedVideoOutput()
{
  release();
}

bool CpuStagedVideoOutput::init(
    CpuStagedVideoOutputConfig cfg,
    std::unique_ptr<score::gfx::GPUVideoEncoder> enc0,
    std::unique_ptr<score::gfx::GPUVideoEncoder> enc1)
{
  if(!enc0 || !enc1 || cfg.frameByteSize == 0 || cfg.planes.empty())
    return false;

  auto s = std::make_unique<Slot>();
  s->cfg = std::move(cfg);
  s->enc[0] = std::move(enc0);
  s->enc[1] = std::move(enc1);

  // Direct readback into vendor frame memory: the GPU copies the encoded
  // frame straight into the card's own output frame (RhiTextureReadback), so
  // neither the QRhi readback nor the vendor's submit-side staging copy runs.
  // Requires a single-plane encoder whose output is byte-identical to the
  // framestore (tight pitch == card pitch, full size) and a backend that can
  // target caller-owned memory. Probes one vendor frame up front so the mode
  // is decided (and reported) before the first frame.
  // Default to this rung only where the backend imports the destination pages,
  // which is where it measured faster (Vulkan -21..-36%, D3D12 -37%). The
  // copy-based backends were a small regression in every cell measured, on both
  // Linux and Windows, because their fence wait lands on the render thread —
  // so they keep the plain QRhi staging path unless asked otherwise.
  // SCORE_GFX_DIRECT_READBACK=always|import|never overrides ("always" also
  // takes the copy-based backends; "never" is the old kill-switch).
  const auto readbackPolicy
      = qEnvironmentVariable("SCORE_GFX_DIRECT_READBACK").toLower();
  const auto path = score::gfx::readbackPath(*s->cfg.rhi);
  const bool pathWanted
      = readbackPolicy == "never"
              || qEnvironmentVariableIsSet("SCORE_GFX_NO_DIRECT_READBACK")
          ? false
          : readbackPolicy == "always"
                ? path != score::gfx::ReadbackPath::Unsupported
                : path == score::gfx::ReadbackPath::Import;

  bool structuralOk = false;
  if(s->cfg.frameMemory && !s->cfg.customStage && s->cfg.planes.size() == 1
     && s->enc[0]->outputTexture() && s->enc[1]->outputTexture() && pathWanted)
  {
    QRhiTexture* tex = s->enc[0]->outputTexture();
    const QSize ts = tex->pixelSize();
    const std::uint32_t stride = std::uint32_t(ts.width()) * 4u;
    const std::uint32_t texBytes = stride * std::uint32_t(ts.height());
    const std::size_t align
        = score::gfx::readbackHostMemoryAlignment(*s->cfg.rhi);
    if(ts.width() > 0 && ts.height() > 0 && align != 0
       && int(stride) == s->cfg.planes[0].rowBytes
       && texBytes == s->cfg.frameByteSize
       && score::gfx::readbackTextureSupported(
           *s->cfg.rhi, *tex, s->cfg.frameByteSize))
    {
      structuralOk = true;
      s->directAlignment = align;
      s->directOffsetAlignment
          = score::gfx::readbackDstOffsetAlignment(*s->cfg.rhi, *tex);
      if(VendorFrameMemory probe = s->cfg.frameMemory.acquire())
      {
        const bool ok = s->targetFor(probe) != nullptr;
        if(s->cfg.frameMemory.cancel)
          s->cfg.frameMemory.cancel(probe.bytes);
        if(ok)
        {
          s->enc[0]->setReadbackEnabled(false);
          s->enc[1]->setReadbackEnabled(false);
          s->directReadback = true;
        }
        else
        {
          qDebug() << "CpuStagedVideoOutput: vendor frame memory cannot be "
                      "wrapped as a readback target; staying host-staged";
        }
      }
    }
  }

  // Pin accounting: "never" is honoured by construction (pathWanted goes
  // false), so only "always" can go unmet. A structurally-possible rung that
  // failed to engage is a defect (pinUnmet); a rung the geometry/backend rules
  // out here (multi-plane, custom stage, pitch mismatch, no vendor frame
  // memory, unsupported backend) does not exist to engage (pinUnavailable).
  s->readbackPinAlways = readbackPolicy == "always";
  if(s->readbackPinAlways && !s->directReadback)
  {
    if(structuralOk)
      s->readbackPinUnmet = true;
    else
      s->readbackPinUnavailable = true;
    qWarning() << "CpuStagedVideoOutput: SCORE_GFX_DIRECT_READBACK=always was "
                  "NOT honoured -"
               << (structuralOk ? "probe failed" : "rung unavailable here");
  }

  // Opt-in GPU-direct download: DMA the encoder output texture straight to a
  // vendor-registered sysmem ring (DVP/AMD-pinned) instead of the QRhi readback.
  // Engages only when a GPU-direct backend is actually selected; otherwise we
  // fall through to the unchanged CPU staging ring below (no regression).
  if(!s->directReadback && s->cfg.preferGpuDownload && s->cfg.caps
     && s->enc[0]->outputTexture() && s->enc[1]->outputTexture())
  {
    const QSize ts = s->enc[0]->outputTexture()->pixelSize();
    const std::uint32_t stride = std::uint32_t(ts.width()) * 4u;
    const std::uint32_t slotBytes = stride * std::uint32_t(ts.height());
    // The ring is configured to the ENCODER TEXTURE geometry (RGBA8), so
    // HostPinnedRing's DVP registration (which uses cfg.width/height/format for
    // both the sysmem buffer and the source texture) matches the texture. The
    // slot byte size must equal the card framestore size for a 1:1 DMA.
    if(ts.width() > 0 && ts.height() > 0 && slotBytes == s->cfg.frameByteSize)
    {
      HostPinnedRingConfig rc;
      rc.rhi = s->cfg.rhi;
      rc.caps = s->cfg.caps;
      rc.direction = HostPinnedDirection::TextureToBuffer;
      rc.format = VideoPixelFormat::RGBA8;
      rc.width = std::uint32_t(ts.width());
      rc.height = std::uint32_t(ts.height());
      rc.stride = stride;
      rc.slotCount = std::uint32_t(std::max(2, s->cfg.slotCount));
      rc.debugName = "direct-video-output-dvp";

      if(s->pinnedRing.create(rc)
         && s->pinnedRing.backend() != HostPinnedRingBackend::CpuStaging
         && s->pinnedRing.backend() != HostPinnedRingBackend::None)
      {
        bool ok = true;
        for(std::size_t i = 0; i < s->pinnedRing.slotCount(); ++i)
        {
          auto& slot = s->pinnedRing.slot(i);
          if(s->cfg.registrar.registerSlot
             && !s->cfg.registrar.registerSlot(
                 slot.host, std::uint32_t(slot.size)))
          {
            ok = false;
            break;
          }
        }
        // Prove one real download before committing. Registration only says
        // DVP accepted the descriptors; it does not say a transfer completes.
        // This matters more here than anywhere else in the file: entering DVP
        // mode DISABLES the encoder readback, so if downloads then fail there
        // is nothing left to produce a frame -- prepareNextFrame returns
        // nullptr forever and the card is handed nothing. Measured on the
        // Quadro as sent=0 with recv=360 and 313 repeats, which scores
        // minPSNR 99 against a frozen reference and reads as a pass.
        if(ok && !qEnvironmentVariableIsSet("SCORE_GFX_DVP_NO_PROBE"))
        {
          if(auto* probeTex = s->enc[0]->outputTexture();
             !probeTex || !s->pinnedRing.downloadTextureToSlot(probeTex, 0))
          {
            qWarning() << "CpuStagedVideoOutput: DVP download probe failed; "
                          "staying on host staging";
            ok = false;
          }
        }
        if(ok)
        {
          s->enc[0]->setReadbackEnabled(false);
          s->enc[1]->setReadbackEnabled(false);
          s->dvpMode = true;
        }
        else
        {
          for(std::size_t i = 0; i < s->pinnedRing.slotCount(); ++i)
          {
            auto& slot = s->pinnedRing.slot(i);
            if(s->cfg.registrar.releaseSlot)
              s->cfg.registrar.releaseSlot(slot.host, std::uint32_t(slot.size));
          }
          s->pinnedRing.destroy();
        }
      }
      else
      {
        s->pinnedRing.destroy();
      }
    }
  }

  // Allocate and pin the CPU staging ring once (only when not in DVP mode).
  // Pinning (page-locking) keeps the kernel from re-locking pages on every DMA -
  // a real chunk of an 8K frame budget. Addresses are stable for the helper's
  // lifetime.
  if(!s->dvpMode)
  {
    const int slots = s->cfg.slotCount > 0 ? s->cfg.slotCount : 4;
    s->ring.resize(slots);
    for(auto& buf : s->ring)
    {
      buf.assign(s->cfg.frameByteSize, 0);
      if(s->cfg.registrar.registerSlot)
        s->cfg.registrar.registerSlot(buf.data(), s->cfg.frameByteSize);
    }
  }

  m_state = std::move(s);
  return true;
}

bool CpuStagedVideoOutput::valid() const noexcept
{
  return m_state && m_state->enc[0] && m_state->enc[1];
}

void CpuStagedVideoOutput::encodeFrame(QRhiCommandBuffer& cb)
{
  if(!m_state)
    return;
  Slot& s = *m_state;

  if(s.directReadback)
  {
    // The destination must be known at record time: acquire the card frame
    // first, then record encoder pass + GPU copy into it. No free frame =
    // card-side back-pressure: skip the whole frame (prepareNextFrame will
    // return nullptr).
    const VendorFrameMemory mem = s.cfg.frameMemory.acquire();
    if(!mem)
    {
      if(mem.bytes && s.cfg.frameMemory.cancel)
        s.cfg.frameMemory.cancel(mem.bytes);
      return;
    }
    auto* tgt = s.targetFor(mem);
    if(tgt)
    {
      const std::size_t offset = static_cast<const char*>(mem.bytes)
                                 - static_cast<const char*>(mem.regionBase);
      s.enc[s.encIdx]->exec(*s.cfg.rhi, cb);
      if(score::gfx::readbackTextureToHost(
             *s.cfg.rhi, cb, *s.enc[s.encIdx]->outputTexture(), *tgt, offset))
      {
        s.pendingDst = mem.bytes;
        s.pendingTarget = tgt;
        s.directFailures = 0;
        return;
      }
    }
    if(s.cfg.frameMemory.cancel)
      s.cfg.frameMemory.cancel(mem.bytes);
    // A pointer that cannot be wrapped or recorded is not transient; after a
    // few strikes revert to the QRhi readback for the rest of the session.
    if(++s.directFailures >= 3)
    {
      qWarning() << "CpuStagedVideoOutput: direct readback failed repeatedly; "
                    "reverting to host staging";
      s.directReadback = false;
      if(s.readbackPinAlways)
        s.readbackPinUnmet = true;
      s.enc[0]->setReadbackEnabled(true);
      s.enc[1]->setReadbackEnabled(true);
    }
    return;
  }

  s.enc[s.encIdx]->exec(*s.cfg.rhi, cb);
}

void* CpuStagedVideoOutput::prepareNextFrame()
{
  if(!m_state)
    return nullptr;
  Slot& s = *m_state;

  // Consume the current encoder, then flip the double-buffer (unconditionally,
  // so a dropped/empty frame still advances - matches the simple alternation).
  const int cur = s.encIdx;
  s.encIdx ^= 1;

  // Direct readback: the GPU already wrote the card frame during the offscreen
  // frame; endOffscreenFrame() waited for it. finishReadbackToHost is the GL /
  // D3D11 completion copy and a no-op on Vulkan/D3D12.
  if(void* dst = std::exchange(s.pendingDst, nullptr))
  {
    auto* tgt = std::exchange(s.pendingTarget, nullptr);
    if(!score::gfx::finishReadbackToHost(*s.cfg.rhi, *tgt))
    {
      if(s.cfg.frameMemory.cancel)
        s.cfg.frameMemory.cancel(dst);
      return nullptr;
    }
    return dst;
  }
  if(s.directReadback)
    return nullptr;

  // GPU-direct download: DVP-copy the encoder output texture into the next ring
  // slot (synchronous on return) and hand the vendor that slot's host pointer.
  // No QRhi readback was scheduled (skipped in the encoder), so no double xfer.
  if(s.dvpMode)
  {
    QRhiTexture* tex = s.enc[cur]->outputTexture();
    if(!tex)
      return nullptr;
    const std::size_t slotIdx = std::size_t(s.dvpRingIdx);
    s.dvpRingIdx
        = (s.dvpRingIdx + 1) % static_cast<int>(s.pinnedRing.slotCount());
    if(!s.pinnedRing.downloadTextureToSlot(tex, slotIdx))
    {
      // Should be unreachable: the init probe proved a download completes.
      // Log the first few rather than dropping frames silently, which is how
      // this went unnoticed as sent=0.
      if(++s.dvpDownloadFailures <= 3)
        qWarning() << "CpuStagedVideoOutput: DVP download failed mid-run; "
                      "frame dropped";
      return nullptr;
    }
    return s.pinnedRing.slot(slotIdx).host;
  }

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

const char* CpuStagedVideoOutput::stagingMode() const noexcept
{
  if(!m_state)
    return "-";
  if(m_state->directReadback)
    return "direct-readback";
  // Name the rung the ring actually engaged, not the one that was asked for.
  // These differ routinely -- DVP gets picked and demoted, AMD-pinned gets
  // selected instead -- and reporting the intent made two genuinely different
  // paths both read as "cpu-staging", which is exactly the kind of row that
  // cannot be used as evidence.
  switch(m_state->pinnedRing.backend())
  {
    case HostPinnedRingBackend::Dvp:
      return "cpu-staging-dvp";
    case HostPinnedRingBackend::AmdPinned:
      return "cpu-staging-amdpinned";
    case HostPinnedRingBackend::CudaHostReg:
      return "cpu-staging-cudareg";
    default:
      return "cpu-staging";
  }
}

bool CpuStagedVideoOutput::readbackPinUnmet() const noexcept
{
  return m_state && m_state->readbackPinUnmet;
}

bool CpuStagedVideoOutput::readbackPinUnavailable() const noexcept
{
  return m_state && m_state->readbackPinUnavailable;
}

void CpuStagedVideoOutput::release()
{
  if(!m_state)
    return;
  Slot& s = *m_state;

  // Direct-readback teardown: give back a frame acquired but never submitted,
  // then destroy the per-pointer targets (render thread; the last offscreen
  // frame has completed, so the GPU is done with them; the vendor's frame
  // memory itself outlives us - the backend closes after this).
  if(void* dst = std::exchange(s.pendingDst, nullptr))
  {
    s.pendingTarget = nullptr;
    if(s.cfg.frameMemory.cancel)
      s.cfg.frameMemory.cancel(dst);
  }
  for(auto& [p, t] : s.readbackTargets)
    if(t)
      score::gfx::destroyReadbackTarget(t);
  s.readbackTargets.clear();

  // GPU-direct ring: unregister each slot from the vendor, then tear down the
  // ring (DVP download is synchronous, so no pending readback to drain).
  if(s.dvpMode)
  {
    for(std::size_t i = 0; i < s.pinnedRing.slotCount(); ++i)
    {
      auto& slot = s.pinnedRing.slot(i);
      if(s.cfg.registrar.releaseSlot)
        s.cfg.registrar.releaseSlot(slot.host, std::uint32_t(slot.size));
    }
    s.pinnedRing.destroy();
  }

  // Unlock the direct-DMA readback buffers before the encoders (and their
  // QByteArrays) are freed. (No-ops in DVP mode: lockedReadback stays null.)
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
