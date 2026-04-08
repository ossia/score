#include <Gfx/Graph/RhiComputeBarrier.hpp>

#include <QtGui/private/qrhi_p.h>
#if __has_include(<QtGui/private/qrhigles2_p.h>)
#include <QtGui/private/qrhigles2_p.h>
#endif

// On non-Apple, provide a no-op stub for copyBufferMetal
// (the real implementation lives in RhiBufferCopyMetal.mm)
#if !defined(Q_OS_MACOS) && !defined(Q_OS_IOS)
namespace score::gfx
{
void copyBufferMetal(
    QRhi&, QRhiCommandBuffer&, QRhiBuffer*, QRhiBuffer*, int, int, int)
{
}
}
#endif

// Vulkan
#if QT_HAS_VULKAN || (QT_CONFIG(vulkan) && __has_include(<vulkan/vulkan.h>))
#include <score/gfx/Vulkan.hpp>
#if __has_include(<QtGui/rhi/qrhi_platform.h>)
#include <QtGui/rhi/qrhi_platform.h>
#else
#include <QtGui/private/qrhivulkan_p.h>
#endif
#include <QVulkanInstance>
#define SCORE_HAS_VULKAN 1
#endif

// OpenGL
#if QT_CONFIG(opengl)
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#if __has_include(<QtGui/rhi/qrhi_platform.h>)
#include <QtGui/rhi/qrhi_platform.h>
#endif

#ifndef GL_SHADER_STORAGE_BARRIER_BIT
#define GL_SHADER_STORAGE_BARRIER_BIT 0x00002000
#endif
#ifndef GL_BUFFER_UPDATE_BARRIER_BIT
#define GL_BUFFER_UPDATE_BARRIER_BIT 0x00000200
#endif
#ifndef GL_COPY_READ_BUFFER
#define GL_COPY_READ_BUFFER 0x8F36
#endif
#ifndef GL_COPY_WRITE_BUFFER
#define GL_COPY_WRITE_BUFFER 0x8F37
#endif
#define SCORE_HAS_GL 1
#endif

// D3D12 / D3D11
#if defined(Q_OS_WIN)
#include <d3d11.h>
#include <d3d12.h>
#if __has_include(<QtGui/rhi/qrhi_platform.h>)
#include <QtGui/rhi/qrhi_platform.h>
#endif
#define SCORE_HAS_D3D 1
#endif

