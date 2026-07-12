#pragma once

/**
 * @file CudaFunctions.hpp
 * @brief Shared dlopen'd CUDA driver-API table for score-plugin-gfx.
 *
 * Avoids the CUDAToolkit link-time dependency: we declare the subset of
 * CUDA types/structs/enums we use, then load libcuda.so.1 (Linux) or
 * nvcuda.dll (Windows) at runtime and resolve every entry point through
 * dlsym/GetProcAddress.
 *
 * Two consumers in-tree:
 *   - Gfx/Graph/decoders/HWCUDA.hpp — zero-copy NVDEC → Vulkan path
 *   - Gfx/Graph/interop/CudaInterop.{h,cpp} — vendor-neutral CUDA interop
 *     (AJA RDMA, planned Magewell + Rivermax)
 *
 * Both embed a `CudaFunctions` and call `load()` once. The function
 * pointers are versioned symbol names (`_v2` where appropriate) — see
 * CUDA Driver API headers for the canonical naming.
 */

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include <cstddef>
#include <cstdint>

#if defined(_WIN32)
// D3D11 interop driver-API entry points take ID3D11Resource*; consumers
// that want the Windows interop path must include <d3d11.h> themselves.
struct ID3D11Resource;
#endif

extern "C" {

// =============================================================================
// CUDA driver-API types (subset used by score-plugin-gfx)
//
// Forward-declared so consumers don't need the CUDA toolkit. The opaque
// pointer types match libcuda's ABI; the enum/struct values match the
// values in <cuda.h> 12.x.
// =============================================================================

// Opaque handles
typedef struct CUctx_st* CUcontext;
typedef struct CUstream_st* CUstream;
typedef struct CUmod_st* CUmodule;
typedef struct CUfunc_st* CUfunction;
typedef struct CUextMemory_st* CUexternalMemory;
typedef struct CUextSemaphore_st* CUexternalSemaphore;
typedef struct CUmipmappedArray_st* CUmipmappedArray;
typedef struct CUarray_st* CUarray;
typedef struct CUgraphicsResource_st* CUgraphicsResource;
typedef int CUdevice;

#if defined(_WIN64) || defined(__LP64__)
typedef unsigned long long CUdeviceptr;
#else
typedef unsigned int CUdeviceptr;
#endif

// Enums
typedef enum cudaError_enum
{
  CUDA_SUCCESS = 0,
  CUDA_ERROR_INVALID_VALUE = 1,
  CUDA_ERROR_OUT_OF_MEMORY = 2,
  CUDA_ERROR_NOT_INITIALIZED = 3,
  CUDA_ERROR_NO_DEVICE = 100,
  CUDA_ERROR_INVALID_DEVICE = 101,
  CUDA_ERROR_UNKNOWN = 999,
} CUresult;

typedef enum CUdevice_attribute_enum
{
  CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR = 75,
  CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR = 76,
  CU_DEVICE_ATTRIBUTE_UNIFIED_ADDRESSING = 41,
  // Whether cuMemGetHandleForAddressRange(..., DMA_BUF_FD) is usable on
  // this device. 0 on Turing/Ada consumer + Quadro RTX parts under driver
  // 595 (empirically: Quadro RTX 4000 and GeForce RTX 4090 both report 0),
  // so CUDA→dma-buf export to Vulkan/GL is unavailable there. Non-zero on
  // data-center parts (A100/H100 class) with a dma-buf-capable kernel.
  CU_DEVICE_ATTRIBUTE_DMA_BUF_SUPPORTED = 124,
} CUdevice_attribute;

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

typedef enum CUexternalSemaphoreHandleType_enum
{
  CU_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD = 1,
  CU_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32 = 2,
  CU_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_KMT = 3,
  CU_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE = 4,
  CU_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D11_FENCE = 5,
  CU_EXTERNAL_SEMAPHORE_HANDLE_TYPE_NVSCISYNC = 6,
  CU_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D11_KEYED_MUTEX = 7,
  CU_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D11_KEYED_MUTEX_KMT = 8,
  CU_EXTERNAL_SEMAPHORE_HANDLE_TYPE_TIMELINE_SEMAPHORE_FD = 9,
  CU_EXTERNAL_SEMAPHORE_HANDLE_TYPE_TIMELINE_SEMAPHORE_WIN32 = 10,
} CUexternalSemaphoreHandleType;

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

// -- VMM (Virtual Memory Management) — CUDA 10.2+ ----------------------
// Used by Rivermax-style direct allocation: cuMemCreate produces a
// physical handle on the GPU, cuMemAddressReserve carves a virtual
// range, cuMemMap binds them, cuMemSetAccess grants R/W. The resulting
// CUdeviceptr is BAR1-mappable and accepted by GPUDirect-RDMA-class
// peers (ConnectX NIC, AJA card with AJA_RDMA, etc).

typedef unsigned long long CUmemGenericAllocationHandle;

typedef enum CUmemAllocationType_enum
{
  CU_MEM_ALLOCATION_TYPE_INVALID = 0,
  CU_MEM_ALLOCATION_TYPE_PINNED = 1,
} CUmemAllocationType;

typedef enum CUmemAllocationHandleType_enum
{
  CU_MEM_HANDLE_TYPE_NONE = 0,
  CU_MEM_HANDLE_TYPE_POSIX_FILE_DESCRIPTOR = 1,
  CU_MEM_HANDLE_TYPE_WIN32 = 2,
  CU_MEM_HANDLE_TYPE_WIN32_KMT = 4,
} CUmemAllocationHandleType;

typedef enum CUmemLocationType_enum
{
  CU_MEM_LOCATION_TYPE_INVALID = 0,
  CU_MEM_LOCATION_TYPE_DEVICE = 1,
} CUmemLocationType;

typedef enum CUmemAccess_flags_enum
{
  CU_MEM_ACCESS_FLAGS_PROT_NONE = 0,
  CU_MEM_ACCESS_FLAGS_PROT_READ = 1,
  CU_MEM_ACCESS_FLAGS_PROT_READWRITE = 3,
} CUmemAccess_flags;

typedef enum CUmemAllocationGranularity_flags_enum
{
  CU_MEM_ALLOC_GRANULARITY_MINIMUM = 0,
  CU_MEM_ALLOC_GRANULARITY_RECOMMENDED = 1,
} CUmemAllocationGranularity_flags;

// Handle type for cuMemGetHandleForAddressRange — export a *mapped* device
// VA range (VMM- or cuMemAlloc-backed) as an OS handle. Unlike
// cuMemExportToShareableHandle (opaque POSIX fd only), this can produce a
// dma-buf fd, which is the cross-API handle type Vulkan can import as an
// aliasing VkImage (VK_EXT_external_memory_dma_buf). Gated on
// CU_DEVICE_ATTRIBUTE_DMA_BUF_SUPPORTED.
typedef enum CUmemRangeHandleType_enum
{
  CU_MEM_RANGE_HANDLE_TYPE_DMA_BUF_FD = 0x1,
} CUmemRangeHandleType;

// Optional flag for cuMemGetHandleForAddressRange: request a PCIe (BAR1)
// mapping type for the exported dma-buf rather than the default.
enum
{
  CU_MEM_RANGE_FLAG_DMA_BUF_MAPPING_TYPE_PCIE = 0x1,
};

typedef struct CUmemLocation_st
{
  CUmemLocationType type;
  int id;
} CUmemLocation;

typedef struct CUmemAllocationProp_st
{
  CUmemAllocationType type;
  CUmemAllocationHandleType requestedHandleTypes;
  CUmemLocation location;
  void* win32HandleMetaData;
  struct
  {
    unsigned char compressionType;
    unsigned char gpuDirectRDMACapable;
    unsigned short usage;
    unsigned char reserved[4];
  } allocFlags;
} CUmemAllocationProp;

typedef struct CUmemAccessDesc_st
{
  CUmemLocation location;
  CUmemAccess_flags flags;
} CUmemAccessDesc;

// Graphics-resource registration flags
#define CU_GRAPHICS_REGISTER_FLAGS_NONE             0x00
#define CU_GRAPHICS_REGISTER_FLAGS_READ_ONLY        0x01
#define CU_GRAPHICS_REGISTER_FLAGS_WRITE_DISCARD    0x02
#define CU_GRAPHICS_REGISTER_FLAGS_SURFACE_LDST     0x04
#define CU_GRAPHICS_REGISTER_FLAGS_TEXTURE_GATHER   0x08

// Primary context retain/release flag bits
#define CU_CTX_SCHED_AUTO 0x00

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

typedef struct CUDA_EXTERNAL_MEMORY_BUFFER_DESC_st
{
  unsigned long long offset;
  unsigned long long size;
  unsigned int flags;
  unsigned int reserved[16];
} CUDA_EXTERNAL_MEMORY_BUFFER_DESC;

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

typedef struct CUDA_EXTERNAL_SEMAPHORE_HANDLE_DESC_st
{
  CUexternalSemaphoreHandleType type;
  union
  {
    int fd;
    struct
    {
      void* handle;
      const void* name;
    } win32;
    const void* nvSciSyncObj;
  } handle;
  unsigned int flags;
  unsigned int reserved[16];
} CUDA_EXTERNAL_SEMAPHORE_HANDLE_DESC;

typedef struct CUDA_EXTERNAL_SEMAPHORE_WAIT_PARAMS_st
{
  struct
  {
    struct
    {
      unsigned long long value;
    } fence;
    union
    {
      void* fence_reserved;
      unsigned long long reserved_a[2];
    } nvSciSync;
    struct
    {
      unsigned long long key;
      unsigned int timeoutMs;
    } keyedMutex;
    unsigned int reserved[10];
  } params;
  unsigned int flags;
  unsigned int reserved[16];
} CUDA_EXTERNAL_SEMAPHORE_WAIT_PARAMS;

} // extern "C"

