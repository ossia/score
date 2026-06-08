/**
 * @file CudaP2PBridge.cpp
 * @brief Driver-API-only impl of the CudaP2PBridge C API.
 *
 * Uses Gfx/Graph/interop/CudaFunctions.hpp for dlopen'd entry points;
 * no link-time CUDAToolkit dependency. All cu* calls go through the
 * shared CudaFunctions table.
 */

#include <Gfx/Graph/interop/CudaP2PBridge.h>
#include <Gfx/Graph/interop/CudaFunctions.hpp>

#if defined(_WIN32)
#  include <d3d11.h>
#endif

#include <mutex>
#include <string>

using score::gfx::CudaFunctions;

// =============================================================================
// Internal types
// =============================================================================

struct CudaP2PContext_t
{
  CudaFunctions cu;
  CUcontext cuContext{};
  CUstream stream{};
  CUdevice device{0};
  std::string lastError;
  std::mutex mutex;
};

// One of (graphicsResource, externalMemory) is non-null at a time:
//   - graphicsResource : D3D11 / OpenGL graphics-resource interop
//   - externalMemory   : D3D12 / Vulkan external-memory interop
struct CudaP2PResource_t
{
  CUgraphicsResource graphicsResource{};
  CUexternalMemory externalMemory{};
  uint64_t byteSize{};
};

struct CudaP2PSemaphore_t
{
  CUexternalSemaphore handle{};
};

// Image import: backed by external memory + mipmapped array. The bridge
// owns both; the caller gets the level-0 CUarray for memcpy destinations.
struct CudaP2PImage_t
{
  CUexternalMemory externalMemory{};
  CUmipmappedArray mipArray{};
  CUarray levelZeroArray{};
};

// Capture the last error string into ctx and return the mapped error code.
static CudaP2PError reportCuError(
    CudaP2PContextHandle ctx, CUresult r, CudaP2PError mapped,
    const char* what)
{
  if(!ctx)
    return mapped;
  const char* msg = nullptr;
  if(ctx->cu.getErrorString && ctx->cu.getErrorString(r, &msg) == CUDA_SUCCESS
     && msg)
    ctx->lastError = std::string(what) + ": " + msg;
  else
    ctx->lastError = what;
  return mapped;
}

#define CU_CHECK(call, ctx, mapped, what)                       \
  do                                                            \
  {                                                             \
    CUresult _r = (call);                                       \
    if(_r != CUDA_SUCCESS)                                      \
      return reportCuError((ctx), _r, (mapped), (what));        \
  } while(0)

// =============================================================================
// Context management
// =============================================================================

CudaP2PError cuda_p2p_init(CudaP2PContextHandle* outCtx)
{
  if(!outCtx)
    return CUDA_P2P_ERROR_INVALID_PARAMETER;

  *outCtx = nullptr;

  auto* ctx = new CudaP2PContext_t();
  if(!ctx->cu.load())
  {
    ctx->lastError = "Failed to load CUDA driver (libcuda.so.1 / nvcuda.dll)";
    delete ctx;
    return CUDA_P2P_ERROR_NOT_INITIALIZED;
  }

  CU_CHECK(ctx->cu.init(0), ctx, CUDA_P2P_ERROR_INIT_FAILED, "cuInit");

  int deviceCount = 0;
  CU_CHECK(
      ctx->cu.deviceGetCount(&deviceCount), ctx, CUDA_P2P_ERROR_NO_DEVICE,
      "cuDeviceGetCount");
  if(deviceCount <= 0)
  {
    ctx->lastError = "No CUDA-capable devices found";
    delete ctx;
    return CUDA_P2P_ERROR_NO_DEVICE;
  }

  CU_CHECK(
      ctx->cu.deviceGet(&ctx->device, 0), ctx, CUDA_P2P_ERROR_INIT_FAILED,
      "cuDeviceGet");
  CU_CHECK(
      ctx->cu.primaryCtxRetain(&ctx->cuContext, ctx->device), ctx,
      CUDA_P2P_ERROR_INIT_FAILED, "cuDevicePrimaryCtxRetain");
  CU_CHECK(
      ctx->cu.ctxSetCurrent(ctx->cuContext), ctx, CUDA_P2P_ERROR_INIT_FAILED,
      "cuCtxSetCurrent");
  CU_CHECK(
      ctx->cu.streamCreate(&ctx->stream, 0), ctx, CUDA_P2P_ERROR_INIT_FAILED,
      "cuStreamCreate");

  *outCtx = ctx;
  return CUDA_P2P_SUCCESS;
}

