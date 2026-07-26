#include <Gfx/Graph/RhiTextureReadback.hpp>

#include <Gfx/Graph/interop/VkHostImportUpload.hpp>

#include <QtGui/private/qrhi_p.h>
#if __has_include(<QtGui/private/qrhigles2_p.h>)
#include <QtGui/private/qrhigles2_p.h>
#endif

#include <QDebug>

#include <cstdint>
#include <cstring>

// Vulkan
#if QT_HAS_VULKAN || (QT_CONFIG(vulkan) && __has_include(<vulkan/vulkan.h>))
#include <score/gfx/Vulkan.hpp>
#if __has_include(<QtGui/rhi/qrhi_platform.h>)
#include <QtGui/rhi/qrhi_platform.h>
#else
#include <QtGui/private/qrhivulkan_p.h>
#endif
#include <QVulkanFunctions>
#include <QVulkanInstance>
#define SCORE_HAS_VULKAN 1
#endif

// OpenGL
#if QT_CONFIG(opengl)
#include <Gfx/Graph/interop/AmdPinnedBuffers.hpp>

#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#if __has_include(<QtGui/rhi/qrhi_platform.h>)
#include <QtGui/rhi/qrhi_platform.h>
#endif

#ifndef GL_PIXEL_PACK_BUFFER
#define GL_PIXEL_PACK_BUFFER 0x88EB
#endif
#ifndef GL_STREAM_READ
#define GL_STREAM_READ 0x88E1
#endif
#ifndef GL_READ_FRAMEBUFFER
#define GL_READ_FRAMEBUFFER 0x8CA8
#endif
#ifndef GL_MAP_READ_BIT
#define GL_MAP_READ_BIT 0x0001
#endif
#ifndef GL_MAP_PERSISTENT_BIT
#define GL_MAP_PERSISTENT_BIT 0x0040
#endif
#ifndef GL_MAP_COHERENT_BIT
#define GL_MAP_COHERENT_BIT 0x0080
#endif
#ifndef GL_SYNC_GPU_COMMANDS_COMPLETE
#define GL_SYNC_GPU_COMMANDS_COMPLETE 0x9117
#endif
#ifndef GL_SYNC_FLUSH_COMMANDS_BIT
#define GL_SYNC_FLUSH_COMMANDS_BIT 0x00000001
#endif
#ifndef GL_ALREADY_SIGNALED
#define GL_ALREADY_SIGNALED 0x911A
#endif
#ifndef GL_CONDITION_SATISFIED
#define GL_CONDITION_SATISFIED 0x911C
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

// OpenExistingHeapFromAddress lives on ID3D12Device3 (Windows SDK 16299+);
// the QRhi::D3D12 enum value and the D3D12 native-handle structs only exist
// from Qt 6.6 (see RhiComputeBarrier.cpp's D3D12 guard).
#if defined(SCORE_HAS_D3D) && QT_VERSION >= QT_VERSION_CHECK(6, 6, 0) \
    && defined(__ID3D12Device3_INTERFACE_DEFINED__)
#define SCORE_HAS_D3D12_EXISTING_HEAPS 1
#endif