namespace score::gfx
{

/**
 * @brief Runtime-loaded CUDA driver-API table. Owns the libcuda handle.
 *
 * Usage:
 *   CudaFunctions cu;
 *   if(!cu.load()) { ...no CUDA driver available... }
 *   cu.init(0);          // cuInit
 *   cu.deviceGet(...);
 *   cu.ctxSetCurrent(c);
 *
 * One instance per consumer is fine — dlopen is refcounted by the loader,
 * and the table itself is ~30 pointers. If a shared instance is desired
 * later, swap to a singleton accessor without changing call sites.
 */
struct CudaFunctions
{
  void* lib{};

  // -- Context / device ---------------------------------------------------
  using FN_cuInit = CUresult (*)(unsigned int);
  using FN_cuDeviceGetCount = CUresult (*)(int*);
  using FN_cuDeviceGet = CUresult (*)(CUdevice*, int);
  using FN_cuDeviceGetAttribute = CUresult (*)(int*, CUdevice_attribute, CUdevice);
  using FN_cuDevicePrimaryCtxRetain = CUresult (*)(CUcontext*, CUdevice);
  using FN_cuDevicePrimaryCtxRelease = CUresult (*)(CUdevice);
  using FN_cuCtxSetCurrent = CUresult (*)(CUcontext);
  using FN_cuCtxGetDevice = CUresult (*)(CUdevice*);
  using FN_cuCtxPushCurrent = CUresult (*)(CUcontext);
  using FN_cuCtxPopCurrent = CUresult (*)(CUcontext*);