void cuda_p2p_shutdown(CudaP2PContextHandle ctx)
{
  if(!ctx)
    return;
  {
    std::lock_guard<std::mutex> lock(ctx->mutex);
    if(ctx->stream && ctx->cu.streamDestroy)
    {
      ctx->cu.streamDestroy(ctx->stream);
      ctx->stream = nullptr;
    }
    if(ctx->cuContext && ctx->cu.primaryCtxRelease)
    {
      ctx->cu.primaryCtxRelease(ctx->device);
      ctx->cuContext = nullptr;
    }
  }
  delete ctx;
}

bool cuda_p2p_available(void)
{
  CudaFunctions cu;
  if(!cu.load())
    return false;
  if(cu.init(0) != CUDA_SUCCESS)
    return false;
  int n = 0;
  if(cu.deviceGetCount(&n) != CUDA_SUCCESS || n == 0)
    return false;
  CUdevice dev{};
  if(cu.deviceGet(&dev, 0) != CUDA_SUCCESS)
    return false;
  // GPUDirect requires compute capability 3.0+ and unified addressing.
  int major = 0, unified = 0;
  if(cu.deviceGetAttribute(
         &major, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, dev)
     != CUDA_SUCCESS)
    return false;
  if(cu.deviceGetAttribute(
         &unified, CU_DEVICE_ATTRIBUTE_UNIFIED_ADDRESSING, dev)
     != CUDA_SUCCESS)
    return false;
  return major >= 3 && unified != 0;
}

const char* cuda_p2p_get_error_string(CudaP2PContextHandle ctx)
{
  if(!ctx)
    return "Invalid context";
  return ctx->lastError.c_str();
}

// =============================================================================
// Shared-buffer importers (one per backend)
// =============================================================================

CudaP2PError cuda_p2p_import_d3d11_buffer(
    CudaP2PContextHandle ctx,
    void* d3d11_buffer,
    void* d3d11_device,
    uint32_t buffer_size,
    void** out_device_ptr,
    CudaP2PResourceHandle* out_handle)
{
#if !defined(_WIN32)
  (void)d3d11_buffer;
  (void)d3d11_device;
  (void)buffer_size;
  (void)out_device_ptr;
  (void)out_handle;
  if(ctx)
    ctx->lastError = "D3D11 interop not available on non-Windows platforms";
  return CUDA_P2P_ERROR_FORMAT_NOT_SUPPORTED;
#else
  (void)d3d11_device; // not needed by driver-API path
  if(!ctx || !d3d11_buffer || !out_device_ptr || !out_handle)
    return CUDA_P2P_ERROR_INVALID_PARAMETER;
  if(!ctx->cu.graphicsD3D11Register)
  {
    ctx->lastError = "cuGraphicsD3D11RegisterResource symbol not available";
    return CUDA_P2P_ERROR_INTEROP_FAILED;
  }

  *out_device_ptr = nullptr;
  *out_handle = nullptr;
  std::lock_guard<std::mutex> lock(ctx->mutex);
  CU_CHECK(
      ctx->cu.ctxSetCurrent(ctx->cuContext), ctx,
      CUDA_P2P_ERROR_INTEROP_FAILED, "cuCtxSetCurrent");

  // Register the SHARED D3D11 buffer with CUDA. Buffers (vs textures)
  // give us a flat device pointer via cuGraphicsResourceGetMappedPointer,
  // which is exactly what AJA's DMABufferLock(inRDMA=true) accepts.
  CUgraphicsResource resource{};
  CU_CHECK(
      ctx->cu.graphicsD3D11Register(
          &resource, static_cast<ID3D11Resource*>(d3d11_buffer),
          CU_GRAPHICS_REGISTER_FLAGS_NONE),
      ctx, CUDA_P2P_ERROR_INTEROP_FAILED,
      "cuGraphicsD3D11RegisterResource");

  // Map once and keep mapped for the resource's lifetime; the peer
  // device needs a stable pointer.
  if(ctx->cu.graphicsMap(1, &resource, ctx->stream) != CUDA_SUCCESS)
  {
    ctx->cu.graphicsUnregister(resource);
    ctx->lastError = "cuGraphicsMapResources(buffer) failed";
    return CUDA_P2P_ERROR_INTEROP_FAILED;
  }

  CUdeviceptr devicePtr{};
  size_t mappedSize = 0;
  if(ctx->cu.graphicsGetMappedPointer(&devicePtr, &mappedSize, resource)
         != CUDA_SUCCESS
     || !devicePtr)
  {
    ctx->cu.graphicsUnmap(1, &resource, ctx->stream);
    ctx->cu.graphicsUnregister(resource);
    ctx->lastError = "cuGraphicsResourceGetMappedPointer failed";
    return CUDA_P2P_ERROR_INTEROP_FAILED;
  }
  if(mappedSize < buffer_size)
  {
    ctx->cu.graphicsUnmap(1, &resource, ctx->stream);
    ctx->cu.graphicsUnregister(resource);
    ctx->lastError = "mapped buffer smaller than requested";
    return CUDA_P2P_ERROR_ALLOC_FAILED;
  }

  auto* h = new CudaP2PResource_t();
  h->graphicsResource = resource;
  h->byteSize = buffer_size;

  *out_device_ptr = reinterpret_cast<void*>(static_cast<uintptr_t>(devicePtr));
  *out_handle = h;
  return CUDA_P2P_SUCCESS;
#endif
}

