#pragma once

#include <score/gfx/Vulkan.hpp>

// CUDA driver-API types + dlopen'd function table (shared with the
// CudaInterop consumer — see Gfx/Graph/interop/CudaFunctions.hpp).
#include <Gfx/Graph/interop/CudaFunctions.hpp>
#include <Gfx/Graph/interop/VkExternalMemoryHelpers.hpp>

extern "C" {
#if __has_include(<libavutil/hwcontext_cuda.h>)
// CUDA_VERSION=0 keeps libavutil from pulling in the real <cuda.h>; the
// types it needs (CUcontext, CUstream, ...) are provided by CudaFunctions.hpp.
#define CUDA_VERSION 0
#include <libavutil/hwcontext_cuda.h>
#define SCORE_HAS_CUDA_HWCONTEXT 1
#endif
}

#if defined(SCORE_HAS_CUDA_HWCONTEXT) && QT_HAS_VULKAN && QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)

#include <Gfx/Graph/decoders/ColorSpace.hpp>
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>
#include <Gfx/Graph/decoders/NV12.hpp>
#include <Gfx/Graph/decoders/P010.hpp>
#include <Video/GpuFormats.hpp>

#include <QtGui/private/qrhivulkan_p.h>
#include <qvulkanfunctions.h>
#include <vulkan/vulkan.h>

#if defined(_WIN32)
#include <windows.h>
#ifndef VK_USE_PLATFORM_WIN32_KHR
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#include <vulkan/vulkan_win32.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#endif

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/hwcontext.h>
}

namespace score::gfx
{

/**
 * @brief CUDA zero-copy decoder for the Vulkan RHI backend.
 *
 * Creates Vulkan textures with exportable memory, imports them into CUDA
 * as external memory mapped to CUDA arrays, then performs GPU-to-GPU copies
 * from the NVDEC-decoded CUdeviceptr to the CUDA-mapped Vulkan textures.
 *
 * Eliminates the GPU→CPU→GPU roundtrip: the copy stays entirely on the GPU.
 *
 * Setup (once in init):
 *   VkImage (exportable) → export fd → cuImportExternalMemory → CUDA array
 *   QRhiTexture::createFrom(VkImage)
 *
 * Per frame (in exec):
 *   cuMemcpy2DAsync(frame->data[i] → CUDA array)  (GPU→GPU)
 *   cuStreamSynchronize
 *
 * Requires: VK_KHR_external_memory_fd (Linux) or VK_KHR_external_memory_win32 (Windows),
 *           CUDA driver with external memory support.
 */
struct HWCudaVulkanDecoder : GPUVideoDecoder
{
  Video::ImageFormat& decoder;
  PixelFormatInfo m_fmt;

  // Vulkan handles (borrowed from QRhi). m_qInst is the route through
  // which vkinterop::VulkanCtx resolves device/instance function tables.
  VkDevice m_dev{VK_NULL_HANDLE};
  VkPhysicalDevice m_physDev{VK_NULL_HANDLE};
  QVulkanInstance* m_qInst{};

  // CUDA context and stream (from FFmpeg's AVCUDADeviceContext)
  CUcontext m_cuCtx{};
  CUstream m_cuStream{};

  // Dynamically loaded CUDA functions
  CudaFunctions m_cu;

  // Per-plane resources (created once, reused every frame)
  struct PlaneResources
  {
    VkImage image{VK_NULL_HANDLE};
    VkDeviceMemory memory{VK_NULL_HANDLE};
    CUexternalMemory cuExtMem{};
    CUmipmappedArray cuMipArray{};
    CUarray cuArray{};
    VkDeviceSize memSize{};
  };
  PlaneResources m_planes[2]{}; // 0=Y, 1=UV

  bool m_interopReady{false};

  // ------------------------------------------------------------------

  static bool isAvailable(QRhi& rhi, AVBufferRef* hwDeviceCtx)
  {
    if(rhi.backend() != QRhi::Vulkan)
      return false;
    auto* nh = static_cast<const QRhiVulkanNativeHandles*>(rhi.nativeHandles());
    if(!nh || !nh->dev || !nh->physDev || !nh->inst)
      return false;
#if defined(_WIN32)
    if(!nh->inst->getInstanceProcAddr("vkGetMemoryWin32HandleKHR"))
      return false;
#else
    if(!nh->inst->getInstanceProcAddr("vkGetMemoryFdKHR"))
      return false;
#endif
    if(!hwDeviceCtx)
      return false;

    // Verify it's actually a CUDA device context
    auto* devCtx = reinterpret_cast<AVHWDeviceContext*>(hwDeviceCtx->data);
    if(devCtx->type != AV_HWDEVICE_TYPE_CUDA)
      return false;

    // Check that CUDA driver supports external memory
    CudaFunctions probe;
    return probe.load();
  }

