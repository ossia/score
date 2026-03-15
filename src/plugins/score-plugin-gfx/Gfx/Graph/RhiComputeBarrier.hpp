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
 * Performs a GPU-side buffer-to-buffer copy of @p size bytes.
 * Both buffers must already be created and large enough.
 * Must be called between beginExternal() and endExternal().
 *
 * Per-backend behaviour:
 *  - Vulkan : vkCmdCopyBuffer
 *  - OpenGL : glCopyBufferSubData
 *  - D3D12  : CopyBufferRegion
 *  - D3D11  : CopySubresourceRegion
 *  - Metal  : MTLBlitCommandEncoder copyFromBuffer
 */
SCORE_PLUGIN_GFX_EXPORT
void copyBuffer(
    QRhi& rhi, QRhiCommandBuffer& cb,
    QRhiBuffer* src, QRhiBuffer* dst, int size);

// Metal-specific implementation (defined in RhiBufferCopyMetal.mm)
void copyBufferMetal(
    QRhi& rhi, QRhiCommandBuffer& cb,
    QRhiBuffer* src, QRhiBuffer* dst, int size);
}
