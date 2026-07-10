#include <Gfx/Graph/RhiComputeBarrier.hpp>

#include <vector>

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
void copyBufferRegionsMetal(
    QRhi&, QRhiCommandBuffer&, QRhiBuffer*, QRhiBuffer*,
    const BufferCopyRegion*, int)
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

#ifndef GL_IMAGE_BINDING_FORMAT
#define GL_IMAGE_BINDING_FORMAT 0x906E
#endif
#ifndef GL_READ_ONLY
#define GL_READ_ONLY 0x88B8
#endif
#ifndef GL_WRITE_ONLY
#define GL_WRITE_ONLY 0x88B9
#endif
#ifndef GL_READ_WRITE
#define GL_READ_WRITE 0x88BA
#endif
#ifndef GL_ALL_BARRIER_BITS
#define GL_ALL_BARRIER_BITS 0xFFFFFFFF
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
// clang-format off
#include <windows.h>
#include <d3d11.h>
#include <d3d12.h>
// clang-format on
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

// The QRhi::D3D12 enum value and QRhiD3D12CommandBufferNativeHandles (declared
// in qrhi_platform.h) only exist from Qt 6.6 onward — guard the whole case so
// it doesn't break the Win build on Qt < 6.6. (RhiClearBuffer.cpp guards its
// D3D12 case the same way.)
#if SCORE_HAS_D3D && QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
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

bool dispatchComputeLayeredImages(
    QRhi& rhi, QRhiCommandBuffer& cb, QRhiShaderResourceBindings& srb,
    int x, int y, int z)
{
#if SCORE_HAS_GL
  if(rhi.backend() != QRhi::OpenGLES2)
    return false;

  // Scan the bound SRB for storage-image bindings whose texture is layered
  // (3D, cube map, or 2D texture array). Qt's GL backend binds exactly these
  // non-layered on the affected versions (only slice/face/layer 0 accessible),
  // which is what corrupts an imageStore into an image3D / imageCube /
  // image2DArray — see the header doc. This must mirror qrhigles2's own
  // `layered` determination (CubeMap || ThreeDimensional || TextureArray).
  struct Img
  {
    int unit;
    GLuint tex;
    GLenum access;
  };
  std::vector<Img> imgs;
  for(auto it = srb.cbeginBindings(); it != srb.cendBindings(); ++it)
  {
    const auto* d
        = reinterpret_cast<const QRhiShaderResourceBinding::Data*>(&*it);
    GLenum access;
    switch(d->type)
    {
      case QRhiShaderResourceBinding::ImageLoad:
        access = GL_READ_ONLY;
        break;
      case QRhiShaderResourceBinding::ImageStore:
        access = GL_WRITE_ONLY;
        break;
      case QRhiShaderResourceBinding::ImageLoadStore:
        access = GL_READ_WRITE;
        break;
      default:
        continue;
    }
    QRhiTexture* tex = d->u.simage.tex;
    // Match qrhigles2.cpp's layered determination EXACTLY: arrays, cubemaps
    // and 3D textures expose the whole texture with all layers/slices when
    // bound with glBindImageTexture(..., layered=GL_TRUE, layer=0).
    if(!tex
       || !(tex->flags().testFlag(QRhiTexture::ThreeDimensional)
            || tex->flags().testFlag(QRhiTexture::CubeMap)
            || tex->flags().testFlag(QRhiTexture::TextureArray)))
      continue;
    imgs.push_back(
        {d->binding, GLuint(tex->nativeTexture().object), access});
  }

  // No layered storage image in this pass → let QRhi issue the dispatch as
  // usual. The 2D image path is thus completely unaffected.
  if(imgs.empty())
    return false;

  auto* native = static_cast<const QRhiGles2NativeHandles*>(rhi.nativeHandles());
  if(!native || !native->context)
    return false;
  auto* f = native->context->extraFunctions();
  if(!f)
    return false;

  // beginExternal() flushes QRhi's queued pipeline + resource bindings (which
  // include the mis-bound, non-layered layered image). We then re-bind each
  // layered storage image LAYERED (layered=GL_TRUE) using the very format QRhi
  // chose for it (queried back from GL, so no format table needs duplicating),
  // issue the dispatch natively, and emit a full barrier so the downstream
  // sampler / next dispatch sees the whole volume / all faces / all layers.
  cb.beginExternal();
  for(const auto& im : imgs)
  {
    GLint fmt = 0;
    f->glGetIntegeri_v(GL_IMAGE_BINDING_FORMAT, im.unit, &fmt);
    f->glBindImageTexture(
        im.unit, im.tex, 0, GL_TRUE, 0, im.access, GLenum(fmt));
  }
  f->glDispatchCompute(GLuint(x), GLuint(y), GLuint(z));
  f->glMemoryBarrier(GL_ALL_BARRIER_BITS);
  cb.endExternal();
  return true;
#else
  (void)rhi;
  (void)cb;
  (void)srb;
  (void)x;
  (void)y;
  (void)z;
  return false;
#endif
}

