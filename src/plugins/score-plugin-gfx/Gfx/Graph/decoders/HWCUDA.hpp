#pragma once

#include <score/gfx/Vulkan.hpp>

extern "C" {
#if __has_include(<libavutil/hwcontext_cuda.h>)
#define CUDA_VERSION 0

// Opaque handles
typedef struct CUctx_st* CUcontext;
typedef struct CUextMemory_st* CUexternalMemory;
typedef struct CUmipmappedArray_st* CUmipmappedArray;
typedef struct CUarray_st* CUarray;
typedef struct CUstream_st* CUstream;

// Device pointer
#if defined(_WIN64) || defined(__LP64__)
typedef unsigned long long CUdeviceptr;
#else
typedef unsigned int CUdeviceptr;
#endif

// Minimal enums
typedef enum
{ /* only need the typedef, values unused at call sites */ } CUresult_placeholder;

typedef enum cudaError_enum
{
  CUDA_SUCCESS = 0,
  // Add others as needed
} CUresult;

typedef enum CUexternalMemoryHandleType_enum
{
  CU_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD = 1,
  CU_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32 = 2,
  CU_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT = 3,
  CU_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_HEAP = 4,
  CU_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE = 5,
  CU_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_RESOURCE = 6,
  CU_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_RESOURCE_KMT = 7,
  CU_EXTERNAL_MEMORY_HANDLE_TYPE_NVSCIBUF = 8,
} CUexternalMemoryHandleType;

typedef enum CUarray_format_enum
{
  CU_AD_FORMAT_UNSIGNED_INT8 = 0x01,
  CU_AD_FORMAT_UNSIGNED_INT16 = 0x02,
  CU_AD_FORMAT_UNSIGNED_INT32 = 0x03,
  CU_AD_FORMAT_SIGNED_INT8 = 0x08,
  CU_AD_FORMAT_SIGNED_INT16 = 0x09,
  CU_AD_FORMAT_SIGNED_INT32 = 0x0a,
  CU_AD_FORMAT_HALF = 0x10,
  CU_AD_FORMAT_FLOAT = 0x20,
} CUarray_format;

typedef enum CUmemorytype_enum
{
  CU_MEMORYTYPE_HOST = 0x01,
  CU_MEMORYTYPE_DEVICE = 0x02,
  CU_MEMORYTYPE_ARRAY = 0x03,
  CU_MEMORYTYPE_UNIFIED = 0x04,
} CUmemorytype;

// Structs
typedef struct CUDA_EXTERNAL_MEMORY_HANDLE_DESC_st
{
  CUexternalMemoryHandleType type;
  union
  {
    int fd;
    struct
    {
      void* handle;
      const void* name;
    } win32;
    const void* nvSciBufObject;
  } handle;
  unsigned long long size;
  unsigned int flags;
  unsigned int reserved[16];
} CUDA_EXTERNAL_MEMORY_HANDLE_DESC;

typedef struct CUDA_ARRAY3D_DESCRIPTOR_st
{
  size_t Width;
  size_t Height;
  size_t Depth;
  CUarray_format Format;
  unsigned int NumChannels;
  unsigned int Flags;
} CUDA_ARRAY3D_DESCRIPTOR;

typedef struct CUDA_EXTERNAL_MEMORY_MIPMAPPED_ARRAY_DESC_st
{
  unsigned long long offset;
  CUDA_ARRAY3D_DESCRIPTOR arrayDesc;
  unsigned int numLevels;
  unsigned int reserved[16];
} CUDA_EXTERNAL_MEMORY_MIPMAPPED_ARRAY_DESC;

typedef struct CUDA_MEMCPY2D_st
{
  size_t srcXInBytes;
  size_t srcY;
  CUmemorytype srcMemoryType;
  const void* srcHost;
  CUdeviceptr srcDevice;
  CUarray srcArray;
  size_t srcPitch;
  size_t dstXInBytes;
  size_t dstY;
  CUmemorytype dstMemoryType;
  void* dstHost;
  CUdeviceptr dstDevice;
  CUarray dstArray;
  size_t dstPitch;
  size_t WidthInBytes;
  size_t Height;
} CUDA_MEMCPY2D;

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
 * @brief Dynamically loaded CUDA driver API functions for external memory interop.
 *
 * Loads libcuda.so.1 at runtime — no CUDA toolkit link dependency.
 */
struct CudaFunctions
{
  void* lib{};

  using FN_cuCtxPushCurrent = CUresult (*)(CUcontext);
  using FN_cuCtxPopCurrent = CUresult (*)(CUcontext*);
  using FN_cuImportExternalMemory
      = CUresult (*)(CUexternalMemory*, const CUDA_EXTERNAL_MEMORY_HANDLE_DESC*);
  using FN_cuExternalMemoryGetMappedMipmappedArray = CUresult (*)(
      CUmipmappedArray*, CUexternalMemory,
      const CUDA_EXTERNAL_MEMORY_MIPMAPPED_ARRAY_DESC*);
  using FN_cuMipmappedArrayGetLevel = CUresult (*)(CUarray*, CUmipmappedArray, unsigned int);
  using FN_cuMemcpy2DAsync = CUresult (*)(const CUDA_MEMCPY2D*, CUstream);
  using FN_cuStreamSynchronize = CUresult (*)(CUstream);
  using FN_cuDestroyExternalMemory = CUresult (*)(CUexternalMemory);
  using FN_cuMipmappedArrayDestroy = CUresult (*)(CUmipmappedArray);

  FN_cuCtxPushCurrent ctxPush{};
  FN_cuCtxPopCurrent ctxPop{};
  FN_cuImportExternalMemory importExtMem{};
  FN_cuExternalMemoryGetMappedMipmappedArray getMapArray{};
  FN_cuMipmappedArrayGetLevel getLevel{};
  FN_cuMemcpy2DAsync memcpy2DAsync{};
  FN_cuStreamSynchronize streamSync{};
  FN_cuDestroyExternalMemory destroyExtMem{};
  FN_cuMipmappedArrayDestroy destroyMipArray{};

  bool load()
  {
#if defined(_WIN32)
    lib = (void*)LoadLibraryA("nvcuda.dll");
    if(!lib)
      return false;
    auto sym = [this](const char* n) { return (void*)GetProcAddress((HMODULE)lib, n); };
#else
    lib = dlopen("libcuda.so.1", RTLD_NOW);
    if(!lib)
      return false;
    auto sym = [this](const char* n) { return dlsym(lib, n); };
#endif

    ctxPush = (FN_cuCtxPushCurrent)sym("cuCtxPushCurrent_v2");
    ctxPop = (FN_cuCtxPopCurrent)sym("cuCtxPopCurrent_v2");
    importExtMem = (FN_cuImportExternalMemory)sym("cuImportExternalMemory");
    getMapArray = (FN_cuExternalMemoryGetMappedMipmappedArray)sym(
        "cuExternalMemoryGetMappedMipmappedArray");
    getLevel = (FN_cuMipmappedArrayGetLevel)sym("cuMipmappedArrayGetLevel");
    memcpy2DAsync = (FN_cuMemcpy2DAsync)sym("cuMemcpy2DAsync_v2");
    streamSync = (FN_cuStreamSynchronize)sym("cuStreamSynchronize");
    destroyExtMem = (FN_cuDestroyExternalMemory)sym("cuDestroyExternalMemory");
    destroyMipArray = (FN_cuMipmappedArrayDestroy)sym("cuMipmappedArrayDestroy");

    return ctxPush && ctxPop && importExtMem && getMapArray && getLevel
           && memcpy2DAsync && streamSync && destroyExtMem && destroyMipArray;
  }

  ~CudaFunctions()
  {
    if(lib)
    {
#if defined(_WIN32)
      FreeLibrary((HMODULE)lib);
#else
      dlclose(lib);
#endif
    }
  }
};

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

  // Vulkan handles (borrowed from QRhi)
  VkDevice m_dev{VK_NULL_HANDLE};
  VkPhysicalDevice m_physDev{VK_NULL_HANDLE};
  QVulkanFunctions* m_funcs{};
  QVulkanDeviceFunctions* m_dfuncs{};
#if defined(_WIN32)
  PFN_vkGetMemoryWin32HandleKHR m_vkGetMemoryWin32HandleKHR{};
#else
  PFN_vkGetMemoryFdKHR m_vkGetMemoryFdKHR{};
#endif

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
    m_funcs = nh->inst->functions();
    m_dfuncs = nh->inst->deviceFunctions(m_dev);
#if defined(_WIN32)
    m_vkGetMemoryWin32HandleKHR = reinterpret_cast<PFN_vkGetMemoryWin32HandleKHR>(
        nh->inst->getInstanceProcAddr("vkGetMemoryWin32HandleKHR"));
#else
    m_vkGetMemoryFdKHR = reinterpret_cast<PFN_vkGetMemoryFdKHR>(
        nh->inst->getInstanceProcAddr("vkGetMemoryFdKHR"));
#endif

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

    for(auto& p : m_planes)
    {
      if(p.image != VK_NULL_HANDLE)
        m_dfuncs->vkDestroyImage(m_dev, p.image, nullptr);
      if(p.memory != VK_NULL_HANDLE)
        m_dfuncs->vkFreeMemory(m_dev, p.memory, nullptr);
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

    // --- Create VkImage with external memory export ---

    VkExternalMemoryImageCreateInfo extInfo{};
    extInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
#if defined(_WIN32)
    extInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
#else
    extInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
#endif

    VkImageCreateInfo imgInfo{};
    imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.pNext = &extInfo;
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.format = vkFmt;
    imgInfo.extent = {static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1};
    imgInfo.mipLevels = 1;
    imgInfo.arrayLayers = 1;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if(m_dfuncs->vkCreateImage(m_dev, &imgInfo, nullptr, &plane.image) != VK_SUCCESS)
      return false;

    // --- Allocate exportable device-local memory ---

    VkMemoryRequirements memReqs{};
    m_dfuncs->vkGetImageMemoryRequirements(m_dev, plane.image, &memReqs);

    VkPhysicalDeviceMemoryProperties memProps{};
    m_funcs->vkGetPhysicalDeviceMemoryProperties(m_physDev, &memProps);

    uint32_t memTypeIdx = UINT32_MAX;
    for(uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
    {
      if((memReqs.memoryTypeBits & (1u << i))
         && (memProps.memoryTypes[i].propertyFlags
             & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
      {
        memTypeIdx = i;
        break;
      }
    }
    if(memTypeIdx == UINT32_MAX)
    {
      m_dfuncs->vkDestroyImage(m_dev, plane.image, nullptr);
      plane.image = VK_NULL_HANDLE;
      return false;
    }

    VkExportMemoryAllocateInfo exportInfo{};
    exportInfo.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
#if defined(_WIN32)
    exportInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
#else
    exportInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
#endif

    VkMemoryDedicatedAllocateInfo dedicatedInfo{};
    dedicatedInfo.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
    dedicatedInfo.pNext = &exportInfo;
    dedicatedInfo.image = plane.image;

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.pNext = &dedicatedInfo;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = memTypeIdx;

    if(m_dfuncs->vkAllocateMemory(m_dev, &allocInfo, nullptr, &plane.memory) != VK_SUCCESS)
    {
      m_dfuncs->vkDestroyImage(m_dev, plane.image, nullptr);
      plane.image = VK_NULL_HANDLE;
      return false;
    }

    plane.memSize = memReqs.size;

    if(m_dfuncs->vkBindImageMemory(m_dev, plane.image, plane.memory, 0) != VK_SUCCESS)
    {
      m_dfuncs->vkFreeMemory(m_dev, plane.memory, nullptr);
      m_dfuncs->vkDestroyImage(m_dev, plane.image, nullptr);
      plane.image = VK_NULL_HANDLE;
      plane.memory = VK_NULL_HANDLE;
      return false;
    }

    // --- Export Vulkan memory and import into CUDA ---

    m_cu.ctxPush(m_cuCtx);

#if defined(_WIN32)
    // --- Export Vulkan memory as Win32 HANDLE ---

    VkMemoryGetWin32HandleInfoKHR getHandleInfo{};
    getHandleInfo.sType = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR;
    getHandleInfo.memory = plane.memory;
    getHandleInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;

    HANDLE handle = nullptr;
    if(m_vkGetMemoryWin32HandleKHR(m_dev, &getHandleInfo, &handle) != VK_SUCCESS
       || !handle)
    {
      CUcontext dummy{};
      m_cu.ctxPop(&dummy);
      return false;
    }

    // --- Import Win32 handle into CUDA as external memory ---

    CUDA_EXTERNAL_MEMORY_HANDLE_DESC memDesc{};
    memDesc.type = CU_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32;
    memDesc.handle.win32.handle = handle;
    memDesc.size = plane.memSize;

    if(m_cu.importExtMem(&plane.cuExtMem, &memDesc) != CUDA_SUCCESS)
    {
      CloseHandle(handle);
      CUcontext dummy{};
      m_cu.ctxPop(&dummy);
      return false;
    }
    // CUDA does not take ownership of Win32 handles — close after import
    CloseHandle(handle);
#else
    // --- Export Vulkan memory as fd ---

    VkMemoryGetFdInfoKHR getFdInfo{};
    getFdInfo.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
    getFdInfo.memory = plane.memory;
    getFdInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;

    int fd = -1;
    if(m_vkGetMemoryFdKHR(m_dev, &getFdInfo, &fd) != VK_SUCCESS || fd < 0)
    {
      CUcontext dummy{};
      m_cu.ctxPop(&dummy);
      return false;
    }

    // --- Import fd into CUDA as external memory ---

    CUDA_EXTERNAL_MEMORY_HANDLE_DESC memDesc{};
    memDesc.type = CU_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD;
    memDesc.handle.fd = fd; // CUDA takes ownership of fd
    memDesc.size = plane.memSize;

    if(m_cu.importExtMem(&plane.cuExtMem, &memDesc) != CUDA_SUCCESS)
    {
      close(fd); // Only close if import failed (ownership not transferred)
      CUcontext dummy{};
      m_cu.ctxPop(&dummy);
      return false;
    }
    // fd is now owned by CUDA — do not close
#endif

    // --- Map to CUDA mipmapped array ---

    CUDA_EXTERNAL_MEMORY_MIPMAPPED_ARRAY_DESC mipDesc{};
    mipDesc.offset = 0;
    mipDesc.arrayDesc.Width = static_cast<size_t>(w);
    mipDesc.arrayDesc.Height = static_cast<size_t>(h);
    mipDesc.arrayDesc.Depth = 0; // 2D
    mipDesc.arrayDesc.Format = (bytesPerChannel == 2) ? CU_AD_FORMAT_UNSIGNED_INT16
                                                      : CU_AD_FORMAT_UNSIGNED_INT8;
    mipDesc.arrayDesc.NumChannels = static_cast<unsigned int>(numChannels);
    mipDesc.arrayDesc.Flags = 0;
    mipDesc.numLevels = 1;

    if(m_cu.getMapArray(&plane.cuMipArray, plane.cuExtMem, &mipDesc) != CUDA_SUCCESS)
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

    // --- Wrap VkImage in QRhiTexture ---
    // createFrom() replaces the QRhi-owned VkImage with ours (non-owning).
    // We set layout to GENERAL since CUDA will write externally.
    samplers[idx].texture->createFrom(
        QRhiTexture::NativeTexture{quint64(plane.image), VK_IMAGE_LAYOUT_GENERAL});

    m_interopReady = (idx == 1); // Ready after both planes are set up
    return true;
  }
};

} // namespace score::gfx

#endif // SCORE_HAS_CUDA_HWCONTEXT && QT_HAS_VULKAN && QT_VERSION >= 6.6