  explicit HWCudaVulkanDecoder(
      Video::ImageFormat& d, QRhi& rhi, AVBufferRef* hwDeviceCtx,
      PixelFormatInfo fmt)
      : decoder{d}
      , m_fmt{fmt}
  {
    auto* nh = static_cast<const QRhiVulkanNativeHandles*>(rhi.nativeHandles());
    m_dev = nh->dev;
    m_physDev = nh->physDev;
    m_qInst = nh->inst;

    // Extract CUDA context and stream from FFmpeg's device context
    auto* devCtx = reinterpret_cast<AVHWDeviceContext*>(hwDeviceCtx->data);
    auto* cudaDevCtx = static_cast<AVCUDADeviceContext*>(devCtx->hwctx);
    m_cuCtx = cudaDevCtx->cuda_ctx;
    m_cuStream = cudaDevCtx->stream;

    m_cu.load();
  }

  ~HWCudaVulkanDecoder() override { cleanup(); }

  void cleanup()
  {
    if(m_cuCtx && m_cu.ctxPush)
    {
      m_cu.ctxPush(m_cuCtx);
      for(auto& p : m_planes)
      {
        if(p.cuMipArray)
          m_cu.destroyMipArray(p.cuMipArray);
        if(p.cuExtMem)
          m_cu.destroyExtMem(p.cuExtMem);
        p.cuArray = {};
        p.cuMipArray = {};
        p.cuExtMem = {};
      }
      CUcontext dummy{};
      m_cu.ctxPop(&dummy);
    }

    vkinterop::VulkanCtx vctx{
        m_qInst ? m_qInst->vkInstance() : VK_NULL_HANDLE, m_physDev, m_dev,
        m_qInst};
    for(auto& p : m_planes)
    {
      vkinterop::ExternalImage img{p.image, p.memory, p.memSize};
      vkinterop::destroyExternal(vctx, img);
      p.image = VK_NULL_HANDLE;
      p.memory = VK_NULL_HANDLE;
    }

    m_interopReady = false;
  }

  // ------------------------------------------------------------------
  //  init — create exportable Vulkan textures, import into CUDA
  // ------------------------------------------------------------------