void beginBufferCopyBarrier(QRhi& rhi, QRhiCommandBuffer& cb)
{
  switch(rhi.backend())
  {
#if SCORE_HAS_VULKAN
    case QRhi::Vulkan: {
      auto* inst = score::gfx::staticVulkanInstance();
      if(!inst)
        break;
      auto barrierFn = reinterpret_cast<PFN_vkCmdPipelineBarrier>(
          inst->getInstanceProcAddr("vkCmdPipelineBarrier"));
      if(!barrierFn)
        break;
      auto* native
          = static_cast<const QRhiVulkanCommandBufferNativeHandles*>(cb.nativeHandles());
      if(!native || !native->commandBuffer)
        break;
      VkMemoryBarrier pre{};
      pre.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
      pre.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
      pre.dstAccessMask
          = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
      barrierFn(native->commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &pre, 0, nullptr, 0, nullptr);
      break;
    }
#endif
    default:
      // D3D11, D3D12, OpenGL, Metal: no explicit pre-barrier needed or
      // handled by the backend when the encoder transitions.
      break;
  }
}

void endBufferCopyBarrier(QRhi& rhi, QRhiCommandBuffer& cb)
{
  switch(rhi.backend())
  {
#if SCORE_HAS_VULKAN
    case QRhi::Vulkan: {
      auto* inst = score::gfx::staticVulkanInstance();
      if(!inst)
        break;
      auto barrierFn = reinterpret_cast<PFN_vkCmdPipelineBarrier>(
          inst->getInstanceProcAddr("vkCmdPipelineBarrier"));
      if(!barrierFn)
        break;
      auto* native
          = static_cast<const QRhiVulkanCommandBufferNativeHandles*>(cb.nativeHandles());
      if(!native || !native->commandBuffer)
        break;
      VkMemoryBarrier post{};
      post.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
      post.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      post.dstAccessMask
          = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT
            | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT
            | VK_ACCESS_INDEX_READ_BIT
            | VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
      barrierFn(native->commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
                    | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT
                    | VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
                0, 1, &post, 0, nullptr, 0, nullptr);
      break;
    }
#endif
    default:
      break;
  }
}