  FN_cuInit init{};
  FN_cuDeviceGetCount deviceGetCount{};
  FN_cuDeviceGet deviceGet{};
  FN_cuDeviceGetAttribute deviceGetAttribute{};
  FN_cuDevicePrimaryCtxRetain primaryCtxRetain{};
  FN_cuDevicePrimaryCtxRelease primaryCtxRelease{};
  FN_cuCtxSetCurrent ctxSetCurrent{};
  FN_cuCtxGetDevice ctxGetDevice{};
  FN_cuCtxPushCurrent ctxPush{};
  FN_cuCtxPopCurrent ctxPop{};

  // -- Stream -------------------------------------------------------------
  using FN_cuStreamCreate = CUresult (*)(CUstream*, unsigned int);
  using FN_cuStreamDestroy = CUresult (*)(CUstream);
  using FN_cuStreamSynchronize = CUresult (*)(CUstream);

  FN_cuStreamCreate streamCreate{};
  FN_cuStreamDestroy streamDestroy{};
  FN_cuStreamSynchronize streamSync{};

  // -- Errors -------------------------------------------------------------
  using FN_cuGetErrorString = CUresult (*)(CUresult, const char**);
  FN_cuGetErrorString getErrorString{};

  // -- Graphics interop (D3D11 / OpenGL / generic) -----------------------
  using FN_cuGraphicsMapResources
      = CUresult (*)(unsigned int, CUgraphicsResource*, CUstream);
  using FN_cuGraphicsUnmapResources
      = CUresult (*)(unsigned int, CUgraphicsResource*, CUstream);
  using FN_cuGraphicsUnregisterResource = CUresult (*)(CUgraphicsResource);
  using FN_cuGraphicsResourceGetMappedPointer
      = CUresult (*)(CUdeviceptr*, size_t*, CUgraphicsResource);