// External-memory path shared by D3D12 + Vulkan tier-3 buffer importers.
static CudaP2PError importExternalMemoryAsBuffer(
    CudaP2PContextHandle ctx,
    CUexternalMemoryHandleType type,
    void* handle,
    uint64_t bufferSize,
    void** outDevicePtr,
    CudaP2PResourceHandle* outHandle)
{
  if(!ctx || !handle || !outDevicePtr || !outHandle || bufferSize == 0)
    return CUDA_P2P_ERROR_INVALID_PARAMETER;

  *outDevicePtr = nullptr;
  *outHandle = nullptr;
  std::lock_guard<std::mutex> lock(ctx->mutex);

  CU_CHECK(
      ctx->cu.ctxSetCurrent(ctx->cuContext), ctx,
      CUDA_P2P_ERROR_INTEROP_FAILED, "cuCtxSetCurrent");

  CUDA_EXTERNAL_MEMORY_HANDLE_DESC memDesc{};
  memDesc.type = type;
  memDesc.size = bufferSize;
#if defined(_WIN32)
  // On Windows the same union member carries both OpaqueWin32 HANDLEs and
  // D3D12 NT handles.
  memDesc.handle.win32.handle = handle;
#else
  // On Linux OpaqueFd carries an int FD that the caller passed in as a
  // pointer-sized value via cast.
  memDesc.handle.fd = static_cast<int>(reinterpret_cast<intptr_t>(handle));
#endif

  CUexternalMemory extMem{};
  CU_CHECK(
      ctx->cu.importExtMem(&extMem, &memDesc), ctx,
      CUDA_P2P_ERROR_INTEROP_FAILED, "cuImportExternalMemory");

  CUDA_EXTERNAL_MEMORY_BUFFER_DESC bufDesc{};
  bufDesc.offset = 0;
  bufDesc.size = bufferSize;
  bufDesc.flags = 0;

  CUdeviceptr devicePtr{};
  if(ctx->cu.extMemGetMappedBuffer(&devicePtr, extMem, &bufDesc) != CUDA_SUCCESS
     || !devicePtr)
  {
    ctx->cu.destroyExtMem(extMem);
    ctx->lastError = "cuExternalMemoryGetMappedBuffer failed";
    return CUDA_P2P_ERROR_INTEROP_FAILED;
  }

  auto* h = new CudaP2PResource_t();
  h->externalMemory = extMem;
  h->byteSize = bufferSize;

  *outDevicePtr = reinterpret_cast<void*>(static_cast<uintptr_t>(devicePtr));
  *outHandle = h;
  return CUDA_P2P_SUCCESS;
}

