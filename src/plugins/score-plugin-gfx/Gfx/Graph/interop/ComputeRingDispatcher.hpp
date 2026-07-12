#pragma once

/**
 * @file ComputeRingDispatcher.hpp
 * @brief Bundles a ImportedGpuBufferRing + per-slot ComputeEncoder + InteropFence
 *        into the per-frame "encode + signal" loop every vendor GPU-direct
 *        path runs.
 *
 * Captures the duplicated `slot.encoder->exec(*rhi, cb, res)` + ring
 * rotation pattern shared by the per-backend RDMA output paths,
 * plus the fence signal that the Vulkan/D3D12 paths will need.
 *
 * Header-only — there's no per-backend logic here, just orchestration.
 *
 * Usage:
 * @code
 *   ComputeRingDispatcher disp;
 *   if(!disp.init({rhi, state, ring, srcTexture, w, h,
 *                  []{ return makeMyEncoder(...); },
 *                  colorShader, fence.get()}))
 *     return false;
 *
 *   // Per frame:
 *   disp.encode(cb, ++fenceValue);
 *   //  ... offscreen frame ended ...
 *   if(disp.waitOnCuda(fenceValue))
 *     vendor.submit(disp.finishedSlot().gpuDevicePtr);
 *   disp.advance();
 * @endcode
 */

#include <Gfx/Graph/encoders/ComputeEncoder.hpp>
#include <Gfx/Graph/interop/ImportedGpuBufferRing.hpp>
#include <Gfx/Graph/interop/InteropFence.hpp>

#include <functional>
#include <memory>
#include <vector>

class QRhiTexture;

namespace score::gfx
{
struct RenderState;
}

namespace score::gfx::interop
{

struct ComputeRingDispatcherConfig
{
  QRhi* rhi{};
  const score::gfx::RenderState* state{};
  ImportedGpuBufferRing* ring{};
  QRhiTexture* sourceTexture{};
  int width{};
  int height{};
  /// Factory called once per slot. Each slot gets its own encoder so the
  /// pipeline + SRB are bound to that slot's buffer.
  std::function<std::unique_ptr<score::gfx::ComputeEncoder>()> encoderFactory;
  /// Color-conversion GLSL snippet (see encoders::colorMatrixOut). Empty
  /// string means the encoder factory picks its own default.
  QString colorConversion;
  /// Optional. nullptr means no cross-API fence (D3D11/GL no-op style).
  InteropFence* fence{};
};

class ComputeRingDispatcher
{
public:
  ComputeRingDispatcher() = default;
  ~ComputeRingDispatcher() { release(); }

  ComputeRingDispatcher(const ComputeRingDispatcher&) = delete;
  ComputeRingDispatcher& operator=(const ComputeRingDispatcher&) = delete;

  bool init(const ComputeRingDispatcherConfig& cfg)
  {
    if(!cfg.rhi || !cfg.state || !cfg.ring || !cfg.ring->valid()
       || !cfg.sourceTexture || !cfg.encoderFactory)
      return false;
    release();
    m_cfg = cfg;
    m_encoders.resize(cfg.ring->slotCount());
    for(std::size_t i = 0; i < m_encoders.size(); ++i)
    {
      m_encoders[i] = cfg.encoderFactory();
      if(!m_encoders[i])
        return false;
      if(!m_encoders[i]->init(
             *cfg.rhi, *cfg.state, cfg.sourceTexture, cfg.width, cfg.height,
             cfg.ring->slot(i).qrhiBuffer, cfg.colorConversion))
        return false;
    }
    return true;
  }

  void release()
  {
    for(auto& e : m_encoders)
    {
      if(e)
      {
        e->release();
        e.reset();
      }
    }
    m_encoders.clear();
    m_cfg = {};
  }

  /// Inside the offscreen frame: dispatch the compute encoder for the
  /// current write slot, then signal the fence at `fenceValue`.
  void encode(QRhiCommandBuffer& cb, std::uint64_t fenceValue)
  {
    if(!m_cfg.ring || !m_cfg.rhi)
      return;
    auto* res = m_cfg.rhi->nextResourceUpdateBatch();
    const auto idx = m_cfg.ring->writeIndex();
    if(idx < m_encoders.size() && m_encoders[idx])
      m_encoders[idx]->exec(*m_cfg.rhi, cb, res);
    if(m_cfg.fence)
      m_cfg.fence->signalAfterEncode(cb, fenceValue);
  }

  /// After the offscreen frame: wait on the fence (CPU-side / CUDA-side)
  /// then return true if the just-encoded slot is safe to read.
  bool waitOnCuda(std::uint64_t fenceValue)
  {
    if(m_cfg.fence)
      return m_cfg.fence->waitOnCuda(fenceValue);
    return true; // no fence configured — caller assumes implicit ordering
  }

  /// The slot whose contents are now visible to the peer device. Call
  /// before advance() — after advance() this points at a *future* slot.
  GpuRingBufferSlot& finishedSlot() noexcept
  {
    return m_cfg.ring->slot(m_cfg.ring->writeIndex());
  }

  /// Rotate the ring. Call after the peer has accepted finishedSlot().
  void advance() noexcept
  {
    if(m_cfg.ring)
      m_cfg.ring->advance();
  }

private:
  ComputeRingDispatcherConfig m_cfg{};
  std::vector<std::unique_ptr<score::gfx::ComputeEncoder>> m_encoders;
};

} // namespace score::gfx::interop