void copyBuffer(
    QRhi& rhi, QRhiCommandBuffer& cb,
    QRhiBuffer* src, QRhiBuffer* dst, int size,
    int srcOffset, int dstOffset,
    BufferCopyBarrier barrier)
{
  if(!src || !dst || size <= 0 || srcOffset < 0 || dstOffset < 0)
    return;

  // Dynamic buffers rotate over 2-3 backing slots per frame, but every
  // backend's nativeBuffer().objects[0] only exposes slot 0 — copying that
  // slot would hit a stale/wrong frame's data. The compute/MDI callers of
  // these helpers all use Static/Immutable storage buffers; bail on Dynamic
  // as defence-in-depth, matching clearBufferNative()'s Dynamic bail.
  if(src->type() == QRhiBuffer::Dynamic || dst->type() == QRhiBuffer::Dynamic)
    return;

  const bool emit_barriers = (barrier == BufferCopyBarrier::Auto);

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

      // Barrier: compute write → transfer read/write. Skipped when the
      // caller batches multiple copies inside explicit begin/endBufferCopyBarrier.
      auto barrierFn = reinterpret_cast<PFN_vkCmdPipelineBarrier>(
          inst->getInstanceProcAddr("vkCmdPipelineBarrier"));
      if(emit_barriers && barrierFn)
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

      // Barrier: transfer write → compute/vertex read
      if(emit_barriers && barrierFn)
      {
        VkMemoryBarrier post{};
        post.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        post.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        post.dstAccessMask
            = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT
              | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT
              | VK_ACCESS_INDEX_READ_BIT
              | VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
        barrierFn(native->commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
                | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT
                | VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
            0, 1, &post, 0, nullptr, 0, nullptr);
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

#if SCORE_HAS_D3D && QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
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

      // D3D12 is the ODD ONE OUT in QRhi: unlike Vulkan/Metal/D3D11/GL
      // which store `&native_handle` (one extra indirection), the D3D12
      // backend stores `res->resource` directly — i.e.
      // `objects[0]` IS the `ID3D12Resource *`, NOT a pointer to it. See
      // QD3D12Buffer::nativeBuffer in qrhid3d12.cpp:
      //     b.objects[0] = res->resource;   // ID3D12Resource *
      // vs. Vulkan/Metal:
      //     b.objects[i] = &buffers[i];     // VkBuffer * / id<MTLBuffer> *
      // vs. D3D11:
      //     return { { &buffer }, 1 };      // ID3D11Buffer * *
      // Dereferencing here as `**` would treat the COM vtable pointer as
      // an `ID3D12Resource *` and hand garbage to CopyBufferRegion, which
      // the D3D12 debug layer flags as
      // "CORRUPTION: First parameter is corrupt — CORRUPTED_PARAMETER1".
      // const_cast: NativeBuffer::objects is `const void *` (Qt's const-
      // correct getter signal that the *array* is const for inspection),
      // but CopyBufferRegion needs a non-const ID3D12Resource* — and the
      // underlying resource is genuinely mutable (it is the GPU buffer
      // we are about to write to).
      auto* srcRes = static_cast<ID3D12Resource*>(
          const_cast<void*>(srcNative.objects[0]));
      auto* dstRes = static_cast<ID3D12Resource*>(
          const_cast<void*>(dstNative.objects[0]));
      if(!srcRes || !dstRes)
        break;

      // D3D12 has explicit resource states (unlike Vulkan's access masks the
      // backend handles for tracked resources). The buffers are written by a
      // compute pass as UAVs, so transition src→COPY_SOURCE and dst→COPY_DEST
      // before CopyBufferRegion, then back to UNORDERED_ACCESS so subsequent
      // compute/draw reads see the data. Mirrors the Vulkan compute→transfer→
      // compute barrier intent and is gated on emit_barriers the same way.
      const auto transition
          = [cmdList](
                ID3D12Resource* res, D3D12_RESOURCE_STATES before,
                D3D12_RESOURCE_STATES after) {
        D3D12_RESOURCE_BARRIER b{};
        b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        b.Transition.pResource = res;
        b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        b.Transition.StateBefore = before;
        b.Transition.StateAfter = after;
        cmdList->ResourceBarrier(1, &b);
      };
      if(emit_barriers)
      {
        transition(
            srcRes, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_COPY_SOURCE);
        transition(
            dstRes, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_COPY_DEST);
      }

      cmdList->CopyBufferRegion(
          dstRes,
          static_cast<UINT64>(dstOffset),
          srcRes,
          static_cast<UINT64>(srcOffset),
          static_cast<UINT64>(size));

      if(emit_barriers)
      {
        transition(
            srcRes, D3D12_RESOURCE_STATE_COPY_SOURCE,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        transition(
            dstRes, D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
      }
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

void copyBufferRegions(
    QRhi& rhi, QRhiCommandBuffer& cb,
    QRhiBuffer* src, QRhiBuffer* dst,
    const BufferCopyRegion* regions, int count,
    BufferCopyBarrier barrier)
{
  if(!src || !dst || !regions || count <= 0)
    return;

  // See copyBuffer(): Dynamic buffers expose only slot 0 via objects[0], so a
  // native copy would read/write the wrong frame slot. Bail like
  // clearBufferNative() does.
  if(src->type() == QRhiBuffer::Dynamic || dst->type() == QRhiBuffer::Dynamic)
    return;

  const bool emit_barriers = (barrier == BufferCopyBarrier::Auto);

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
      VkBuffer srcBuf = *static_cast<const VkBuffer*>(srcNative.objects[0]);
      VkBuffer dstBuf = *static_cast<const VkBuffer*>(dstNative.objects[0]);
      if(srcBuf == VK_NULL_HANDLE || dstBuf == VK_NULL_HANDLE)
        break;

      auto barrierFn = reinterpret_cast<PFN_vkCmdPipelineBarrier>(
          inst->getInstanceProcAddr("vkCmdPipelineBarrier"));
      if(emit_barriers && barrierFn)
      {
        VkMemoryBarrier pre{};
        pre.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        pre.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        pre.dstAccessMask
            = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
        barrierFn(native->commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                  VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &pre, 0, nullptr, 0, nullptr);
      }

      // Build region array once and issue a single vkCmdCopyBuffer.
      // Small-stack path for the common ≤1024 vertex case; heap fallback
      // for larger point clouds.
      constexpr int kStackMax = 1024;
      VkBufferCopy stack_regions[kStackMax];
      std::vector<VkBufferCopy> heap_regions;
      VkBufferCopy* vk_regions;
      if(count <= kStackMax)
      {
        vk_regions = stack_regions;
      }
      else
      {
        heap_regions.resize(count);
        vk_regions = heap_regions.data();
      }
      for(int i = 0; i < count; ++i)
      {
        vk_regions[i].srcOffset = static_cast<VkDeviceSize>(regions[i].src_offset);
        vk_regions[i].dstOffset = static_cast<VkDeviceSize>(regions[i].dst_offset);
        vk_regions[i].size = static_cast<VkDeviceSize>(regions[i].size);
      }
      fn(native->commandBuffer, srcBuf, dstBuf, (uint32_t)count, vk_regions);

      if(emit_barriers && barrierFn)
      {
        VkMemoryBarrier post{};
        post.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        post.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        post.dstAccessMask
            = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT
              | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT
              | VK_ACCESS_INDEX_READ_BIT
              | VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
        barrierFn(native->commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
                      | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT
                      | VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
                  0, 1, &post, 0, nullptr, 0, nullptr);
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
      GLuint srcId = *static_cast<const GLuint*>(srcNative.objects[0]);
      GLuint dstId = *static_cast<const GLuint*>(dstNative.objects[0]);
      if(srcId == 0 || dstId == 0)
        break;
      auto* gl = native->context->functions();
      gl->glBindBuffer(GL_COPY_READ_BUFFER, srcId);
      gl->glBindBuffer(GL_COPY_WRITE_BUFFER, dstId);
      for(int i = 0; i < count; ++i)
      {
        f->glCopyBufferSubData(
            GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER,
            static_cast<GLintptr>(regions[i].src_offset),
            static_cast<GLintptr>(regions[i].dst_offset),
            static_cast<GLsizeiptr>(regions[i].size));
      }
      gl->glBindBuffer(GL_COPY_READ_BUFFER, 0);
      gl->glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
      break;
    }
#endif

#if SCORE_HAS_D3D && QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
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
      // D3D12 stores the raw ID3D12Resource* directly (no extra
      // indirection). See the long comment in copyBuffer's D3D12 branch
      // above for the Qt-source-level details.
      auto* srcRes = static_cast<ID3D12Resource*>(
          const_cast<void*>(srcNative.objects[0]));
      auto* dstRes = static_cast<ID3D12Resource*>(
          const_cast<void*>(dstNative.objects[0]));
      if(!srcRes || !dstRes)
        break;

      // UAV(compute-write) → COPY_SOURCE/COPY_DEST around the copies, then
      // back to UAV. One transition pair brackets all regions (same src/dst).
      // See the matching comment in copyBuffer's D3D12 branch.
      const auto transition
          = [cmdList](
                ID3D12Resource* res, D3D12_RESOURCE_STATES before,
                D3D12_RESOURCE_STATES after) {
        D3D12_RESOURCE_BARRIER b{};
        b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        b.Transition.pResource = res;
        b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        b.Transition.StateBefore = before;
        b.Transition.StateAfter = after;
        cmdList->ResourceBarrier(1, &b);
      };
      if(emit_barriers)
      {
        transition(
            srcRes, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_COPY_SOURCE);
        transition(
            dstRes, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_COPY_DEST);
      }

      for(int i = 0; i < count; ++i)
      {
        cmdList->CopyBufferRegion(
            dstRes, static_cast<UINT64>(regions[i].dst_offset),
            srcRes, static_cast<UINT64>(regions[i].src_offset),
            static_cast<UINT64>(regions[i].size));
      }

      if(emit_barriers)
      {
        transition(
            srcRes, D3D12_RESOURCE_STATE_COPY_SOURCE,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        transition(
            dstRes, D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
      }
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
      auto* srcBuf
          = *static_cast<ID3D11Buffer* const*>(srcNative.objects[0]);
      auto* dstBuf
          = *static_cast<ID3D11Buffer* const*>(dstNative.objects[0]);
      if(!srcBuf || !dstBuf)
        break;
      for(int i = 0; i < count; ++i)
      {
        D3D11_BOX box{};
        box.left = static_cast<UINT>(regions[i].src_offset);
        box.right = static_cast<UINT>(regions[i].src_offset + regions[i].size);
        box.top = 0; box.bottom = 1; box.front = 0; box.back = 1;
        ctx->CopySubresourceRegion(
            dstBuf, 0, static_cast<UINT>(regions[i].dst_offset), 0, 0,
            srcBuf, 0, &box);
      }
#endif
      break;
    }

    case QRhi::Metal:
      copyBufferRegionsMetal(rhi, cb, src, dst, regions, count);
      break;

    default:
      break;
  }
}

}
