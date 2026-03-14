#pragma once

class QRhi;
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
void insertComputeBarrier(QRhi& rhi, QRhiCommandBuffer& cb);
}
