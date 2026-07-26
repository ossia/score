#pragma once
#include <score_plugin_gfx_export.h>

#include <cstddef>

class QRhi;
class QRhiCommandBuffer;
class QRhiTexture;

namespace score::gfx
{
/**
 * @brief GPU→CPU texture readback into caller-provided host memory.
 *
 * QRhi::readBackTexture always lands in a QByteArray it allocates itself, so
 * handing a frame to an I/O device (DeckLink, AJA, shared memory) costs
 * GPU→staging, staging→QByteArray, then a memcpy into the device's frame.
 * These helpers instead record a copy whose destination is the device's own
 * frame memory:
 *
 *  - Vulkan : dst imported once via VK_EXT_external_memory_host, then
 *             vkCmdCopyImageToBuffer straight into it.
 *  - D3D12  : dst wrapped once via ID3D12Device3::OpenExistingHeapFromAddress
 *             (dst must be a VirtualAlloc / file-mapping region), then
 *             CopyTextureRegion into a buffer placed in that heap.
 *  - OpenGL : dst bound as a GL_AMD_pinned_memory pack buffer when the
 *             extension exists (AMD only), else a PBO + one memcpy in
 *             finishReadbackToHost — still one copy fewer than QRhi's path.
 *  - D3D11  : no API can wrap a host allocation as a GPU-writable resource, so
 *             not zero-copy: a reused staging texture, mapped and copied into
 *             dst in finishReadbackToHost. One copy fewer than QRhi's
 *             readBackTexture (no per-frame QByteArray) but a copy nonetheless.
 *  - Metal  : unsupported, canReadbackToHostMemory returns false.
 *
 * Threading / lifetime:
 *  - All functions must be called on the QRhi's render thread.
 *  - Targets must be destroyed before the QRhi, and only once the GPU is done
 *    with them (e.g. after QRhi::finish()).
 *
 * Completion: readbackTextureToHost only records; the bytes are in dst once
 * the frame's commands have executed (QRhi::finish(), or frame fence). The GL
 * and D3D11 paths additionally need finishReadbackToHost to perform their
 * copy; call it on EVERY backend after the frame completes — it is a no-op on
 * Vulkan/D3D12, and readbackTextureToHost asserts (and warns) if the previous
 * readback was recorded but never finished, so a caller that forgets it fails
 * loudly instead of consuming stale data.
 */
struct ReadbackTarget;

/// Whether this backend can read a texture straight into caller-owned host
/// memory. Stable per QRhi; query once.
SCORE_PLUGIN_GFX_EXPORT
bool canReadbackToHostMemory(QRhi& rhi) noexcept;

/// How the backend reaches caller-owned memory. Vulkan and D3D12 import the
/// pages and the GPU writes them directly; OpenGL and D3D11 have no import and
/// must land in a driver buffer that is then memcpy'd, with the fence wait
/// falling on the render thread.
///
/// This is a performance property, not a capability one, and it decides the
/// default: measured against plain QRhi staging, Import is 21-36% cheaper per
/// frame (AJA/Vulkan, DeckLink/D3D12) while HostCopy is a small regression
/// (OpenGL on both Linux and Windows, up to +12 ms latency in one cell).
enum class ReadbackPath
{
  Unsupported,
  HostCopy, ///< driver buffer + memcpy + fence on the render thread
  Import    ///< caller pages imported; GPU writes them directly
};
ReadbackPath readbackPath(QRhi& rhi) noexcept;

/// Required alignment of `dst` and granularity of `bytes` for
/// createReadbackTarget, or 0 when unsupported. Vulkan: the device's
/// minImportedHostPointerAlignment (4096 on NVIDIA). OpenGL: 4096
/// (GL_AMD_pinned_memory page requirement; the PBO fallback ignores it).
/// D3D12: 65536 (VirtualAlloc allocation granularity). D3D11: 1 (dst is only
/// ever a memcpy destination).
SCORE_PLUGIN_GFX_EXPORT
std::size_t readbackHostMemoryAlignment(QRhi& rhi) noexcept;

/// Whether readbackTextureToHost can handle this texture on this backend
/// (format/size/pitch constraints), for a target of `dstBytes` — the same
/// checks the record path performs, without recording anything. Lets a caller
/// decide at init time instead of discovering a permanent per-frame failure.
SCORE_PLUGIN_GFX_EXPORT
bool readbackTextureSupported(QRhi& rhi, QRhiTexture& src, std::size_t dstBytes) noexcept;

/// Required alignment of the `dstOffset` passed to readbackTextureToHost for
/// a texture of this format (0 = backend unsupported). D3D12: 512
/// (D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT); Vulkan: the texel size and 4;
/// OpenGL: 4; D3D11: 1.
SCORE_PLUGIN_GFX_EXPORT
std::size_t readbackDstOffsetAlignment(QRhi& rhi, QRhiTexture& src) noexcept;

/// Registers `dst` (alignment-aligned, `bytes` rounded up to the alignment
/// and owned by the caller) once. Returns nullptr when the backend or the
/// specific allocation cannot be wrapped — fall back to QRhi::readBackTexture.
/// Never call per frame.
SCORE_PLUGIN_GFX_EXPORT
ReadbackTarget* createReadbackTarget(QRhi& rhi, void* dst, std::size_t bytes);

SCORE_PLUGIN_GFX_EXPORT
void destroyReadbackTarget(ReadbackTarget*);

/// Records the texture→(dst + dstOffset) copy on `cb`. Must be called outside
/// a render pass. The copy is tightly packed rows, texture storage order
/// (top-left origin in score). `dstOffset` lets the copy land on an interior
/// pointer of the registered region (device SDKs often hand out offset frame
/// pointers inside their allocations); it must satisfy
/// readbackDstOffsetAlignment and fit in the target. Returns false when the
/// texture's format/size/offset cannot target this backend's path — nothing
/// is recorded, fall back.
SCORE_PLUGIN_GFX_EXPORT
bool readbackTextureToHost(
    QRhi& rhi, QRhiCommandBuffer& cb, QRhiTexture& src, ReadbackTarget& dst,
    std::size_t dstOffset = 0);

/// Makes the last recorded readback's bytes visible in dst. On the GL paths
/// this waits on a fence (and memcpys from the PBO when not pinned); on D3D11
/// it maps the staging texture and copies it into dst; on Vulkan/D3D12 it is
/// a no-op — the caller's frame synchronization suffices. Must be called
/// after every recorded readback, on every backend.
SCORE_PLUGIN_GFX_EXPORT
bool finishReadbackToHost(QRhi& rhi, ReadbackTarget& dst);
}