namespace score::gfx
{

void insertComputeBarrier(QRhi& rhi, QRhiCommandBuffer& cb)
{
  switch(rhi.backend())
  {
#if SCORE_HAS_VULKAN
    case QRhi::Vulkan: {
      auto* inst = score::gfx::staticVulkanInstance();
      if(!inst)
        break;

      auto fn = reinterpret_cast<PFN_vkCmdPipelineBarrier>(
          inst->getInstanceProcAddr("vkCmdPipelineBarrier"));
      if(!fn)
        break;

      auto* native
          = static_cast<const QRhiVulkanCommandBufferNativeHandles*>(cb.nativeHandles());
      if(!native || !native->commandBuffer)
        break;

      VkMemoryBarrier barrier{};
      barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
      barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
      barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

      fn(native->commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
      break;
    }
#endif

#if SCORE_HAS_GL
    case QRhi::OpenGLES2: {
      auto* native = static_cast<const QRhiGles2NativeHandles*>(rhi.nativeHandles());
      if(!native || !native->context)
        break;

      auto* f = native->context->extraFunctions();
      if(!f)
        break;

      f->glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
      break;
    }
#endif

#if SCORE_HAS_D3D
    case QRhi::D3D12: {
      auto* native
          = static_cast<const QRhiD3D12CommandBufferNativeHandles*>(cb.nativeHandles());
      if(!native || !native->commandList)
        break;

      auto* cmdList = static_cast<ID3D12GraphicsCommandList*>(native->commandList);
      D3D12_RESOURCE_BARRIER barrier{};
      barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
      barrier.UAV.pResource = nullptr; // all UAV resources
      cmdList->ResourceBarrier(1, &barrier);
      break;
    }
#endif

    case QRhi::D3D11:
      // D3D11 guarantees ordered UAV access within a device context.
      break;

    case QRhi::Metal:
      // Metal guarantees memory visibility between MTLComputeCommandEncoders.
      // Each QRhi compute pass uses a separate encoder, so the
      // endComputePass / beginComputePass boundary is sufficient.
      break;

    default:
      break;
  }
}

void copyBuffer(
    QRhi& rhi, QRhiCommandBuffer& cb,
    QRhiBuffer* src, QRhiBuffer* dst, int size,
    int srcOffset, int dstOffset)
{
  if(!src || !dst || size <= 0 || srcOffset < 0 || dstOffset < 0)
    return;

  switch(rhi.backend())
  {
#if SCORE_HAS_VULKAN
    case QRhi::Vulkan: {
      auto* inst = score::gfx::staticVulkanInstance();
      if(!inst)
        break;

      auto fn = reinterpret_cast<PFN_vkCmdCopyBuffer>(
          inst->getInstanceProcAddr("vkCmdCopyBuffer"));
      if(!fn)
        break;

      auto* native
          = static_cast<const QRhiVulkanCommandBufferNativeHandles*>(cb.nativeHandles());
      if(!native || !native->commandBuffer)
        break;

      auto srcNative = src->nativeBuffer();
      auto dstNative = dst->nativeBuffer();
      if(!srcNative.objects[0] || !dstNative.objects[0])
        break;

      // QRhi documents that NativeBuffer::objects[i] is a POINTER TO the
      // native handle, not the handle itself. From qrhi.cpp's NativeBuffer
      // doc: "the elements are always pointers to the native buffer handle
      // type, even if the native type itself is a pointer (so the elements
      // are VkBuffer * on Vulkan, even though VkBuffer itself is a pointer
      // on 64-bit architectures)". So we must DEREFERENCE objects[0] as
      // const VkBuffer* to get the actual VkBuffer handle.
      VkBuffer srcBuf = *static_cast<const VkBuffer*>(srcNative.objects[0]);
      VkBuffer dstBuf = *static_cast<const VkBuffer*>(dstNative.objects[0]);
      if(srcBuf == VK_NULL_HANDLE || dstBuf == VK_NULL_HANDLE)
        break;

      // Barrier: compute write → transfer read/write
      auto barrierFn = reinterpret_cast<PFN_vkCmdPipelineBarrier>(
          inst->getInstanceProcAddr("vkCmdPipelineBarrier"));
      if(barrierFn)
      {
        VkMemoryBarrier pre{};
        pre.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        pre.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        pre.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
        barrierFn(native->commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &pre, 0, nullptr, 0, nullptr);
      }

      VkBufferCopy region{};
      region.srcOffset = static_cast<VkDeviceSize>(srcOffset);
      region.dstOffset = static_cast<VkDeviceSize>(dstOffset);
      region.size = static_cast<VkDeviceSize>(size);

      fn(native->commandBuffer, srcBuf, dstBuf, 1, &region);

      // Barrier: transfer write → compute read
      if(barrierFn)
      {
        VkMemoryBarrier post{};
        post.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        post.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        post.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        barrierFn(native->commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &post, 0, nullptr, 0, nullptr);
      }
      break;
    }
#endif

#if SCORE_HAS_GL
    case QRhi::OpenGLES2: {
      auto* native = static_cast<const QRhiGles2NativeHandles*>(rhi.nativeHandles());
      if(!native || !native->context)
        break;

      auto* f = native->context->extraFunctions();
      if(!f)
        break;

      auto srcNative = src->nativeBuffer();
      auto dstNative = dst->nativeBuffer();
      if(!srcNative.objects[0] || !dstNative.objects[0])
        break;

      // objects[0] is a `GLuint *`, not a GLuint. See the long comment in
      // the Vulkan branch above.
      GLuint srcId = *static_cast<const GLuint*>(srcNative.objects[0]);
      GLuint dstId = *static_cast<const GLuint*>(dstNative.objects[0]);
      if(srcId == 0 || dstId == 0)
        break;

      auto* gl = native->context->functions();
      gl->glBindBuffer(GL_COPY_READ_BUFFER, srcId);
      gl->glBindBuffer(GL_COPY_WRITE_BUFFER, dstId);
      f->glCopyBufferSubData(
          GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER,
          static_cast<GLintptr>(srcOffset),
          static_cast<GLintptr>(dstOffset),
          static_cast<GLsizeiptr>(size));
      gl->glBindBuffer(GL_COPY_READ_BUFFER, 0);
      gl->glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
      break;
    }
#endif

#if SCORE_HAS_D3D
    case QRhi::D3D12: {
      auto* native
          = static_cast<const QRhiD3D12CommandBufferNativeHandles*>(cb.nativeHandles());
      if(!native || !native->commandList)
        break;

      auto* cmdList = static_cast<ID3D12GraphicsCommandList*>(native->commandList);

      auto srcNative = src->nativeBuffer();
      auto dstNative = dst->nativeBuffer();
      if(!srcNative.objects[0] || !dstNative.objects[0])
        break;

      // objects[0] is an `ID3D12Resource * *`, i.e. a pointer to the
      // resource pointer slot. Same convention as Vulkan -- see the long
      // comment in the Vulkan branch above.
      auto* srcRes
          = *static_cast<ID3D12Resource* const*>(srcNative.objects[0]);
      auto* dstRes
          = *static_cast<ID3D12Resource* const*>(dstNative.objects[0]);
      if(!srcRes || !dstRes)
        break;

      cmdList->CopyBufferRegion(
          dstRes,
          static_cast<UINT64>(dstOffset),
          srcRes,
          static_cast<UINT64>(srcOffset),
          static_cast<UINT64>(size));
      break;
    }
#endif

    case QRhi::D3D11: {
#if SCORE_HAS_D3D
      auto* native = static_cast<const QRhiD3D11NativeHandles*>(rhi.nativeHandles());
      if(!native || !native->context)
        break;

      auto srcNative = src->nativeBuffer();
      auto dstNative = dst->nativeBuffer();
      if(!srcNative.objects[0] || !dstNative.objects[0])
        break;

      auto* ctx = static_cast<ID3D11DeviceContext*>(native->context);
      // objects[0] is an `ID3D11Buffer * *`. Same convention as Vulkan,
      // see the long comment there.
      auto* srcBuf
          = *static_cast<ID3D11Buffer* const*>(srcNative.objects[0]);
      auto* dstBuf
          = *static_cast<ID3D11Buffer* const*>(dstNative.objects[0]);
      if(!srcBuf || !dstBuf)
        break;

      // D3D11 buffer copies use a 1D box; offsets go into box.left and the
      // destination's X coordinate.
      D3D11_BOX box{};
      box.left = static_cast<UINT>(srcOffset);
      box.right = static_cast<UINT>(srcOffset + size);
      box.top = 0;
      box.bottom = 1;
      box.front = 0;
      box.back = 1;
      ctx->CopySubresourceRegion(
          dstBuf, 0, static_cast<UINT>(dstOffset), 0, 0, srcBuf, 0, &box);
#endif
      break;
    }

    case QRhi::Metal:
      copyBufferMetal(rhi, cb, src, dst, size, srcOffset, dstOffset);
      break;

    default:
      break;
  }
}

}