namespace score::gfx
{
namespace
{
std::size_t textureBytesPerPixel(QRhiTexture::Format f) noexcept
{
  switch(f)
  {
    case QRhiTexture::RGBA8:
    case QRhiTexture::BGRA8:
    case QRhiTexture::RGB10A2:
      return 4;
    case QRhiTexture::R8:
    case QRhiTexture::RED_OR_ALPHA8:
      return 1;
    case QRhiTexture::RG8:
    case QRhiTexture::R16:
    case QRhiTexture::R16F:
      return 2;
    case QRhiTexture::RG16:
    case QRhiTexture::R32F:
      return 4;
    case QRhiTexture::RGBA16F:
      return 8;
    case QRhiTexture::RGBA32F:
      return 16;
    default:
      return 0;
  }
}

#if defined(SCORE_HAS_D3D)
DXGI_FORMAT toDxgiFormat(QRhiTexture::Format f) noexcept
{
  switch(f)
  {
    case QRhiTexture::RGBA8:
      return DXGI_FORMAT_R8G8B8A8_UNORM;
    case QRhiTexture::BGRA8:
      return DXGI_FORMAT_B8G8R8A8_UNORM;
    case QRhiTexture::RGB10A2:
      return DXGI_FORMAT_R10G10B10A2_UNORM;
    case QRhiTexture::R8:
      return DXGI_FORMAT_R8_UNORM;
    case QRhiTexture::RG8:
      return DXGI_FORMAT_R8G8_UNORM;
    case QRhiTexture::R16:
      return DXGI_FORMAT_R16_UNORM;
    case QRhiTexture::RG16:
      return DXGI_FORMAT_R16G16_UNORM;
    case QRhiTexture::R16F:
      return DXGI_FORMAT_R16_FLOAT;
    case QRhiTexture::R32F:
      return DXGI_FORMAT_R32_FLOAT;
    case QRhiTexture::RGBA16F:
      return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case QRhiTexture::RGBA32F:
      return DXGI_FORMAT_R32G32B32A32_FLOAT;
    default:
      return DXGI_FORMAT_UNKNOWN;
  }
}
#endif
} // namespace

struct ReadbackTarget
{
  QRhi* rhi{};
  QRhi::Implementation backend{};
  void* dst{};
  std::size_t bytes{};