  FN_cuGraphicsMapResources graphicsMap{};
  FN_cuGraphicsUnmapResources graphicsUnmap{};
  FN_cuGraphicsUnregisterResource graphicsUnregister{};
  FN_cuGraphicsResourceGetMappedPointer graphicsGetMappedPointer{};

#if defined(_WIN32)
  using FN_cuGraphicsD3D11RegisterResource
      = CUresult (*)(CUgraphicsResource*, ID3D11Resource*, unsigned int);
  FN_cuGraphicsD3D11RegisterResource graphicsD3D11Register{};
#endif

  using FN_cuGraphicsGLRegisterBuffer
      = CUresult (*)(CUgraphicsResource*, unsigned int, unsigned int);
  FN_cuGraphicsGLRegisterBuffer graphicsGLRegisterBuffer{};

  // Register a GL *texture* (image) for CUDA interop and fetch its level-0
  // CUarray so a cuMemcpy2D can blit straight into the texture (one
  // DtoD→array copy instead of DtoD→SSBO + glTexSubImage2D). Driver-API
  // symbols in libcuda.so; OPTIONAL
  // (null when the driver/GL-interop pairing doesn't expose them — callers
  // null-check and fall back to the buffer+PBO path).
  using FN_cuGraphicsGLRegisterImage = CUresult (*)(
      CUgraphicsResource*, unsigned int /*image*/, unsigned int /*target*/,
      unsigned int /*flags*/);
  FN_cuGraphicsGLRegisterImage graphicsGLRegisterImage{};

  using FN_cuGraphicsSubResourceGetMappedArray = CUresult (*)(
      CUarray*, CUgraphicsResource, unsigned int /*arrayIndex*/,
      unsigned int /*mipLevel*/);
  FN_cuGraphicsSubResourceGetMappedArray graphicsSubResourceGetMappedArray{};

  // -- External memory ----------------------------------------------------
  using FN_cuImportExternalMemory
      = CUresult (*)(CUexternalMemory*, const CUDA_EXTERNAL_MEMORY_HANDLE_DESC*);
  using FN_cuExternalMemoryGetMappedBuffer = CUresult (*)(
      CUdeviceptr*, CUexternalMemory, const CUDA_EXTERNAL_MEMORY_BUFFER_DESC*);
  using FN_cuExternalMemoryGetMappedMipmappedArray = CUresult (*)(
      CUmipmappedArray*, CUexternalMemory,
      const CUDA_EXTERNAL_MEMORY_MIPMAPPED_ARRAY_DESC*);
  using FN_cuMipmappedArrayGetLevel
      = CUresult (*)(CUarray*, CUmipmappedArray, unsigned int);
  using FN_cuMipmappedArrayDestroy = CUresult (*)(CUmipmappedArray);
  using FN_cuDestroyExternalMemory = CUresult (*)(CUexternalMemory);

