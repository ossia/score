#pragma once
#include <score_plugin_gfx_export.h>

#include <QtCore/qglobal.h>

class QRhi;
class QRhiBuffer;
class QRhiCommandBuffer;
class QRhiResourceUpdateBatch;

namespace score::gfx
{

/**
 * @brief Fill (a sub-range of) a QRhiBuffer with a 4-byte pattern.
 *
 * Replaces the wasteful `std::vector<char> zeros(size, 0); res.uploadStaticBuffer(buf, 0, size, zeros.data());`
 * idiom that pays a per-call zero-vector allocation + a CPU→GPU upload of
 * zero bytes. The new entry points either issue a native GPU-side fill
 * (vkCmdFillBuffer / MTLBlitCommandEncoder fillBuffer:range:value:) or
 * route to QRhi's update batch with a thread-local zero-buffer pool so
 * the zero source bytes are amortised across calls.
 *
 * The motivating bug: Vulkan does NOT initialise VkBuffer memory — the
 * underlying device-memory page contains whatever was there before. For
 * sparse-uploaded SSBOs (RawLight arena, world_transforms, per_draws past
 * drawCount, …), the un-touched bytes get read by shaders and feed
 * garbage into the pipeline. Manifests as "wildly different lighting per
 * resize" because each fresh VkBuffer lands on a different page. The
 * defensive zero-fill via uploadStaticBuffer ships zeros from CPU to GPU
 * — correct but slow; this abstraction picks the right native path.
 *
 * Per-backend behaviour:
 *  - Vulkan : vkCmdFillBuffer (CB variant) — Static buffers only, since
 *             QRhi's setupBuffer adds VK_BUFFER_USAGE_TRANSFER_DST_BIT
 *             only when m_type != Dynamic. Dynamic UBOs fall back to the
 *             update batch path. (See qrhivulkan.cpp QVkBuffer::create.)
 *  - Metal  : id<MTLBlitCommandEncoder> fillBuffer:range:value: (CB variant)
 *  - D3D12  : currently falls back to the update batch (a future
 *             optimisation can use ClearUnorderedAccessViewUint or a
 *             thread-local zero-resource + CopyBufferRegion).
 *  - D3D11  : fall back to the update batch.
 *  - GL/GLES: fall back to the update batch (drivers commonly zero
 *             initialised buffer memory anyway, and GL exposes
 *             glClearBufferSubData on 4.3+ which we don't currently wire).
 *
 * Both variants accept an arbitrary 4-byte @p pattern (replicated across
 * the requested range). Default is 0 — the only pattern any current call
 * site uses. @p offset and @p size MUST be 4-byte aligned (Vulkan
 * vkCmdFillBuffer requires it; the batch fallback is permissive but the
 * abstraction enforces the strict contract for portability).
 */
namespace RhiClearBuffer
{

/// CB-recording variant. Uses native fast paths inside
/// beginExternal()/endExternal() per QRhi convention. Falls back to
/// recording a host-side memset uploaded via a temporary update batch
/// when no native path is available — but the batch variant is the
/// preferred entry point for sites that aren't already inside a render
/// pass and have only a QRhiResourceUpdateBatch in scope.
SCORE_PLUGIN_GFX_EXPORT
void clearBuffer(
    QRhi& rhi,
    QRhiCommandBuffer& cb,
    QRhiBuffer* buf,
    quint32 offset,
    quint32 size,
    quint32 pattern = 0u);

/// Update-batch variant. Routes to QRhi's uploadStaticBuffer (Static
/// buffers) or updateDynamicBuffer (Dynamic UBOs) using a thread-local
/// zero-buffer pool — no per-call zero-vector allocation. This is the
/// drop-in replacement for the existing
/// `std::vector<char> zeros(size, 0); batch.uploadStaticBuffer(...)`
/// pattern.
///
/// @p pattern other than 0 will allocate a small thread-local pattern
/// buffer for the call (uncommon path); 0 hits the fast pool.
SCORE_PLUGIN_GFX_EXPORT
void clearBuffer(
    QRhi& rhi,
    QRhiResourceUpdateBatch& batch,
    QRhiBuffer* buf,
    quint32 offset,
    quint32 size,
    quint32 pattern = 0u);

}  // namespace RhiClearBuffer

// Metal-specific implementation hook (lives in RhiClearBufferMetal.mm).
// On non-Apple platforms a no-op stub is provided in RhiClearBuffer.cpp.
// Returns true on success, false if the native path is unavailable
// (caller should fall back to the batch variant).
bool clearBufferMetal(
    QRhi& rhi,
    QRhiCommandBuffer& cb,
    QRhiBuffer* buf,
    quint32 offset,
    quint32 size,
    quint32 pattern);

}  // namespace score::gfx