  /// Set by every successful readbackTextureToHost, cleared by
  /// finishReadbackToHost — a recorded-but-never-finished readback is a caller
  /// bug on GL/D3D11 (stale dst) and is asserted on all backends.
  bool pendingFinish{};

#if SCORE_HAS_VULKAN
  VkDevice vkDev{};
  QVulkanDeviceFunctions* vkDf{};
  interop::VkHostImportedBuffer vkImport{};
#endif

#if SCORE_HAS_GL
  unsigned glBuffer{};
  unsigned glFbo{};
  void* glMapped{};
  void* glSync{};
  std::size_t glPending{};
  bool glPinned{};
#endif

#if SCORE_HAS_D3D12_EXISTING_HEAPS
  ID3D12Heap* d3dHeap{};
  ID3D12Resource* d3dBuffer{};
#endif

#if defined(SCORE_HAS_D3D)
  ID3D11Device* d11Dev{};
  ID3D11DeviceContext* d11Ctx{};
  ID3D11Texture2D* d11Staging{};
  UINT d11W{};
  UINT d11H{};
  DXGI_FORMAT d11Fmt{DXGI_FORMAT_UNKNOWN};
  std::size_t d11RowBytes{};
  std::size_t d11Rows{};
#endif
};

bool canReadbackToHostMemory(QRhi& rhi) noexcept
{
  switch(rhi.backend())
  {
#if SCORE_HAS_VULKAN
    case QRhi::Vulkan:
      return interop::VkHostImportUpload::requiredAlignment(rhi) != 0;
#endif

#if SCORE_HAS_GL
    case QRhi::OpenGLES2:
      // GL_AMD_pinned_memory when present, PBO + one memcpy otherwise.
      return true;
#endif

#if SCORE_HAS_D3D12_EXISTING_HEAPS
    case QRhi::D3D12: {
      const auto* native = static_cast<const QRhiD3D12NativeHandles*>(rhi.nativeHandles());
      if(!native || !native->dev)
        return false;
      auto* dev = static_cast<ID3D12Device*>(native->dev);
      D3D12_FEATURE_DATA_EXISTING_HEAPS eh{};
      if(FAILED(dev->CheckFeatureSupport(D3D12_FEATURE_EXISTING_HEAPS, &eh, sizeof(eh))))
        return false;
      ID3D12Device3* dev3{};
      if(FAILED(dev->QueryInterface(
             __uuidof(ID3D12Device3), reinterpret_cast<void**>(&dev3))))
        return false;
      dev3->Release();
      return eh.Supported;
    }
#endif

#if defined(SCORE_HAS_D3D)
    case QRhi::D3D11: {
      // No D3D11 API can wrap an arbitrary host allocation as a GPU-writable
      // resource, so this path is NOT zero-copy: a reused staging texture is
      // mapped and copied into dst in finishReadbackToHost. That is one copy
      // and one allocation fewer per frame than QRhi::readBackTexture.
      const auto* native
          = static_cast<const QRhiD3D11NativeHandles*>(rhi.nativeHandles());
      return native && native->dev && native->context;
    }
#endif

    case QRhi::Metal:
      // newBufferWithBytesNoCopy: is the analogue (wrap a page-aligned host
      // allocation as an MTLBuffer, then a blit encoder copy into it). Left
      // unimplemented: no Mac hardware is available to validate it, and an
      // unexecuted path here would misreport capability.
      return false;

    default:
      return false;
  }
}

std::size_t readbackHostMemoryAlignment(QRhi& rhi) noexcept
{
  switch(rhi.backend())
  {
#if SCORE_HAS_VULKAN
    case QRhi::Vulkan:
      return interop::VkHostImportUpload::requiredAlignment(rhi);
#endif

#if SCORE_HAS_GL
    case QRhi::OpenGLES2:
      // GL_AMD_pinned_memory page-locks whole pages; the PBO fallback has no
      // requirement but a uniform granularity keeps callers simple.
      return 4096;
#endif

#if SCORE_HAS_D3D12_EXISTING_HEAPS
    case QRhi::D3D12:
      // OpenExistingHeapFromAddress operates on VirtualAlloc regions, whose
      // allocation granularity is 64 KiB.
      return canReadbackToHostMemory(rhi) ? 65536 : 0;
#endif

#if defined(SCORE_HAS_D3D)
    case QRhi::D3D11:
      // dst is only ever a memcpy destination.
      return canReadbackToHostMemory(rhi) ? 1 : 0;
#endif

    default:
      return 0;
  }
}

bool readbackTextureSupported(QRhi& rhi, QRhiTexture& src, std::size_t dstBytes) noexcept
{
  const QSize sz = src.pixelSize();
  const std::size_t bpp = textureBytesPerPixel(src.format());
  if(bpp == 0 || sz.width() <= 0 || sz.height() <= 0)
    return false;
  const std::size_t rowBytes = std::size_t(sz.width()) * bpp;
  if(rowBytes * std::size_t(sz.height()) > dstBytes)
    return false;

  switch(rhi.backend())
  {
#if SCORE_HAS_VULKAN
    case QRhi::Vulkan:
      return canReadbackToHostMemory(rhi);
#endif

#if SCORE_HAS_GL
    case QRhi::OpenGLES2:
      // glReadPixels path: RGBA8 only (see readbackTextureToHost).
      return src.format() == QRhiTexture::RGBA8;
#endif

#if SCORE_HAS_D3D12_EXISTING_HEAPS
    case QRhi::D3D12:
      return canReadbackToHostMemory(rhi)
             && rowBytes % D3D12_TEXTURE_DATA_PITCH_ALIGNMENT == 0;
#endif

#if defined(SCORE_HAS_D3D)
    case QRhi::D3D11:
      return canReadbackToHostMemory(rhi)
             && toDxgiFormat(src.format()) != DXGI_FORMAT_UNKNOWN;
#endif

    default:
      return false;
  }
}

ReadbackTarget* createReadbackTarget(QRhi& rhi, void* dst, std::size_t bytes)
{
  if(!dst || bytes == 0)
    return nullptr;

  switch(rhi.backend())
  {
#if SCORE_HAS_VULKAN
    case QRhi::Vulkan: {
      const auto* native
          = static_cast<const QRhiVulkanNativeHandles*>(rhi.nativeHandles());
      auto* inst = score::gfx::staticVulkanInstance(false);
      if(!native || !native->dev || !inst)
        break;
      auto* df = inst->deviceFunctions(native->dev);
      if(!df)
        break;

      interop::VkHostImportedBuffer imported;
      if(!interop::importHostPointerBuffer(
             rhi, dst, bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
             /*requireHostCoherent=*/true, imported))
        break;

      auto* t = new ReadbackTarget;
      t->rhi = &rhi;
      t->backend = QRhi::Vulkan;
      t->dst = dst;
      t->bytes = bytes;
      t->vkDev = native->dev;
      t->vkDf = df;
      t->vkImport = imported;
      return t;
    }
#endif

#if SCORE_HAS_GL
    case QRhi::OpenGLES2: {
      if(!rhi.makeThreadLocalNativeContextCurrent())
        break;
      auto* ctx = QOpenGLContext::currentContext();
      if(!ctx)
        break;
      auto* f = ctx->extraFunctions();
      if(!f)
        break;

      while(f->glGetError() != 0)
      {
      }

      auto* t = new ReadbackTarget;
      t->rhi = &rhi;
      t->backend = QRhi::OpenGLES2;
      t->dst = dst;
      t->bytes = bytes;

      interop::AmdPinnedBuffers amd;
      if(amd.tryInit(ctx) && amd.hasPinnedMemory
         && (reinterpret_cast<std::uintptr_t>(dst) % 4096) == 0)
      {
        const unsigned buf
            = amd.createPinnedBuffer(GL_PIXEL_PACK_BUFFER, bytes, dst, GL_STREAM_READ);
        if(buf != 0 && f->glGetError() == 0)
        {
          t->glBuffer = buf;
          t->glPinned = true;
          return t;
        }
        if(buf != 0)
        {
          GLuint b = buf;
          f->glDeleteBuffers(1, &b);
        }
        while(f->glGetError() != 0)
        {
        }
      }

      GLuint buf = 0;
      f->glGenBuffers(1, &buf);
      if(buf == 0)
      {
        delete t;
        break;
      }
      f->glBindBuffer(GL_PIXEL_PACK_BUFFER, buf);

      using FN_BufferStorage
          = void(QOPENGLF_APIENTRYP)(GLenum, GLsizeiptr, const void*, GLbitfield);
      auto bufferStorage
          = reinterpret_cast<FN_BufferStorage>(ctx->getProcAddress("glBufferStorage"));
      if(!bufferStorage)
        bufferStorage = reinterpret_cast<FN_BufferStorage>(
            ctx->getProcAddress("glBufferStorageEXT"));
      if(bufferStorage)
      {
        bufferStorage(
            GL_PIXEL_PACK_BUFFER, GLsizeiptr(bytes), nullptr,
            GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
        if(f->glGetError() == 0)
          t->glMapped = f->glMapBufferRange(
              GL_PIXEL_PACK_BUFFER, 0, GLsizeiptr(bytes),
              GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
        while(f->glGetError() != 0)
        {
        }
      }
      if(!t->glMapped)
      {
        // glBufferStorage makes the data store immutable, so a fresh buffer
        // object is needed for the plain-PBO fallback.
        f->glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        f->glDeleteBuffers(1, &buf);
        buf = 0;
        f->glGenBuffers(1, &buf);
        f->glBindBuffer(GL_PIXEL_PACK_BUFFER, buf);
        f->glBufferData(GL_PIXEL_PACK_BUFFER, GLsizeiptr(bytes), nullptr, GL_STREAM_READ);
        if(buf == 0 || f->glGetError() != 0)
        {
          f->glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
          if(buf != 0)
            f->glDeleteBuffers(1, &buf);
          delete t;
          break;
        }
      }
      f->glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
      t->glBuffer = buf;
      return t;
    }
#endif

#if SCORE_HAS_D3D12_EXISTING_HEAPS
    case QRhi::D3D12: {
      const auto* native = static_cast<const QRhiD3D12NativeHandles*>(rhi.nativeHandles());
      if(!native || !native->dev)
        break;
      auto* dev = static_cast<ID3D12Device*>(native->dev);
      D3D12_FEATURE_DATA_EXISTING_HEAPS eh{};
      if(FAILED(dev->CheckFeatureSupport(D3D12_FEATURE_EXISTING_HEAPS, &eh, sizeof(eh)))
         || !eh.Supported)
        break;
      ID3D12Device3* dev3{};
      if(FAILED(dev->QueryInterface(
             __uuidof(ID3D12Device3), reinterpret_cast<void**>(&dev3))))
        break;

      // Only VirtualAlloc / file-mapping regions can be opened as a heap;
      // malloc'd memory fails here and the caller falls back.
      ID3D12Heap* heap{};
      if(FAILED(dev3->OpenExistingHeapFromAddress(
             dst, __uuidof(ID3D12Heap), reinterpret_cast<void**>(&heap)))
         || !heap)
      {
        dev3->Release();
        break;
      }

      D3D12_RESOURCE_DESC bd{};
      bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
      bd.Width = UINT64(bytes);
      bd.Height = 1;
      bd.DepthOrArraySize = 1;
      bd.MipLevels = 1;
      bd.Format = DXGI_FORMAT_UNKNOWN;
      bd.SampleDesc = {1, 0};
      bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
      ID3D12Resource* buf{};
      if(FAILED(dev3->CreatePlacedResource(
             heap, 0, &bd, D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
             __uuidof(ID3D12Resource), reinterpret_cast<void**>(&buf)))
         || !buf)
      {
        heap->Release();
        dev3->Release();
        break;
      }
      dev3->Release();

      auto* t = new ReadbackTarget;
      t->rhi = &rhi;
      t->backend = QRhi::D3D12;
      t->dst = dst;
      t->bytes = bytes;
      t->d3dHeap = heap;
      t->d3dBuffer = buf;
      return t;
    }
#endif

#if defined(SCORE_HAS_D3D)
    case QRhi::D3D11: {
      const auto* native
          = static_cast<const QRhiD3D11NativeHandles*>(rhi.nativeHandles());
      if(!native || !native->dev || !native->context)
        break;

      // The staging texture needs the source's size/format, only known at
      // readback time — created lazily there, reused across frames.
      auto* t = new ReadbackTarget;
      t->rhi = &rhi;
      t->backend = QRhi::D3D11;
      t->dst = dst;
      t->bytes = bytes;
      t->d11Dev = static_cast<ID3D11Device*>(native->dev);
      t->d11Ctx = static_cast<ID3D11DeviceContext*>(native->context);
      return t;
    }
#endif

    default:
      break;
  }
  return nullptr;
}

void destroyReadbackTarget(ReadbackTarget* t)
{
  if(!t)
    return;

  switch(t->backend)
  {
#if SCORE_HAS_VULKAN
    case QRhi::Vulkan:
      if(t->rhi)
        interop::releaseHostImportedBuffer(*t->rhi, t->vkImport);
      break;
#endif

#if SCORE_HAS_GL
    case QRhi::OpenGLES2: {
      if(!t->rhi || !t->rhi->makeThreadLocalNativeContextCurrent())
        break;
      auto* ctx = QOpenGLContext::currentContext();
      if(!ctx)
        break;
      auto* f = ctx->extraFunctions();
      if(!f)
        break;
      if(t->glSync)
        f->glDeleteSync(static_cast<GLsync>(t->glSync));
      if(t->glMapped)
      {
        f->glBindBuffer(GL_PIXEL_PACK_BUFFER, t->glBuffer);
        f->glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
        f->glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
      }
      if(t->glBuffer)
      {
        GLuint b = t->glBuffer;
        f->glDeleteBuffers(1, &b);
      }
      if(t->glFbo)
      {
        GLuint fb = t->glFbo;
        f->glDeleteFramebuffers(1, &fb);
      }
      break;
    }
#endif

#if SCORE_HAS_D3D12_EXISTING_HEAPS
    case QRhi::D3D12:
      if(t->d3dBuffer)
        t->d3dBuffer->Release();
      if(t->d3dHeap)
        t->d3dHeap->Release();
      break;
#endif

#if defined(SCORE_HAS_D3D)
    case QRhi::D3D11:
      if(t->d11Staging)
        t->d11Staging->Release();
      break;
#endif

    default:
      break;
  }
  delete t;
}

bool readbackTextureToHost(
    QRhi& rhi, QRhiCommandBuffer& cb, QRhiTexture& src, ReadbackTarget& t)
{
  const QSize sz = src.pixelSize();
  const std::size_t bpp = textureBytesPerPixel(src.format());
  if(bpp == 0 || sz.width() <= 0 || sz.height() <= 0)
    return false;
  const std::size_t rowBytes = std::size_t(sz.width()) * bpp;
  const std::size_t required = rowBytes * std::size_t(sz.height());
  if(required > t.bytes)
    return false;

  if(t.pendingFinish)
  {
    // On GL/D3D11 the previous frame's dst bytes were never produced — the
    // caller would be consuming stale data without noticing.
    qWarning("readbackTextureToHost: previous readback was never completed - "
             "finishReadbackToHost must be called after every frame");
    Q_ASSERT(!"finishReadbackToHost was not called after the previous readback");
    finishReadbackToHost(rhi, t);
  }

  switch(rhi.backend())
  {
#if SCORE_HAS_VULKAN
    case QRhi::Vulkan: {
      if(!t.vkDf || !t.vkImport.buffer)
        return false;
      const auto nt = src.nativeTexture();
      if(!nt.object || nt.layout == 0)
        return false;
      const VkImage img = VkImage(nt.object);
      const VkBuffer buf = reinterpret_cast<VkBuffer>(t.vkImport.buffer);

      cb.beginExternal();
      const auto* native
          = static_cast<const QRhiVulkanCommandBufferNativeHandles*>(cb.nativeHandles());
      if(!native || !native->commandBuffer)
      {
        cb.endExternal();
        return false;
      }

      auto transition = [&](VkImageLayout from, VkImageLayout to) {
        VkImageMemoryBarrier b{};
        b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        b.oldLayout = from;
        b.newLayout = to;
        b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image = img;
        b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        b.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        b.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        t.vkDf->vkCmdPipelineBarrier(
            native->commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &b);
      };

      transition(VkImageLayout(nt.layout), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

      VkBufferImageCopy region{};
      region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
      region.imageExtent = {uint32_t(sz.width()), uint32_t(sz.height()), 1};
      t.vkDf->vkCmdCopyImageToBuffer(
          native->commandBuffer, img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buf, 1,
          &region);

      VkBufferMemoryBarrier bb{};
      bb.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
      bb.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      bb.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
      bb.srcQueueFamilyIndex = bb.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      bb.buffer = buf;
      bb.size = VK_WHOLE_SIZE;
      t.vkDf->vkCmdPipelineBarrier(
          native->commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
          VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr, 1, &bb, 0, nullptr);

      transition(
          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
      cb.endExternal();

      // QRhi tracks layouts itself; without this its next barrier would use a
      // stale oldLayout and the driver would reject the transition.
      src.setNativeLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
      t.pendingFinish = true;
      return true;
    }
#endif

#if SCORE_HAS_GL
    case QRhi::OpenGLES2: {
      if(!t.glBuffer)
        return false;
      // glReadPixels' universally supported external format; other texture
      // formats fall back to QRhi::readBackTexture.
      if(src.format() != QRhiTexture::RGBA8)
        return false;
      const auto nt = src.nativeTexture();
      const GLuint tex = GLuint(nt.object);
      if(tex == 0)
        return false;

      cb.beginExternal();
      auto* ctx = QOpenGLContext::currentContext();
      auto* f = ctx ? ctx->extraFunctions() : nullptr;
      if(!f)
      {
        cb.endExternal();
        return false;
      }

      if(!t.glFbo)
      {
        GLuint fbo = 0;
        f->glGenFramebuffers(1, &fbo);
        t.glFbo = fbo;
      }
      if(!t.glFbo)
      {
        cb.endExternal();
        return false;
      }
      f->glBindFramebuffer(GL_READ_FRAMEBUFFER, t.glFbo);
      f->glFramebufferTexture2D(
          GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
      if(f->glCheckFramebufferStatus(GL_READ_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
      {
        f->glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        cb.endExternal();
        return false;
      }

      f->glBindBuffer(GL_PIXEL_PACK_BUFFER, t.glBuffer);
      f->glPixelStorei(GL_PACK_ALIGNMENT, 1);
      f->glReadPixels(0, 0, sz.width(), sz.height(), GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
      f->glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
      f->glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

      if(t.glSync)
        f->glDeleteSync(static_cast<GLsync>(t.glSync));
      t.glSync = f->glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
      t.glPending = required;
      cb.endExternal();
      t.pendingFinish = true;
      return true;
    }
#endif

#if SCORE_HAS_D3D12_EXISTING_HEAPS
    case QRhi::D3D12: {
      if(!t.d3dBuffer)
        return false;
      const DXGI_FORMAT fmt = toDxgiFormat(src.format());
      if(fmt == DXGI_FORMAT_UNKNOWN)
        return false;
      // CopyTextureRegion requires the buffer footprint's row pitch to be a
      // multiple of D3D12_TEXTURE_DATA_PITCH_ALIGNMENT (256); a padded pitch
      // would change the layout the caller sees, so only tightly-packable
      // sizes are supported.
      if(rowBytes % D3D12_TEXTURE_DATA_PITCH_ALIGNMENT != 0)
        return false;
      const auto nt = src.nativeTexture();
      auto* res = reinterpret_cast<ID3D12Resource*>(quintptr(nt.object));
      if(!res)
        return false;
      // On D3D12, QRhiTexture::nativeTexture().layout carries the resource's
      // current D3D12_RESOURCE_STATES (qrhid3d12.cpp).
      const auto state = D3D12_RESOURCE_STATES(nt.layout);

      cb.beginExternal();
      const auto* native
          = static_cast<const QRhiD3D12CommandBufferNativeHandles*>(cb.nativeHandles());
      if(!native || !native->commandList)
      {
        cb.endExternal();
        return false;
      }
      auto* cmdList = static_cast<ID3D12GraphicsCommandList*>(native->commandList);

      const auto transition
          = [cmdList, res](D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
        if(before == after)
          return;
        D3D12_RESOURCE_BARRIER b{};
        b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b.Transition.pResource = res;
        b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        b.Transition.StateBefore = before;
        b.Transition.StateAfter = after;
        cmdList->ResourceBarrier(1, &b);
      };
      transition(state, D3D12_RESOURCE_STATE_COPY_SOURCE);

      D3D12_TEXTURE_COPY_LOCATION dstLoc{};
      dstLoc.pResource = t.d3dBuffer;
      dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
      dstLoc.PlacedFootprint.Offset = 0;
      dstLoc.PlacedFootprint.Footprint
          = {fmt, UINT(sz.width()), UINT(sz.height()), 1, UINT(rowBytes)};
      D3D12_TEXTURE_COPY_LOCATION srcLoc{};
      srcLoc.pResource = res;
      srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
      srcLoc.SubresourceIndex = 0;
      cmdList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

      transition(D3D12_RESOURCE_STATE_COPY_SOURCE, state);
      cb.endExternal();
      t.pendingFinish = true;
      return true;
    }
#endif

#if defined(SCORE_HAS_D3D)
    case QRhi::D3D11: {
      if(!t.d11Dev || !t.d11Ctx)
        return false;
      const DXGI_FORMAT fmt = toDxgiFormat(src.format());
      if(fmt == DXGI_FORMAT_UNKNOWN)
        return false;
      const auto nt = src.nativeTexture();
      auto* res = reinterpret_cast<ID3D11Texture2D*>(quintptr(nt.object));
      if(!res)
        return false;

      if(t.d11Staging
         && (t.d11W != UINT(sz.width()) || t.d11H != UINT(sz.height())
             || t.d11Fmt != fmt))
      {
        t.d11Staging->Release();
        t.d11Staging = nullptr;
      }
      if(!t.d11Staging)
      {
        D3D11_TEXTURE2D_DESC sd{};
        res->GetDesc(&sd);
        sd.Usage = D3D11_USAGE_STAGING;
        sd.BindFlags = 0;
        sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        sd.MiscFlags = 0;
        sd.MipLevels = 1;
        sd.ArraySize = 1;
        if(FAILED(t.d11Dev->CreateTexture2D(&sd, nullptr, &t.d11Staging))
           || !t.d11Staging)
          return false;
        t.d11W = UINT(sz.width());
        t.d11H = UINT(sz.height());
        t.d11Fmt = fmt;
      }

      cb.beginExternal();
      t.d11Ctx->CopySubresourceRegion(t.d11Staging, 0, 0, 0, 0, res, 0, nullptr);
      cb.endExternal();

      t.d11RowBytes = rowBytes;
      t.d11Rows = std::size_t(sz.height());
      t.pendingFinish = true;
      return true;
    }
#endif

    case QRhi::Metal:
      // Not implemented; see canReadbackToHostMemory.
      return false;

    default:
      return false;
  }
}

bool finishReadbackToHost(QRhi& rhi, ReadbackTarget& t)
{
  t.pendingFinish = false;
  switch(rhi.backend())
  {
#if SCORE_HAS_GL
    case QRhi::OpenGLES2: {
      if(!t.glSync)
        return true;
      if(!rhi.makeThreadLocalNativeContextCurrent())
        return false;
      auto* ctx = QOpenGLContext::currentContext();
      auto* f = ctx ? ctx->extraFunctions() : nullptr;
      if(!f)
        return false;

      const GLenum r = f->glClientWaitSync(
          static_cast<GLsync>(t.glSync), GL_SYNC_FLUSH_COMMANDS_BIT,
          5'000'000'000ull);
      f->glDeleteSync(static_cast<GLsync>(t.glSync));
      t.glSync = nullptr;
      const std::size_t pending = t.glPending;
      t.glPending = 0;
      if(r != GL_ALREADY_SIGNALED && r != GL_CONDITION_SATISFIED)
        return false;
      if(t.glPinned)
        return true;

      const std::size_t n = pending < t.bytes ? pending : t.bytes;
      if(n == 0)
        return true;
      if(t.glMapped)
      {
        std::memcpy(t.dst, t.glMapped, n);
        return true;
      }
      f->glBindBuffer(GL_PIXEL_PACK_BUFFER, t.glBuffer);
      void* p = f->glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, GLsizeiptr(n), GL_MAP_READ_BIT);
      bool ok = false;
      if(p)
      {
        std::memcpy(t.dst, p, n);
        f->glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
        ok = true;
      }
      f->glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
      return ok;
    }
#endif

#if defined(SCORE_HAS_D3D)
    case QRhi::D3D11: {
      if(!t.d11Staging || !t.d11Ctx || t.d11Rows == 0)
        return true;
      // Map blocks until the recorded copy completes; when the caller's frame
      // already synchronized (offscreen endFrame) it returns immediately.
      D3D11_MAPPED_SUBRESOURCE map{};
      if(FAILED(t.d11Ctx->Map(t.d11Staging, 0, D3D11_MAP_READ, 0, &map))
         || !map.pData)
        return false;
      const std::size_t rows = t.d11Rows;
      const std::size_t rowBytes = t.d11RowBytes;
      auto* dst = static_cast<std::uint8_t*>(t.dst);
      const auto* srcp = static_cast<const std::uint8_t*>(map.pData);
      if(map.RowPitch == rowBytes)
        std::memcpy(dst, srcp, rowBytes * rows);
      else
        for(std::size_t y = 0; y < rows; ++y)
          std::memcpy(dst + y * rowBytes, srcp + y * map.RowPitch, rowBytes);
      t.d11Ctx->Unmap(t.d11Staging, 0);
      t.d11Rows = 0;
      return true;
    }
#endif

    default:
      // Vulkan and D3D12 write into dst directly; the frame synchronization
      // the caller already performs (QRhi::finish() / frame fence) makes the
      // bytes visible.
      return true;
  }
}
}
