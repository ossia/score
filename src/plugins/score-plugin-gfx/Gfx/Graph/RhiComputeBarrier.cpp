#include <Gfx/Graph/RhiComputeBarrier.hpp>

#include <QtGui/private/qrhi_p.h>

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
#define SCORE_HAS_GL 1
#endif

// D3D12 / D3D11
#if defined(Q_OS_WIN)
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
}