  std::pair<QShader, QShader> init(RenderList& r) override
  {
    auto& rhi = *r.state.rhi;
    const auto w = decoder.width, h = decoder.height;

    if(m_fmt.is10bit())
    {
      // P010: R16 (Y) + RG16 (UV)
      {
        auto tex = rhi.newTexture(QRhiTexture::R16, {w, h}, 1, QRhiTexture::Flag{});
        tex->create();
        auto sampler = rhi.newSampler(
            QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
            QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
        sampler->create();
        samplers.push_back({sampler, tex});
      }
      {
        auto tex
            = rhi.newTexture(QRhiTexture::RG16, {w / 2, h / 2}, 1, QRhiTexture::Flag{});
        tex->create();
        auto sampler = rhi.newSampler(
            QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
            QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
        sampler->create();
        samplers.push_back({sampler, tex});
      }

      // Setup Vulkan→CUDA interop for both planes
      if(!setupPlane(0, VK_FORMAT_R16_UNORM, w, h, 1, 2)
         || !setupPlane(1, VK_FORMAT_R16G16_UNORM, w / 2, h / 2, 2, 2))
      {
        qDebug() << "HWCudaVulkanDecoder: interop setup failed";
        cleanup();
        // setupPlane may already have re-pointed a sampler texture at a
        // VkImage cleanup() just destroyed (createFrom). Recreate each
        // texture with its own QRhi-owned storage so the material keeps
        // binding valid images (rendering black instead of faulting).
        for(auto& s : samplers)
          if(s.texture)
            s.texture->create();
      }

      return score::gfx::makeShaders(
          r.state, vertexShader(),
          QString(P010Decoder::frag).arg("").arg(colorMatrix(decoder)));
    }
    else
    {
      // NV12: R8 (Y) + RG8 (UV)
      {
        auto tex = rhi.newTexture(QRhiTexture::R8, {w, h}, 1, QRhiTexture::Flag{});
        tex->create();
        auto sampler = rhi.newSampler(
            QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
            QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
        sampler->create();
        samplers.push_back({sampler, tex});
      }
      {
        auto tex
            = rhi.newTexture(QRhiTexture::RG8, {w / 2, h / 2}, 1, QRhiTexture::Flag{});
        tex->create();
        auto sampler = rhi.newSampler(
            QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
            QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
        sampler->create();
        samplers.push_back({sampler, tex});
      }

      // Setup Vulkan→CUDA interop for both planes
      if(!setupPlane(0, VK_FORMAT_R8_UNORM, w, h, 1, 1)
         || !setupPlane(1, VK_FORMAT_R8G8_UNORM, w / 2, h / 2, 2, 1))
      {
        qDebug() << "HWCudaVulkanDecoder: interop setup failed";
        cleanup();
        // setupPlane may already have re-pointed a sampler texture at a
        // VkImage cleanup() just destroyed (createFrom). Recreate each
        // texture with its own QRhi-owned storage so the material keeps
        // binding valid images (rendering black instead of faulting).
        for(auto& s : samplers)
          if(s.texture)
            s.texture->create();
      }

      QString frag = NV12Decoder::nv12_filter_prologue;
      frag += "    vec3 yuv = vec3(y, u, v);\n";
      frag += NV12Decoder::nv12_filter_epilogue;
      return score::gfx::makeShaders(
          r.state, vertexShader(), frag.arg("").arg(colorMatrix(decoder)));
    }
  }

  // ------------------------------------------------------------------
  //  exec — GPU-to-GPU copy from NVDEC output to Vulkan texture
  // ------------------------------------------------------------------

  void exec(RenderList& r, QRhiResourceUpdateBatch& res, AVFrame& frame) override
  {
#if LIBAVUTIL_VERSION_MAJOR >= 57
    if(!m_interopReady)
      return;

    if(!Video::formatIsHardwareDecoded(static_cast<AVPixelFormat>(frame.format)))
      return;

    const int w = decoder.width;
    const int h = decoder.height;
    const int bpc = m_fmt.is10bit() ? 2 : 1; // bytes per component

    m_cu.ctxPush(m_cuCtx);

    // Y plane: frame->data[0] is CUdeviceptr
    {
      CUDA_MEMCPY2D cpy{};
      cpy.srcMemoryType = CU_MEMORYTYPE_DEVICE;
      cpy.srcDevice = reinterpret_cast<CUdeviceptr>(frame.data[0]);
      cpy.srcPitch = static_cast<size_t>(frame.linesize[0]);
      cpy.dstMemoryType = CU_MEMORYTYPE_ARRAY;
      cpy.dstArray = m_planes[0].cuArray;
      cpy.WidthInBytes = static_cast<size_t>(w * 1 * bpc); // 1 channel
      cpy.Height = static_cast<size_t>(h);
      m_cu.memcpy2DAsync(&cpy, m_cuStream);
    }

    // UV plane: frame->data[1] is CUdeviceptr
    {
      CUDA_MEMCPY2D cpy{};
      cpy.srcMemoryType = CU_MEMORYTYPE_DEVICE;
      cpy.srcDevice = reinterpret_cast<CUdeviceptr>(frame.data[1]);
      cpy.srcPitch = static_cast<size_t>(frame.linesize[1]);
      cpy.dstMemoryType = CU_MEMORYTYPE_ARRAY;
      cpy.dstArray = m_planes[1].cuArray;
      cpy.WidthInBytes = static_cast<size_t>((w / 2) * 2 * bpc); // 2 channels
      cpy.Height = static_cast<size_t>(h / 2);
      m_cu.memcpy2DAsync(&cpy, m_cuStream);
    }

    // Wait for copies to complete before Vulkan reads the textures
    m_cu.streamSync(m_cuStream);

    CUcontext dummy{};
    m_cu.ctxPop(&dummy);

    // Tell Qt RHI the images were written externally so it inserts a barrier.
    // VK_IMAGE_LAYOUT_GENERAL (1) → SHADER_READ_ONLY_OPTIMAL transition
    // preserves content and acts as a memory barrier.
    samplers[0].texture->setNativeLayout(VK_IMAGE_LAYOUT_GENERAL);
    samplers[1].texture->setNativeLayout(VK_IMAGE_LAYOUT_GENERAL);
#endif
  }

private:
  // ------------------------------------------------------------------
  //  Setup one plane: exportable VkImage → fd → CUDA external memory
  // ------------------------------------------------------------------
  bool setupPlane(
      int idx, VkFormat vkFmt, int w, int h,
      int numChannels, int bytesPerChannel)
  {
    auto& plane = m_planes[idx];

#if defined(_WIN32)
    constexpr auto kHandleType
        = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
    constexpr auto kCudaHandleType
        = CU_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32;
#else
    constexpr auto kHandleType
        = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
    constexpr auto kCudaHandleType
        = CU_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD;
#endif

    vkinterop::VulkanCtx vctx{m_qInst->vkInstance(), m_physDev, m_dev, m_qInst};

    // --- Create exportable VkImage + bound memory in one helper call. ---
    vkinterop::ExternalImageDesc imgDesc{};
    imgDesc.format = vkFmt;
    imgDesc.extent
        = {static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1u};
    imgDesc.usage
        = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imgDesc.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgDesc.handleType = kHandleType;
    imgDesc.dedicated = true;
    imgDesc.preferDeviceLocal = true;

    auto extImg = vkinterop::createExportableImage(vctx, imgDesc);
    if(!extImg)
      return false;

    plane.image = extImg->image;
    plane.memory = extImg->memory;
    plane.memSize = extImg->size;

    // --- Export memory as fd / HANDLE. ---
    auto exported = vkinterop::exportMemoryHandle(vctx, plane.memory, kHandleType);
    if(!exported)
    {
      vkinterop::ExternalImage tmp{plane.image, plane.memory, plane.memSize};
      vkinterop::destroyExternal(vctx, tmp);
      plane.image = VK_NULL_HANDLE;
      plane.memory = VK_NULL_HANDLE;
      return false;
    }

    // --- Import into CUDA using FFmpeg's CUcontext (not score's own — we
    //     deliberately do NOT use cuda_interop_import_vulkan_image here because
    //     the CUarray must live in the same context as NVDEC's decoder
    //     output for cuMemcpy2DAsync in exec()).
    m_cu.ctxPush(m_cuCtx);

    CUDA_EXTERNAL_MEMORY_HANDLE_DESC memDesc{};
    memDesc.type = kCudaHandleType;
#if defined(_WIN32)
    memDesc.handle.win32.handle = exported->handle;
#else
    memDesc.handle.fd = exported->fd;
#endif
    memDesc.size = plane.memSize;

    if(m_cu.importExtMem(&plane.cuExtMem, &memDesc) != CUDA_SUCCESS)
    {
#if defined(_WIN32)
      CloseHandle(exported->handle);
#else
      ::close(exported->fd); // ownership did not transfer; we must close
#endif
      CUcontext dummy{};
      m_cu.ctxPop(&dummy);
      return false;
    }
#if defined(_WIN32)
    // CUDA does NOT take ownership of Win32 handles — close after import.
    CloseHandle(exported->handle);
#else
    // fd ownership transferred to CUDA per Vulkan / CUDA spec.
#endif

    // --- Map to a CUDA mipmapped array; expose level 0 as the memcpy
    //     destination used by exec().
    CUDA_EXTERNAL_MEMORY_MIPMAPPED_ARRAY_DESC mipDesc{};
    mipDesc.offset = 0;
    mipDesc.arrayDesc.Width = static_cast<size_t>(w);
    mipDesc.arrayDesc.Height = static_cast<size_t>(h);
    mipDesc.arrayDesc.Depth = 0; // 2D
    mipDesc.arrayDesc.Format = (bytesPerChannel == 2)
                                   ? CU_AD_FORMAT_UNSIGNED_INT16
                                   : CU_AD_FORMAT_UNSIGNED_INT8;
    mipDesc.arrayDesc.NumChannels = static_cast<unsigned int>(numChannels);
    mipDesc.arrayDesc.Flags = 0;
    mipDesc.numLevels = 1;

    if(m_cu.getMapArray(&plane.cuMipArray, plane.cuExtMem, &mipDesc)
       != CUDA_SUCCESS)
    {
      CUcontext dummy{};
      m_cu.ctxPop(&dummy);
      return false;
    }
    if(m_cu.getLevel(&plane.cuArray, plane.cuMipArray, 0) != CUDA_SUCCESS)
    {
      CUcontext dummy{};
      m_cu.ctxPop(&dummy);
      return false;
    }

    CUcontext dummy{};
    m_cu.ctxPop(&dummy);

    // --- Wrap VkImage in QRhiTexture (non-owning); layout=GENERAL since
    //     CUDA writes externally.
    samplers[idx].texture->createFrom(QRhiTexture::NativeTexture{
        quint64(plane.image), VK_IMAGE_LAYOUT_GENERAL});

    m_interopReady = (idx == 1); // Ready after both planes are set up
    return true;
  }
};

} // namespace score::gfx

#endif // SCORE_HAS_CUDA_HWCONTEXT && QT_HAS_VULKAN && QT_VERSION >= 6.6