CudaP2PError cuda_p2p_import_d3d12_buffer(
    CudaP2PContextHandle ctx,
    void* shared_resource_handle,
    uint64_t buffer_size,
    void** out_device_ptr,
    CudaP2PResourceHandle* out_handle)
{
#if !defined(_WIN32)
  (void)shared_resource_handle;
  (void)buffer_size;
  (void)out_device_ptr;
  (void)out_handle;
  if(ctx)
    ctx->lastError = "D3D12 interop not available on non-Windows platforms";
  return CUDA_P2P_ERROR_FORMAT_NOT_SUPPORTED;
#else
  return importExternalMemoryAsBuffer(
      ctx, CU_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE,
      shared_resource_handle, buffer_size, out_device_ptr, out_handle);
#endif
}

CudaP2PError cuda_p2p_import_vulkan_buffer(
    CudaP2PContextHandle ctx,
    void* external_memory_handle,
    uint64_t buffer_size,
    void** out_device_ptr,
    CudaP2PResourceHandle* out_handle)
{
#if defined(_WIN32)
  const auto t = CU_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32;
#else
  const auto t = CU_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD;
#endif
  return importExternalMemoryAsBuffer(
      ctx, t, external_memory_handle, buffer_size, out_device_ptr, out_handle);
}

CudaP2PError cuda_p2p_import_gl_buffer(
    CudaP2PContextHandle ctx,
    uint32_t gl_buffer_id,
    uint32_t buffer_size,
    void** out_device_ptr,
    CudaP2PResourceHandle* out_handle)
{
  if(!ctx || !out_device_ptr || !out_handle || buffer_size == 0)
    return CUDA_P2P_ERROR_INVALID_PARAMETER;
  if(!ctx->cu.graphicsGLRegisterBuffer)
  {
    ctx->lastError = "cuGraphicsGLRegisterBuffer symbol not available";
    return CUDA_P2P_ERROR_INTEROP_FAILED;
  }

  *out_device_ptr = nullptr;
  *out_handle = nullptr;
  std::lock_guard<std::mutex> lock(ctx->mutex);

  CU_CHECK(
      ctx->cu.ctxSetCurrent(ctx->cuContext), ctx,
      CUDA_P2P_ERROR_INTEROP_FAILED, "cuCtxSetCurrent");

  CUgraphicsResource resource{};
  CU_CHECK(
      ctx->cu.graphicsGLRegisterBuffer(
          &resource, gl_buffer_id, CU_GRAPHICS_REGISTER_FLAGS_NONE),
      ctx, CUDA_P2P_ERROR_INTEROP_FAILED,
      "cuGraphicsGLRegisterBuffer");

  if(ctx->cu.graphicsMap(1, &resource, ctx->stream) != CUDA_SUCCESS)
  {
    ctx->cu.graphicsUnregister(resource);
    ctx->lastError = "cuGraphicsMapResources(GL buffer) failed";
    return CUDA_P2P_ERROR_INTEROP_FAILED;
  }

  CUdeviceptr devicePtr{};
  size_t mappedSize = 0;
  if(ctx->cu.graphicsGetMappedPointer(&devicePtr, &mappedSize, resource)
         != CUDA_SUCCESS
     || !devicePtr)
  {
    ctx->cu.graphicsUnmap(1, &resource, ctx->stream);
    ctx->cu.graphicsUnregister(resource);
    ctx->lastError = "cuGraphicsResourceGetMappedPointer(GL) failed";
    return CUDA_P2P_ERROR_INTEROP_FAILED;
  }
  if(mappedSize < buffer_size)
  {
    ctx->cu.graphicsUnmap(1, &resource, ctx->stream);
    ctx->cu.graphicsUnregister(resource);
    ctx->lastError = "GL buffer mapping smaller than requested";
    return CUDA_P2P_ERROR_ALLOC_FAILED;
  }

  auto* h = new CudaP2PResource_t();
  h->graphicsResource = resource;
  h->byteSize = buffer_size;

  *out_device_ptr = reinterpret_cast<void*>(static_cast<uintptr_t>(devicePtr));
  *out_handle = h;
  return CUDA_P2P_SUCCESS;
}

