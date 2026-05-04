#pragma once
#include <score_plugin_gfx_export.h>
class QRhi;
class QRhiBuffer;
class QRhiCommandBuffer;

namespace score::gfx
{
/**
 * @brief Insert a compute-to-compute memory barrier.
 *
 * Ensures that SSBO writes from a preceding compute dispatch are visible
 * to the next compute dispatch.  Must be called between
 * QRhiCommandBuffer::beginExternal() and endExternal(), inside a compute pass.
 *
 * Per-backend behaviour:
 *  - Vulkan : vkCmdPipelineBarrier (COMPUTE → COMPUTE, SHADER_WRITE → SHADER_READ|WRITE)
 *  - OpenGL : glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT)
 *  - D3D12  : UAV barrier on all resources
 *  - D3D11  : no-op (implicit within device context)
 *  - Metal  : no-op (implicit between command encoders; each QRhi compute pass
 *             uses a separate MTLComputeCommandEncoder)
 */
SCORE_PLUGIN_GFX_EXPORT
void insertComputeBarrier(QRhi& rhi, QRhiCommandBuffer& cb);

/**
 * @brief Copy the contents of one GPU buffer to another.
 *
 * Performs a GPU-side buffer-to-buffer copy of @p size bytes from
 * @p src + @p srcOffset to @p dst + @p dstOffset. Both buffers must
 * already be created and large enough to satisfy the requested region.
 * Must be called between beginExternal() and endExternal().
 *
 * The source and destination buffers must NOT overlap (the copy is
 * unordered when src == dst).
 *
 * Per-backend behaviour:
 *  - Vulkan : vkCmdCopyBuffer
 *  - OpenGL : glCopyBufferSubData
 *  - D3D12  : CopyBufferRegion
 *  - D3D11  : CopySubresourceRegion (offsets supported via D3D11_BOX)
 *  - Metal  : MTLBlitCommandEncoder copyFromBuffer
 */
// Controls whether the copy helpers emit their own pre/post pipeline
// barriers. Default: Auto (each call emits a compute→transfer +
// transfer→compute pair). Use `None` when you are batching N calls
// inside explicit beginBufferCopyBarrier / endBufferCopyBarrier brackets
// to avoid N−1 redundant pipeline stalls.
enum class BufferCopyBarrier
{
  Auto,
  None
};

/// Emit the compute→transfer barrier that must precede a buffer copy
/// consuming data written by a compute shader. Pair with
/// endBufferCopyBarrier(). No-op on backends that handle the transition
/// implicitly (D3D11, Metal).
SCORE_PLUGIN_GFX_EXPORT
void beginBufferCopyBarrier(QRhi& rhi, QRhiCommandBuffer& cb);

/// Emit the transfer→compute barrier after a batch of buffer copies so
/// downstream compute/graphics reads observe the writes.
SCORE_PLUGIN_GFX_EXPORT
void endBufferCopyBarrier(QRhi& rhi, QRhiCommandBuffer& cb);

SCORE_PLUGIN_GFX_EXPORT
void copyBuffer(
    QRhi& rhi, QRhiCommandBuffer& cb,
    QRhiBuffer* src, QRhiBuffer* dst, int size,
    int srcOffset = 0, int dstOffset = 0,
    BufferCopyBarrier barrier = BufferCopyBarrier::Auto);

// Metal-specific implementation (defined in RhiBufferCopyMetal.mm)
void copyBufferMetal(
    QRhi& rhi, QRhiCommandBuffer& cb,
    QRhiBuffer* src, QRhiBuffer* dst, int size,
    int srcOffset = 0, int dstOffset = 0);

/**
 * @brief Region-based GPU buffer copy for strided / gather patterns.
 *
 * One src buffer → one dst buffer, with @p count distinct {srcOffset,
 * dstOffset, size} regions. Emits ONE pre-barrier and ONE post-barrier
 * for the whole batch on backends that need them (Vulkan), then issues
 * the minimum native work:
 *   - Vulkan : single vkCmdCopyBuffer call with `count` regions
 *   - OpenGL : N glCopyBufferSubData (bindings reused)
 *   - D3D12  : N CopyBufferRegion (no per-call barriers needed)
 *   - D3D11  : N CopySubresourceRegion
 *   - Metal  : N copyFromBuffer within one MTLBlitCommandEncoder
 *
 * Replaces what would otherwise be N copyBuffer() calls (each with its
 * own barrier pair) for strided source layouts — the
 * std430-vec3-padded-to-vec4 case in particular. Must be called inside
 * beginExternal()/endExternal() like copyBuffer().
 */
struct BufferCopyRegion
{
  int src_offset{};
  int dst_offset{};
  int size{};
};
SCORE_PLUGIN_GFX_EXPORT
void copyBufferRegions(
    QRhi& rhi, QRhiCommandBuffer& cb,
    QRhiBuffer* src, QRhiBuffer* dst,
    const BufferCopyRegion* regions, int count,
    BufferCopyBarrier barrier = BufferCopyBarrier::Auto);

// Metal-specific implementation
void copyBufferRegionsMetal(
    QRhi& rhi, QRhiCommandBuffer& cb,
    QRhiBuffer* src, QRhiBuffer* dst,
    const BufferCopyRegion* regions, int count);
}