  FN_cuImportExternalMemory importExtMem{};
  FN_cuExternalMemoryGetMappedBuffer extMemGetMappedBuffer{};
  FN_cuExternalMemoryGetMappedMipmappedArray getMapArray{};
  FN_cuMipmappedArrayGetLevel getLevel{};
  FN_cuMipmappedArrayDestroy destroyMipArray{};
  FN_cuDestroyExternalMemory destroyExtMem{};

  // -- External semaphores -----------------------------------------------
  using FN_cuImportExternalSemaphore = CUresult (*)(
      CUexternalSemaphore*, const CUDA_EXTERNAL_SEMAPHORE_HANDLE_DESC*);
  using FN_cuWaitExternalSemaphoresAsync = CUresult (*)(
      const CUexternalSemaphore*,
      const CUDA_EXTERNAL_SEMAPHORE_WAIT_PARAMS*, unsigned int, CUstream);
  using FN_cuDestroyExternalSemaphore = CUresult (*)(CUexternalSemaphore);

  FN_cuImportExternalSemaphore importExtSem{};
  FN_cuWaitExternalSemaphoresAsync waitExtSems{};
  FN_cuDestroyExternalSemaphore destroyExtSem{};

  // -- Memcpy -------------------------------------------------------------
  using FN_cuMemcpy2DAsync = CUresult (*)(const CUDA_MEMCPY2D*, CUstream);
  FN_cuMemcpy2DAsync memcpy2DAsync{};
  using FN_cuMemcpyHtoD = CUresult (*)(CUdeviceptr, const void*, size_t);
  FN_cuMemcpyHtoD memcpyHtoD{};
  using FN_cuMemcpyDtoH = CUresult (*)(void*, CUdeviceptr, size_t);
  FN_cuMemcpyDtoH memcpyDtoH{};
  using FN_cuMemcpyDtoDAsync
      = CUresult (*)(CUdeviceptr, CUdeviceptr, size_t, CUstream);
  FN_cuMemcpyDtoDAsync memcpyDtoDAsync{};

  // -- Linear device allocation --------------------------------------------
  // cuMemAlloc'd memory is what third-party kernel DMA engines can pin
  // (nvidia_p2p_get_pages accepts only CUDA-allocator VA ranges — never
  // GL/D3D-owned memory mapped into CUDA).
  using FN_cuMemAlloc = CUresult (*)(CUdeviceptr*, size_t);
  using FN_cuMemFree = CUresult (*)(CUdeviceptr);
  FN_cuMemAlloc memAlloc{};
  FN_cuMemFree memFree{};

  // -- VMM (Virtual Memory Management) — CUDA 10.2+, OPTIONAL ------------
  // All-or-nothing: either every entry point resolves and `vmmSupported`
  // is true, or the bundle is unavailable and consumers fall back to
  // cuMemAlloc + non-RDMA paths.
  using FN_cuMemCreate
      = CUresult (*)(CUmemGenericAllocationHandle*, size_t,
                     const CUmemAllocationProp*, unsigned long long);
  using FN_cuMemAddressReserve
      = CUresult (*)(CUdeviceptr*, size_t, size_t, CUdeviceptr,
                     unsigned long long);
  using FN_cuMemMap
      = CUresult (*)(CUdeviceptr, size_t, size_t,
                     CUmemGenericAllocationHandle, unsigned long long);
  using FN_cuMemSetAccess
      = CUresult (*)(CUdeviceptr, size_t, const CUmemAccessDesc*, size_t);
  using FN_cuMemUnmap = CUresult (*)(CUdeviceptr, size_t);
  using FN_cuMemAddressFree = CUresult (*)(CUdeviceptr, size_t);
  using FN_cuMemRelease = CUresult (*)(CUmemGenericAllocationHandle);
  using FN_cuMemExportToShareableHandle
      = CUresult (*)(void*, CUmemGenericAllocationHandle, int, unsigned long long);
  using FN_cuMemGetAllocationGranularity
      = CUresult (*)(size_t*, const CUmemAllocationProp*,
                     CUmemAllocationGranularity_flags);