void cuda_p2p_release_buffer(CudaP2PContextHandle ctx, CudaP2PResourceHandle h)
{
  if(!ctx || !h)
    return;
  std::lock_guard<std::mutex> lock(ctx->mutex);

  if(h->graphicsResource)
  {
    ctx->cu.graphicsUnmap(1, &h->graphicsResource, ctx->stream);
    ctx->cu.graphicsUnregister(h->graphicsResource);
  }
  if(h->externalMemory)
  {
    ctx->cu.destroyExtMem(h->externalMemory);
  }
  delete h;
}

// =============================================================================
// Shared-image importer (Vulkan external-memory → CUmipmappedArray)
// =============================================================================

CudaP2PError cuda_p2p_import_vulkan_image(
    CudaP2PContextHandle ctx,
    void* external_memory_handle,
    uint64_t memory_size,
    const CudaP2PImageDesc* desc,
    uint64_t offset_in_memory,
    void** out_cuda_array,
    CudaP2PImageHandle* out_handle)
{
  if(!ctx || !external_memory_handle || !desc || !out_cuda_array
     || !out_handle || memory_size == 0 || desc->width == 0
     || desc->height == 0 || desc->numChannels == 0)
    return CUDA_P2P_ERROR_INVALID_PARAMETER;

  *out_cuda_array = nullptr;
  *out_handle = nullptr;
  std::lock_guard<std::mutex> lock(ctx->mutex);

  CU_CHECK(
      ctx->cu.ctxSetCurrent(ctx->cuContext), ctx,
      CUDA_P2P_ERROR_INTEROP_FAILED, "cuCtxSetCurrent");

  // Import the memory as external — same handle-type selection as
  // cuda_p2p_import_vulkan_buffer.
  CUDA_EXTERNAL_MEMORY_HANDLE_DESC memDesc{};
#if defined(_WIN32)
  memDesc.type = CU_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32;
  memDesc.handle.win32.handle = external_memory_handle;
#else
  memDesc.type = CU_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD;
  memDesc.handle.fd
      = static_cast<int>(reinterpret_cast<intptr_t>(external_memory_handle));
#endif
  memDesc.size = memory_size;

  CUexternalMemory extMem{};
  CU_CHECK(
      ctx->cu.importExtMem(&extMem, &memDesc), ctx,
      CUDA_P2P_ERROR_INTEROP_FAILED, "cuImportExternalMemory(image)");

  // Materialize as a single-level mipmapped array.
  CUDA_EXTERNAL_MEMORY_MIPMAPPED_ARRAY_DESC mipDesc{};
  mipDesc.offset = offset_in_memory;
  mipDesc.numLevels = 1;
  mipDesc.arrayDesc.Width = desc->width;
  mipDesc.arrayDesc.Height = desc->height;
  mipDesc.arrayDesc.Depth = desc->depth;
  mipDesc.arrayDesc.NumChannels = desc->numChannels;
  mipDesc.arrayDesc.Format = static_cast<CUarray_format>(desc->format);
  mipDesc.arrayDesc.Flags = desc->flags;

  CUmipmappedArray mipArray{};
  if(ctx->cu.getMapArray(&mipArray, extMem, &mipDesc) != CUDA_SUCCESS)
  {
    ctx->cu.destroyExtMem(extMem);
    ctx->lastError = "cuExternalMemoryGetMappedMipmappedArray failed";
    return CUDA_P2P_ERROR_INTEROP_FAILED;
  }

  CUarray levelZero{};
  if(ctx->cu.getLevel(&levelZero, mipArray, 0) != CUDA_SUCCESS)
  {
    ctx->cu.destroyMipArray(mipArray);
    ctx->cu.destroyExtMem(extMem);
    ctx->lastError = "cuMipmappedArrayGetLevel(0) failed";
    return CUDA_P2P_ERROR_INTEROP_FAILED;
  }

  auto* h = new CudaP2PImage_t();
  h->externalMemory = extMem;
  h->mipArray = mipArray;
  h->levelZeroArray = levelZero;

  *out_cuda_array = levelZero;
  *out_handle = h;
  return CUDA_P2P_SUCCESS;
}

void cuda_p2p_release_image(CudaP2PContextHandle ctx, CudaP2PImageHandle h)
{
  if(!ctx || !h)
    return;
  std::lock_guard<std::mutex> lock(ctx->mutex);

  if(h->mipArray)
    ctx->cu.destroyMipArray(h->mipArray);
  if(h->externalMemory)
    ctx->cu.destroyExtMem(h->externalMemory);
  delete h;
}

