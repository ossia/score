#pragma once
#include <Gfx/Graph/encoders/ColorSpaceOut.hpp>

#include <cstdint>

class QRhi;
class QRhiTexture;
class QRhiBuffer;
class QRhiCommandBuffer;
class QRhiResourceUpdateBatch;

namespace score::gfx
{
struct RenderState;

/**
 * @brief Base interface for compute-shader RGBA -> AJA-format encoders.
 *
 * Unlike the fragment GPUVideoEncoder family which writes into textures
 * (and is consumed by readback), these encoders write directly to a
 * caller-owned QRhiBuffer (Storage). Designed for the AJA tier-3 path:
 * the buffer is backed by a SHARED, CUDA-importable, AJA-DMA-locked GPU
 * allocation, so the encoder's writes ARE the bytes AJA P2P-DMAs to the
 * SDI card.
 *
 * Concrete encoders: V210Compute, UYVYCompute, BGRACompute.
 *
 * Lifecycle:
 *   1. init(rhi, state, src, w, h, outBuf, colorConv): create the
 *      compute pipeline + bind outBuf as the output SSBO.
 *   2. exec(rhi, cb, res): inside an offscreen frame, update the params
 *      UBO via res and dispatch the compute pass on cb.
 *   3. release(): free GPU resources.
 */
struct ComputeEncoder
{
  virtual ~ComputeEncoder() = default;

  virtual bool init(
      QRhi& rhi, const RenderState& state, QRhiTexture* src, int width,
      int height, QRhiBuffer* outputBuffer,
      const QString& colorConversion = colorMatrixOut())
      = 0;

  virtual void exec(
      QRhi& rhi, QRhiCommandBuffer& cb, QRhiResourceUpdateBatch* res)
      = 0;

  virtual void release() = 0;
};

} // namespace score::gfx