  FN_cuMemCreate memCreate{};
  FN_cuMemAddressReserve memAddressReserve{};
  FN_cuMemMap memMap{};
  FN_cuMemSetAccess memSetAccess{};
  FN_cuMemUnmap memUnmap{};
  FN_cuMemAddressFree memAddressFree{};
  FN_cuMemRelease memRelease{};
  /// Optional (not part of the vmmSupported bundle): export a VMM
  /// allocation as an OS shareable handle (POSIX fd / NT handle) for
  /// Vulkan/D3D import. Null on very old drivers.
  FN_cuMemExportToShareableHandle memExportToShareableHandle{};
  FN_cuMemGetAllocationGranularity memGetGranularity{};

  /// Optional: export a mapped device VA range as a dma-buf fd (the
  /// cross-API handle type importable into Vulkan/GL as an aliasing image).
  /// Present in libcuda since 11.7, but only *usable* when the device
  /// reports CU_DEVICE_ATTRIBUTE_DMA_BUF_SUPPORTED != 0 — otherwise it
  /// returns CUDA_ERROR_NOT_SUPPORTED (801). Null on very old drivers.
  using FN_cuMemGetHandleForAddressRange
      = CUresult (*)(void*, CUdeviceptr, size_t, CUmemRangeHandleType,
                     unsigned long long);
  FN_cuMemGetHandleForAddressRange memGetHandleForAddressRange{};

  // -- Pointer attributes — OPTIONAL --------------------------------------
  // cuPointerSetAttribute(CU_POINTER_ATTRIBUTE_SYNC_MEMOPS) marks a device
  // range for synchronous memory ops so third-party DMA engines (Deltacast
  // VHD_CreateSlotEx RDMAEnabled, Rivermax, ...) can target it. Null on very
  // old drivers; callers null-check.
  using FN_cuPointerSetAttribute
      = CUresult (*)(const void*, int /*CUpointer_attribute*/, CUdeviceptr);
  FN_cuPointerSetAttribute pointerSetAttribute{};

  /** True when every VMM entry point resolved. False on pre-CUDA-10.2
   *  drivers; callers must check before calling memCreate et al. */
  bool vmmSupported{};

  bool loaded() const noexcept { return lib != nullptr; }