// =============================================================================
// External semaphores (D3D12 fence + Vulkan timeline)
// =============================================================================

static CudaP2PError importSemaphore(
    CudaP2PContextHandle ctx,
    CUexternalSemaphoreHandleType type,
    void* handle,
    CudaP2PSemaphoreHandle* out)
{
  if(!ctx || !handle || !out)
    return CUDA_P2P_ERROR_INVALID_PARAMETER;

  *out = nullptr;
  std::lock_guard<std::mutex> lock(ctx->mutex);

  CUDA_EXTERNAL_SEMAPHORE_HANDLE_DESC desc{};
  desc.type = type;
#if defined(_WIN32)
  desc.handle.win32.handle = handle;
#else
  desc.handle.fd = static_cast<int>(reinterpret_cast<intptr_t>(handle));
#endif
  desc.flags = 0;

  CUexternalSemaphore sem{};
  CU_CHECK(
      ctx->cu.importExtSem(&sem, &desc), ctx, CUDA_P2P_ERROR_INTEROP_FAILED,
      "cuImportExternalSemaphore");

  auto* s = new CudaP2PSemaphore_t();
  s->handle = sem;
  *out = s;
  return CUDA_P2P_SUCCESS;
}

CudaP2PError cuda_p2p_import_d3d12_fence(
    CudaP2PContextHandle ctx,
    void* shared_fence_handle,
    CudaP2PSemaphoreHandle* sem)
{
#if !defined(_WIN32)
  (void)shared_fence_handle;
  (void)sem;
  if(ctx)
    ctx->lastError = "D3D12 fence interop not available on non-Windows platforms";
  return CUDA_P2P_ERROR_FORMAT_NOT_SUPPORTED;
#else
  return importSemaphore(
      ctx, CU_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE, shared_fence_handle,
      sem);
#endif
}

CudaP2PError cuda_p2p_import_vulkan_semaphore(
    CudaP2PContextHandle ctx,
    void* external_semaphore_handle,
    CudaP2PSemaphoreHandle* sem)
{
#if defined(_WIN32)
  const auto t = CU_EXTERNAL_SEMAPHORE_HANDLE_TYPE_TIMELINE_SEMAPHORE_WIN32;
#else
  const auto t = CU_EXTERNAL_SEMAPHORE_HANDLE_TYPE_TIMELINE_SEMAPHORE_FD;
#endif
  return importSemaphore(ctx, t, external_semaphore_handle, sem);
}

CudaP2PError cuda_p2p_wait_semaphore(
    CudaP2PContextHandle ctx,
    CudaP2PSemaphoreHandle sem,
    uint64_t value)
{
  if(!ctx || !sem)
    return CUDA_P2P_ERROR_INVALID_PARAMETER;
  std::lock_guard<std::mutex> lock(ctx->mutex);

  CUDA_EXTERNAL_SEMAPHORE_WAIT_PARAMS params{};
  params.params.fence.value = value;
  CU_CHECK(
      ctx->cu.waitExtSems(&sem->handle, &params, 1, ctx->stream), ctx,
      CUDA_P2P_ERROR_SYNC_FAILED, "cuWaitExternalSemaphoresAsync");
  return CUDA_P2P_SUCCESS;
}

void cuda_p2p_release_semaphore(
    CudaP2PContextHandle ctx,
    CudaP2PSemaphoreHandle sem)
{
  if(!ctx || !sem)
    return;
  std::lock_guard<std::mutex> lock(ctx->mutex);
  if(sem->handle)
    ctx->cu.destroyExtSem(sem->handle);
  delete sem;
}

// =============================================================================
// Synchronization
// =============================================================================

CudaP2PError cuda_p2p_sync(CudaP2PContextHandle ctx)
{
  if(!ctx)
    return CUDA_P2P_ERROR_INVALID_PARAMETER;
  std::lock_guard<std::mutex> lock(ctx->mutex);
  CU_CHECK(
      ctx->cu.streamSync(ctx->stream), ctx, CUDA_P2P_ERROR_SYNC_FAILED,
      "cuStreamSynchronize");
  return CUDA_P2P_SUCCESS;
}