  /**
   * @brief Load libcuda + resolve all symbols. Idempotent.
   * @return true if the library opened and all REQUIRED symbols resolved.
   *         Optional symbols (D3D11/GL interop) may be null on systems
   *         that don't support them — check the corresponding pointer
   *         before use.
   */
  bool load()
  {
    if(lib)
      return true;
#if defined(_WIN32)
    lib = (void*)LoadLibraryA("nvcuda.dll");
    if(!lib)
      return false;
    auto sym = [this](const char* n) {
      return (void*)GetProcAddress((HMODULE)lib, n);
    };
#else
    lib = dlopen("libcuda.so.1", RTLD_NOW);
    if(!lib)
      return false;
    auto sym = [this](const char* n) { return dlsym(lib, n); };
#endif

    // Context / device — REQUIRED
    init = (FN_cuInit)sym("cuInit");
    deviceGetCount = (FN_cuDeviceGetCount)sym("cuDeviceGetCount");
    deviceGet = (FN_cuDeviceGet)sym("cuDeviceGet");
    deviceGetAttribute = (FN_cuDeviceGetAttribute)sym("cuDeviceGetAttribute");
    primaryCtxRetain = (FN_cuDevicePrimaryCtxRetain)sym("cuDevicePrimaryCtxRetain");
    primaryCtxRelease = (FN_cuDevicePrimaryCtxRelease)sym("cuDevicePrimaryCtxRelease_v2");
    ctxSetCurrent = (FN_cuCtxSetCurrent)sym("cuCtxSetCurrent");
    ctxGetDevice = (FN_cuCtxGetDevice)sym("cuCtxGetDevice");
    ctxPush = (FN_cuCtxPushCurrent)sym("cuCtxPushCurrent_v2");
    ctxPop = (FN_cuCtxPopCurrent)sym("cuCtxPopCurrent_v2");

    // Stream — REQUIRED
    streamCreate = (FN_cuStreamCreate)sym("cuStreamCreate");
    streamDestroy = (FN_cuStreamDestroy)sym("cuStreamDestroy_v2");
    streamSync = (FN_cuStreamSynchronize)sym("cuStreamSynchronize");

    // Errors — REQUIRED
    getErrorString = (FN_cuGetErrorString)sym("cuGetErrorString");

    // Graphics interop — REQUIRED (generic ops); D3D11/GL optional
    graphicsMap = (FN_cuGraphicsMapResources)sym("cuGraphicsMapResources");
    graphicsUnmap = (FN_cuGraphicsUnmapResources)sym("cuGraphicsUnmapResources");
    graphicsUnregister
        = (FN_cuGraphicsUnregisterResource)sym("cuGraphicsUnregisterResource");
    graphicsGetMappedPointer = (FN_cuGraphicsResourceGetMappedPointer)sym(
        "cuGraphicsResourceGetMappedPointer_v2");

#if defined(_WIN32)
    graphicsD3D11Register = (FN_cuGraphicsD3D11RegisterResource)sym(
        "cuGraphicsD3D11RegisterResource");
#endif
    graphicsGLRegisterBuffer
        = (FN_cuGraphicsGLRegisterBuffer)sym("cuGraphicsGLRegisterBuffer");
    graphicsGLRegisterImage
        = (FN_cuGraphicsGLRegisterImage)sym("cuGraphicsGLRegisterImage");
    graphicsSubResourceGetMappedArray
        = (FN_cuGraphicsSubResourceGetMappedArray)sym(
            "cuGraphicsSubResourceGetMappedArray");

    // External memory — REQUIRED
    importExtMem = (FN_cuImportExternalMemory)sym("cuImportExternalMemory");
    extMemGetMappedBuffer = (FN_cuExternalMemoryGetMappedBuffer)sym(
        "cuExternalMemoryGetMappedBuffer");
    getMapArray = (FN_cuExternalMemoryGetMappedMipmappedArray)sym(
        "cuExternalMemoryGetMappedMipmappedArray");
    getLevel = (FN_cuMipmappedArrayGetLevel)sym("cuMipmappedArrayGetLevel");
    destroyMipArray = (FN_cuMipmappedArrayDestroy)sym("cuMipmappedArrayDestroy");
    destroyExtMem = (FN_cuDestroyExternalMemory)sym("cuDestroyExternalMemory");

    // External semaphores — REQUIRED
    importExtSem
        = (FN_cuImportExternalSemaphore)sym("cuImportExternalSemaphore");
    waitExtSems = (FN_cuWaitExternalSemaphoresAsync)sym(
        "cuWaitExternalSemaphoresAsync");
    destroyExtSem
        = (FN_cuDestroyExternalSemaphore)sym("cuDestroyExternalSemaphore");

    // Memcpy — REQUIRED
    memcpy2DAsync = (FN_cuMemcpy2DAsync)sym("cuMemcpy2DAsync_v2");
    memcpyHtoD = (FN_cuMemcpyHtoD)sym("cuMemcpyHtoD_v2");
    memcpyDtoH = (FN_cuMemcpyDtoH)sym("cuMemcpyDtoH_v2");
    memcpyDtoDAsync = (FN_cuMemcpyDtoDAsync)sym("cuMemcpyDtoDAsync_v2");

    // Linear alloc — REQUIRED (core API since CUDA 3.2)
    memAlloc = (FN_cuMemAlloc)sym("cuMemAlloc_v2");
    memFree = (FN_cuMemFree)sym("cuMemFree_v2");

    // VMM — OPTIONAL (CUDA 10.2+). All-or-nothing: if any entry is
    // missing, mark the whole bundle unsupported. The driver exports
    // these unversioned on Linux and unversioned on Windows too in
    // 10.2+.
    memCreate = (FN_cuMemCreate)sym("cuMemCreate");
    memAddressReserve = (FN_cuMemAddressReserve)sym("cuMemAddressReserve");
    memMap = (FN_cuMemMap)sym("cuMemMap");
    memSetAccess = (FN_cuMemSetAccess)sym("cuMemSetAccess");
    memUnmap = (FN_cuMemUnmap)sym("cuMemUnmap");
    memAddressFree = (FN_cuMemAddressFree)sym("cuMemAddressFree");
    memRelease = (FN_cuMemRelease)sym("cuMemRelease");
    memExportToShareableHandle
        = (FN_cuMemExportToShareableHandle)sym("cuMemExportToShareableHandle");
    memGetGranularity = (FN_cuMemGetAllocationGranularity)sym(
        "cuMemGetAllocationGranularity");
    memGetHandleForAddressRange = (FN_cuMemGetHandleForAddressRange)sym(
        "cuMemGetHandleForAddressRange");
    pointerSetAttribute
        = (FN_cuPointerSetAttribute)sym("cuPointerSetAttribute");
    vmmSupported = memCreate && memAddressReserve && memMap && memSetAccess
                   && memUnmap && memAddressFree && memRelease
                   && memGetGranularity;

    const bool ok = init && deviceGetCount && deviceGet && deviceGetAttribute
                    && primaryCtxRetain && primaryCtxRelease && ctxSetCurrent
                    && ctxGetDevice && ctxPush && ctxPop && streamCreate
                    && streamDestroy && streamSync && getErrorString
                    && graphicsMap && graphicsUnmap && graphicsUnregister
                    && graphicsGetMappedPointer && importExtMem
                    && extMemGetMappedBuffer && getMapArray && getLevel
                    && destroyMipArray && destroyExtMem && importExtSem
                    && waitExtSems && destroyExtSem && memcpy2DAsync
                    && memcpyHtoD && memcpyDtoH && memcpyDtoDAsync && memAlloc
                    && memFree;
    if(!ok)
    {
      unload();
      return false;
    }
    return true;
  }

  /** True when `device` can export a mapped VA range as a dma-buf fd via
   *  cuMemGetHandleForAddressRange (the CUDA→Vulkan/GL zero-copy aliasing
   *  route). This queries CU_DEVICE_ATTRIBUTE_DMA_BUF_SUPPORTED; it is 0
   *  on the Turing/Ada Quadro+GeForce parts in this rig (driver 595), so
   *  callers MUST check this and fall back to the bounce path — the export
   *  otherwise returns CUDA_ERROR_NOT_SUPPORTED. Requires a current
   *  context on the device. */
  bool dmaBufExportSupported(CUdevice device) const noexcept
  {
    if(!deviceGetAttribute || !memGetHandleForAddressRange)
      return false;
    int v = 0;
    if(deviceGetAttribute(&v, CU_DEVICE_ATTRIBUTE_DMA_BUF_SUPPORTED, device)
       != CUDA_SUCCESS)
      return false;
    return v != 0;
  }

  void unload() noexcept
  {
    if(!lib)
      return;
#if defined(_WIN32)
    FreeLibrary((HMODULE)lib);
#else
    dlclose(lib);
#endif
    lib = nullptr;
  }

  ~CudaFunctions() { unload(); }

  CudaFunctions() = default;
  CudaFunctions(const CudaFunctions&) = delete;
  CudaFunctions& operator=(const CudaFunctions&) = delete;
};

} // namespace score::gfx
