/**
 * @file nv_dvp_bridge.cpp
 * @brief Implementation of nv_dvp_bridge.h. Wraps NVIDIA's "GPUDirect for
 *        Video" (DVP) SDK so score's AJA tier-3 path can DMA encoder output
 *        from GPU memory to AJA-DMA-locked sysmem on Windows.
 *
 * Reference implementation: AJA's `ntv2glTextureTransferNV.cpp` in
 * `3rdparty/libajantv2/demos/NVIDIA/common/`. This bridge collapses the
 * demo's chunked-async transfer into one synchronous call per frame; the
 * AJA strategies in score-plugin-gfx provide pipelining via their slot
 * ring + the AJAConsumerThread instead of via DVP-side chunking.
 */

#include "nv_dvp_bridge.h"
#include "dvpapi_shim.h"

#if defined(_WIN32)
#include <d3d11.h>
#include <malloc.h> // _aligned_malloc / _aligned_free
#endif

#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace
{

/* DVP semaphores must be 32-bit ints aligned to alignment-bytes that
 * `dvpGetRequiredConstants*` reports. `_semaphoreAllocSize` is the
 * minimum byte count to allocate; we add `_semaphoreAddrAlignment` as
 * slack and round up the resulting pointer. Same shape as AJA's demo. */
struct SyncSlot
{
  volatile uint32_t* sem{};
  volatile uint32_t* semOrg{};
  uint32_t releaseValue{0};
  uint32_t acquireValue{0};
  DVPSyncObjectHandle obj{};
};

struct ResourceState
{
  enum class Kind : uint8_t { Texture, Buffer };
  Kind kind{};
  DVPBufferHandle dvp{};

  /* Geometry: needed for `dvpMemcpyLined` which takes an explicit line
   * count. Both source and destination of a memcpy must agree on the
   * line count, so the strategy registers the texture and buffer with
   * matching (width, height) and the bridge passes height to the DMA. */
  uint32_t width{};
  uint32_t height{};

  /* Buffer-only: one sync slot tracks DVP DMA completion (waited on by
   * AJA before reading). For Texture, sync is implicit via API-side
   * map/unmap calls; we do not allocate a slot. */
  SyncSlot bufSync;

  /* Strong references (texture only) so we can dvpUnbind on shutdown. */
#if defined(_WIN32)
  ID3D11Texture2D* d3d11Texture{};
#endif
};

} // namespace

struct NvDvpContext_t
{
  enum class Backend : uint8_t { D3D11, GL, CUDA };
  Backend backend{};

  /* D3D11-only */
#if defined(_WIN32)
  ID3D11Device* d3d11Device{};
#endif

  uint32_t bufferAddrAlignment{4096};
  uint32_t bufferGPUStrideAlignment{4096};
  uint32_t semaphoreAddrAlignment{16};
  uint32_t semaphoreAllocSize{16};

  std::mutex mtx;
  std::string lastError;

  /* Resource registry. Pointer-stable; handed back to the caller as
   * an opaque NvDvpResourceHandle. */
  std::map<ResourceState*, std::unique_ptr<ResourceState>> resources;

  bool initialized{false};
};

namespace
{

/* ----------------------------------------------------------------------
 * Helpers
 * ---------------------------------------------------------------------- */

inline ResourceState* asResource(NvDvpResourceHandle h)
{
  return reinterpret_cast<ResourceState*>(h);
}

inline NvDvpResourceHandle asHandle(ResourceState* r)
{
  return reinterpret_cast<NvDvpResourceHandle>(r);
}

void initSyncSlot(NvDvpContext_t* ctx, SyncSlot& s)
{
  /* Aligned-up allocation for DVP semaphore memory. AJA's demo uses
   * malloc + manual alignment, same here. */
  s.semOrg = static_cast<volatile uint32_t*>(
      std::calloc(1, ctx->semaphoreAllocSize + ctx->semaphoreAddrAlignment));
  uintptr_t v = reinterpret_cast<uintptr_t>(s.semOrg);
  v += ctx->semaphoreAddrAlignment - 1;
  v &= ~(uintptr_t(ctx->semaphoreAddrAlignment) - 1);
  s.sem = reinterpret_cast<volatile uint32_t*>(v);
  *s.sem = 0;

  DVPSyncObjectDesc desc{};
  desc.externalClientWaitFunc = nullptr;
  desc.flags = 0;
  desc.sem = const_cast<uint32_t*>(s.sem);
  dvpImportSyncObject(&desc, &s.obj);
}

void freeSyncSlot(SyncSlot& s)
{
  if(s.obj)
    dvpFreeSyncObject(s.obj);
  s.obj = 0;
  if(s.semOrg)
    std::free(const_cast<uint32_t*>(s.semOrg));
  s.semOrg = nullptr;
  s.sem = nullptr;
}

uint32_t bytesPerPixel(NvDvpFormat fmt)
{
  switch(fmt)
  {
    case NV_DVP_FORMAT_RGBA8:
    case NV_DVP_FORMAT_BGRA8:
      return 4u;
  }
  return 4u;
}

DVPBufferFormats toDvpFormat(NvDvpFormat f)
{
  switch(f)
  {
    case NV_DVP_FORMAT_RGBA8:
      return DVP_RGBA;
    case NV_DVP_FORMAT_BGRA8:
      return DVP_BGRA;
  }
  return DVP_RGBA;
}

/* Sticky last-init error. On init failure we destroy the context (the caller's
 * out_ctx stays null), so the per-context lastError is lost and a subsequent
 * nv_dvp_get_error_string(nullptr) would fall back to the generic "Invalid
 * context" string. Stash the real failure (incl. DVP status) here so callers
 * get the actual cause (e.g. "dvpInitGLContext failed (DVP status=-1)" — the
 * runtime rejecting a non-Quadro GPU). */
std::mutex& initErrorMutex()
{
  static std::mutex m;
  return m;
}
std::string& lastInitErrorStorage()
{
  static std::string e;
  return e;
}
void setInitError(const char* msg, DVPStatus status)
{
  std::lock_guard<std::mutex> lk{initErrorMutex()};
  char buf[160];
  std::snprintf(buf, sizeof(buf), "%s (DVP status=%d)", msg, int(status));
  lastInitErrorStorage().assign(buf);
}

/* Set ctx->lastError. Called under ctx->mtx. */
void setError(NvDvpContext_t* ctx, const char* msg, DVPStatus status = DVP_STATUS_OK)
{
  if(!ctx)
    return;
  if(status != DVP_STATUS_OK)
  {
    char buf[160];
    std::snprintf(buf, sizeof(buf), "%s (DVP status=%d)", msg, int(status));
    ctx->lastError.assign(buf);
  }
  else
  {
    ctx->lastError.assign(msg);
  }
}

#define DVP_CHECK(call, ctx, retval)                                  \
  do                                                                  \
  {                                                                   \
    DVPStatus _st = (call);                                           \
    if(_st != DVP_STATUS_OK)                                          \
    {                                                                 \
      setError((ctx), #call, _st);                                    \
      return (retval);                                                \
    }                                                                 \
  } while(0)

} // namespace

/* ============================================================================
 * Public API
 * ============================================================================ */

extern "C" {

NV_DVP_API bool nv_dvp_available(void)
{
  /* Try to load dvp.dll. If it's not present (no NVIDIA "GPUDirect for
   * Video" SDK installed and dvp.dll not in PATH), this returns false
   * and the AJA strategies refuse to use the DVP path. */
  return nv_dvp_load_runtime();
}

NV_DVP_API const char* nv_dvp_get_error_string(NvDvpContextHandle ctx)
{
  if(!ctx)
  {
    /* Loader-time error (dvp.dll missing, version mismatch, mangled-name
     * mismatch on a required entry point) is sticky — surface it even if
     * the caller never got a context. */
    const char* loadErr = nv_dvp_get_runtime_error();
    if(loadErr && loadErr[0])
      return loadErr;
    /* Init-time error (e.g. dvpInit*Context rejecting a non-Quadro GPU) is
     * also sticky: the context is destroyed on failure so it can't carry the
     * message itself. */
    {
      std::lock_guard<std::mutex> lk{initErrorMutex()};
      if(!lastInitErrorStorage().empty())
        return lastInitErrorStorage().c_str();
    }
    return "Invalid context";
  }
  return ctx->lastError.c_str();
}

NV_DVP_API NvDvpError nv_dvp_init_d3d11(
    NvDvpContextHandle* out_ctx, void* d3d11_device)
{
#if !defined(_WIN32)
  (void)d3d11_device;
  if(out_ctx)
    *out_ctx = nullptr;
  return NV_DVP_ERROR_UNKNOWN; // D3D11 unavailable on non-Windows
#else
  if(!out_ctx || !d3d11_device)
    return NV_DVP_ERROR_INVALID_PARAMETER;

  *out_ctx = nullptr;

  if(!nv_dvp_load_runtime() || !nv_dvp_have_d3d11())
    return NV_DVP_ERROR_INIT_FAILED;

  auto ctx = std::make_unique<NvDvpContext_t>();
  ctx->backend = NvDvpContext_t::Backend::D3D11;
  ctx->d3d11Device = static_cast<ID3D11Device*>(d3d11_device);

  if(auto _st = dvpInitD3D11Device(ctx->d3d11Device, 0); _st != DVP_STATUS_OK)
  {
    fprintf(
        stderr, "[nv-dvp] dvpInitD3D11Device failed: DVP_STATUS=%d\n", (int)_st);
    setError(ctx.get(), "dvpInitD3D11Device failed", _st);
    setInitError("dvpInitD3D11Device failed", _st);
    return NV_DVP_ERROR_INIT_FAILED;
  }

  uint32_t unused = 0;
  if(dvpGetRequiredConstantsD3D11Device(
         &ctx->bufferAddrAlignment, &ctx->bufferGPUStrideAlignment,
         &ctx->semaphoreAddrAlignment, &ctx->semaphoreAllocSize,
         &unused, &unused, ctx->d3d11Device)
     != DVP_STATUS_OK)
  {
    dvpCloseD3D11Device(ctx->d3d11Device);
    setError(ctx.get(), "dvpGetRequiredConstantsD3D11Device failed");
    return NV_DVP_ERROR_INIT_FAILED;
  }

  ctx->initialized = true;
  *out_ctx = ctx.release();
  return NV_DVP_SUCCESS;
#endif
}

NV_DVP_API NvDvpError nv_dvp_init_gl(NvDvpContextHandle* out_ctx)
{
  if(!out_ctx)
    return NV_DVP_ERROR_INVALID_PARAMETER;

  *out_ctx = nullptr;

  if(!nv_dvp_load_runtime() || !nv_dvp_have_gl())
    return NV_DVP_ERROR_INIT_FAILED;

  auto ctx = std::make_unique<NvDvpContext_t>();
  ctx->backend = NvDvpContext_t::Backend::GL;

  /* DVP_DEVICE_FLAGS_SHARE_APP_CONTEXT (1): make libdvp share the app's GL
   * context instead of creating its own internal one. Without it, only the
   * FIRST dvpInitGLContext'd context in the process can transfer — a second
   * one (e.g. an output node and a capture node each with their own QRhi)
   * gets DVP_STATUS_ERROR from every dvpMemcpyLined while all setup calls
   * report success. Verified with a standalone two-context probe. */
  if(auto _st = dvpInitGLContext(1); _st != DVP_STATUS_OK)
  {
    fprintf(stderr, "[nv-dvp] dvpInitGLContext failed: DVP_STATUS=%d\n", (int)_st);
    setError(ctx.get(), "dvpInitGLContext failed", _st);
    setInitError("dvpInitGLContext failed", _st);
    return NV_DVP_ERROR_INIT_FAILED;
  }

  uint32_t unused = 0;
  if(dvpGetRequiredConstantsGLCtx(
         &ctx->bufferAddrAlignment, &ctx->bufferGPUStrideAlignment,
         &ctx->semaphoreAddrAlignment, &ctx->semaphoreAllocSize,
         &unused, &unused)
     != DVP_STATUS_OK)
  {
    dvpCloseGLContext();
    setError(ctx.get(), "dvpGetRequiredConstantsGLCtx failed");
    return NV_DVP_ERROR_INIT_FAILED;
  }

  ctx->initialized = true;
  *out_ctx = ctx.release();
  return NV_DVP_SUCCESS;
}

NV_DVP_API NvDvpError nv_dvp_init_cuda(NvDvpContextHandle* out_ctx)
{
  if(!out_ctx)
    return NV_DVP_ERROR_INVALID_PARAMETER;

  *out_ctx = nullptr;

  if(!nv_dvp_load_runtime() || !nv_dvp_have_cuda())
    return NV_DVP_ERROR_INIT_FAILED;

  auto ctx = std::make_unique<NvDvpContext_t>();
  ctx->backend = NvDvpContext_t::Backend::CUDA;

  if(auto _st = dvpInitCUDAContext(0); _st != DVP_STATUS_OK)
  {
    fprintf(
        stderr, "[nv-dvp] dvpInitCUDAContext failed: DVP_STATUS=%d\n", (int)_st);
    setError(ctx.get(), "dvpInitCUDAContext failed", _st);
    setInitError("dvpInitCUDAContext failed", _st);
    return NV_DVP_ERROR_INIT_FAILED;
  }

  uint32_t unused = 0;
  if(dvpGetRequiredConstantsCUDACtx(
         &ctx->bufferAddrAlignment, &ctx->bufferGPUStrideAlignment,
         &ctx->semaphoreAddrAlignment, &ctx->semaphoreAllocSize,
         &unused, &unused)
     != DVP_STATUS_OK)
  {
    dvpCloseCUDAContext();
    setError(ctx.get(), "dvpGetRequiredConstantsCUDACtx failed");
    return NV_DVP_ERROR_INIT_FAILED;
  }

  ctx->initialized = true;
  *out_ctx = ctx.release();
  return NV_DVP_SUCCESS;
}

NV_DVP_API void nv_dvp_shutdown(NvDvpContextHandle ctx)
{
  if(!ctx)
    return;
  std::lock_guard<std::mutex> lock(ctx->mtx);

  /* Drop all registered resources. Textures returned from
   * dvpCreateGPU*Resource were never explicitly bound (see comment in
   * register_*_texture), so only sysmem buffers need the unbind. */
  for(auto& kv : ctx->resources)
  {
    auto* r = kv.second.get();
    if(r->kind == ResourceState::Kind::Buffer)
    {
      switch(ctx->backend)
      {
        case NvDvpContext_t::Backend::D3D11:
#if defined(_WIN32)
          dvpUnbindFromD3D11Device(r->dvp, ctx->d3d11Device);
#endif
          break;
        case NvDvpContext_t::Backend::GL:
          dvpUnbindFromGLCtx(r->dvp);
          break;
        case NvDvpContext_t::Backend::CUDA:
          dvpUnbindFromCUDACtx(r->dvp);
          break;
      }
    }
    if(r->dvp)
      dvpFreeBuffer(r->dvp);
    freeSyncSlot(r->bufSync);
  }
  ctx->resources.clear();

  if(ctx->initialized)
  {
    switch(ctx->backend)
    {
      case NvDvpContext_t::Backend::D3D11:
#if defined(_WIN32)
        dvpCloseD3D11Device(ctx->d3d11Device);
#endif
        break;
      case NvDvpContext_t::Backend::GL:
        dvpCloseGLContext();
        break;
      case NvDvpContext_t::Backend::CUDA:
        dvpCloseCUDAContext();
        break;
    }
  }
  delete ctx;
}

/* dvpBegin / dvpEnd is not held across the lifetime of the strategy:
 * NVIDIA's docs require dvpMapBufferEndAPI / dvpMapBufferWaitAPI to be
 * called *outside* a dvpBegin/dvpEnd pair, while WaitDVP/EndDVP/
 * Memcpy/ClientWait must be called *inside*. AJA's GL demo solves this
 * by running the DVP DMA work on a separate thread that holds the
 * begin/end pair; in our single-threaded model the bridge instead
 * brackets each transfer call with its own dvpBegin/dvpEnd internally.
 *
 * These thread_begin / thread_end functions are kept for ABI
 * compatibility but are no-ops. */
NV_DVP_API NvDvpError nv_dvp_thread_begin(NvDvpContextHandle ctx)
{
  if(!ctx)
    return NV_DVP_ERROR_INVALID_PARAMETER;
  return NV_DVP_SUCCESS;
}

NV_DVP_API NvDvpError nv_dvp_thread_end(NvDvpContextHandle ctx)
{
  if(!ctx)
    return NV_DVP_ERROR_INVALID_PARAMETER;
  return NV_DVP_SUCCESS;
}

NV_DVP_API NvDvpError nv_dvp_register_d3d11_texture(
    NvDvpContextHandle ctx, void* d3d11_texture, NvDvpFormat /*format*/,
    uint32_t width, uint32_t height, NvDvpResourceHandle* out_handle)
{
#if !defined(_WIN32)
  (void)ctx; (void)d3d11_texture; (void)width; (void)height;
  if(out_handle)
    *out_handle = nullptr;
  return NV_DVP_ERROR_UNKNOWN;
#else
  if(!ctx || !d3d11_texture || !out_handle || width == 0 || height == 0
     || ctx->backend != NvDvpContext_t::Backend::D3D11)
    return NV_DVP_ERROR_INVALID_PARAMETER;

  *out_handle = nullptr;
  std::lock_guard<std::mutex> lock(ctx->mtx);

  auto r = std::make_unique<ResourceState>();
  r->kind = ResourceState::Kind::Texture;
  r->width = width;
  r->height = height;
  r->d3d11Texture = static_cast<ID3D11Texture2D*>(d3d11_texture);

  /* dvpCreateGPUD3D11Resource takes any ID3D11Resource*. ID3D11Texture2D
   * derives from ID3D11Resource so the implicit upcast is fine. The
   * resulting buffer handle is implicitly bound to the device that
   * dvpInitD3D11Device was called against - we do NOT (and must not)
   * also call dvpBindToD3D11Device on it. AJA's reference demo
   * (ntv2glTextureTransferNV.cpp:RegisterTexture) only binds the
   * sysmem buffers, never the GPU resources. */
  if(dvpCreateGPUD3D11Resource(r->d3d11Texture, &r->dvp)
     != DVP_STATUS_OK)
  {
    setError(ctx, "dvpCreateGPUD3D11Resource failed");
    return NV_DVP_ERROR_INTEROP_FAILED;
  }

  auto* raw = r.get();
  ctx->resources.emplace(raw, std::move(r));
  *out_handle = asHandle(raw);
  return NV_DVP_SUCCESS;
#endif
}

NV_DVP_API NvDvpError nv_dvp_register_gl_texture(
    NvDvpContextHandle ctx, uint32_t gl_texture_id, NvDvpFormat /*format*/,
    uint32_t width, uint32_t height, NvDvpResourceHandle* out_handle)
{
  if(!ctx || !out_handle || gl_texture_id == 0 || width == 0 || height == 0
     || ctx->backend != NvDvpContext_t::Backend::GL)
    return NV_DVP_ERROR_INVALID_PARAMETER;

  *out_handle = nullptr;
  std::lock_guard<std::mutex> lock(ctx->mtx);

  auto r = std::make_unique<ResourceState>();
  r->kind = ResourceState::Kind::Texture;
  r->width = width;
  r->height = height;

  if(dvpCreateGPUTextureGL(gl_texture_id, &r->dvp) != DVP_STATUS_OK)
  {
    setError(ctx, "dvpCreateGPUTextureGL failed");
    return NV_DVP_ERROR_INTEROP_FAILED;
  }
  /* GL textures are implicitly bound to the GL context dvpInitGLContext
   * was called against; no separate dvpBindToGLCtx for GPU textures. */

  /* Texture-destination copies (buffer->texture) need a sync object to
   * signal on completion — dvpMemcpyLined rejects a null dstSync.
   * (Blender/UPBGE's VideoDeckLink allocates a "gpu sync" per texture
   * for exactly this.) */
  initSyncSlot(ctx, r->bufSync);

  auto* raw = r.get();
  ctx->resources.emplace(raw, std::move(r));
  *out_handle = asHandle(raw);
  return NV_DVP_SUCCESS;
}

NV_DVP_API NvDvpError nv_dvp_register_cuda_device_ptr(
    NvDvpContextHandle ctx, uint64_t cuda_device_ptr, NvDvpFormat /*format*/,
    uint32_t width, uint32_t height, NvDvpResourceHandle* out_handle)
{
  if(!ctx || !out_handle || cuda_device_ptr == 0
     || width == 0 || height == 0
     || ctx->backend != NvDvpContext_t::Backend::CUDA)
    return NV_DVP_ERROR_INVALID_PARAMETER;

  *out_handle = nullptr;
  std::lock_guard<std::mutex> lock(ctx->mtx);

  auto r = std::make_unique<ResourceState>();
  r->kind = ResourceState::Kind::Texture;
  r->width = width;
  r->height = height;

  /* CUDA flat device pointers are implicitly bound to the CUDA context
   * dvpInitCUDAContext was called against - no dvpBindToCUDACtx for
   * GPU resources, only sysmem. Same convention as GL/D3D11 GPU
   * resources in DVP. */
  if(dvpCreateGPUCUDADevicePtr(
         static_cast<CUdeviceptr>(cuda_device_ptr), &r->dvp)
     != DVP_STATUS_OK)
  {
    setError(ctx, "dvpCreateGPUCUDADevicePtr failed");
    return NV_DVP_ERROR_INTEROP_FAILED;
  }

  auto* raw = r.get();
  ctx->resources.emplace(raw, std::move(r));
  *out_handle = asHandle(raw);
  return NV_DVP_SUCCESS;
}

NV_DVP_API NvDvpError nv_dvp_register_cuda_array(
    NvDvpContextHandle ctx, void* cuda_array, NvDvpFormat /*format*/,
    uint32_t width, uint32_t height, NvDvpResourceHandle* out_handle)
{
  if(!ctx || !out_handle || !cuda_array || width == 0 || height == 0
     || ctx->backend != NvDvpContext_t::Backend::CUDA)
    return NV_DVP_ERROR_INVALID_PARAMETER;

  *out_handle = nullptr;
  std::lock_guard<std::mutex> lock(ctx->mtx);

  auto r = std::make_unique<ResourceState>();
  r->kind = ResourceState::Kind::Texture;
  r->width = width;
  r->height = height;

  if(dvpCreateGPUCUDAArray(static_cast<CUarray>(cuda_array), &r->dvp)
     != DVP_STATUS_OK)
  {
    setError(ctx, "dvpCreateGPUCUDAArray failed");
    return NV_DVP_ERROR_INTEROP_FAILED;
  }

  auto* raw = r.get();
  ctx->resources.emplace(raw, std::move(r));
  *out_handle = asHandle(raw);
  return NV_DVP_SUCCESS;
}

NV_DVP_API NvDvpError nv_dvp_register_sysmem_buffer(
    NvDvpContextHandle ctx, void* sysmem_ptr, NvDvpFormat format,
    uint32_t width, uint32_t height, uint32_t stride_bytes,
    NvDvpResourceHandle* out_handle)
{
  if(!ctx || !sysmem_ptr || !out_handle || width == 0 || height == 0)
    return NV_DVP_ERROR_INVALID_PARAMETER;

  /* Caller is responsible for sysmem_ptr being aligned to
   * ctx->bufferAddrAlignment and stride to bufferGPUStrideAlignment.
   * We don't enforce here because score's strategies use _aligned_malloc
   * with alignment >= 4096 and align stride to the same, which is
   * always >= what dvpGetRequiredConstants reports on current drivers. */

  std::lock_guard<std::mutex> lock(ctx->mtx);

  auto r = std::make_unique<ResourceState>();
  r->kind = ResourceState::Kind::Buffer;
  r->width = width;
  r->height = height;

  DVPSysmemBufferDesc desc{};
  desc.width = width;
  desc.height = height;
  desc.stride = stride_bytes;
  desc.size = uint32_t(stride_bytes) * height;
  desc.format = toDvpFormat(format);
  desc.type = DVP_UNSIGNED_BYTE;
  desc.bufAddr = sysmem_ptr;

  (void)bytesPerPixel; /* size already supplied by caller via stride */

  if(dvpCreateBuffer(&desc, &r->dvp) != DVP_STATUS_OK)
  {
    setError(ctx, "dvpCreateBuffer (sysmem) failed");
    return NV_DVP_ERROR_ALLOC_FAILED;
  }
  DVPStatus bindStatus = DVP_STATUS_OK;
  const char* bindErr = nullptr;
  switch(ctx->backend)
  {
    case NvDvpContext_t::Backend::D3D11:
#if defined(_WIN32)
      bindStatus = dvpBindToD3D11Device(r->dvp, ctx->d3d11Device);
      bindErr = "dvpBindToD3D11Device (sysmem) failed";
#endif
      break;
    case NvDvpContext_t::Backend::GL:
      bindStatus = dvpBindToGLCtx(r->dvp);
      bindErr = "dvpBindToGLCtx (sysmem) failed";
      break;
    case NvDvpContext_t::Backend::CUDA:
      bindStatus = dvpBindToCUDACtx(r->dvp);
      bindErr = "dvpBindToCUDACtx (sysmem) failed";
      break;
  }
  if(bindStatus != DVP_STATUS_OK)
  {
    dvpFreeBuffer(r->dvp);
    setError(ctx, bindErr ? bindErr : "DVP sysmem bind failed", bindStatus);
    return NV_DVP_ERROR_INTEROP_FAILED;
  }

  initSyncSlot(ctx, r->bufSync);

  auto* raw = r.get();
  ctx->resources.emplace(raw, std::move(r));
  *out_handle = asHandle(raw);
  return NV_DVP_SUCCESS;
}

NV_DVP_API void nv_dvp_unregister(
    NvDvpContextHandle ctx, NvDvpResourceHandle handle)
{
  if(!ctx || !handle)
    return;
  std::lock_guard<std::mutex> lock(ctx->mtx);

  auto* r = asResource(handle);
  auto it = ctx->resources.find(r);
  if(it == ctx->resources.end())
    return;

  /* Textures (from dvpCreateGPU*Resource) are never explicitly bound,
   * so they don't need unbinding. Only sysmem buffers do. */
  if(r->kind == ResourceState::Kind::Buffer)
  {
    switch(ctx->backend)
    {
      case NvDvpContext_t::Backend::D3D11:
#if defined(_WIN32)
        dvpUnbindFromD3D11Device(r->dvp, ctx->d3d11Device);
#endif
        break;
      case NvDvpContext_t::Backend::GL:
        dvpUnbindFromGLCtx(r->dvp);
        break;
      case NvDvpContext_t::Backend::CUDA:
        dvpUnbindFromCUDACtx(r->dvp);
        break;
    }
  }

  if(r->dvp)
    dvpFreeBuffer(r->dvp);
  freeSyncSlot(r->bufSync);
  ctx->resources.erase(it);
}

NV_DVP_API NvDvpError nv_dvp_acquire_texture(
    NvDvpContextHandle ctx, NvDvpResourceHandle texture)
{
  if(!ctx || !texture)
    return NV_DVP_ERROR_INVALID_PARAMETER;
  auto* r = asResource(texture);
  if(r->kind != ResourceState::Kind::Texture)
    return NV_DVP_ERROR_INVALID_HANDLE;

  std::lock_guard<std::mutex> lock(ctx->mtx);
  /* dvpMapBufferWaitAPI inserts a wait on the API command queue (D3D11
   * deferred or GL command stream) until any pending DVP DMA completes.
   * It does NOT block the CPU. */
  DVP_CHECK(dvpMapBufferWaitAPI(r->dvp), ctx, NV_DVP_ERROR_SYNC_FAILED);
  return NV_DVP_SUCCESS;
}

NV_DVP_API NvDvpError nv_dvp_release_texture(
    NvDvpContextHandle ctx, NvDvpResourceHandle texture)
{
  if(!ctx || !texture)
    return NV_DVP_ERROR_INVALID_PARAMETER;
  auto* r = asResource(texture);
  if(r->kind != ResourceState::Kind::Texture)
    return NV_DVP_ERROR_INVALID_HANDLE;

  std::lock_guard<std::mutex> lock(ctx->mtx);
  /* dvpMapBufferEndAPI signals the API queue is done writing the
   * texture; the next DVP DMA can proceed. */
  DVP_CHECK(dvpMapBufferEndAPI(r->dvp), ctx, NV_DVP_ERROR_SYNC_FAILED);
  return NV_DVP_SUCCESS;
}

NV_DVP_API NvDvpError nv_dvp_copy_texture_to_buffer(
    NvDvpContextHandle ctx, NvDvpResourceHandle src_texture,
    NvDvpResourceHandle dst_buffer)
{
  if(!ctx || !src_texture || !dst_buffer)
    return NV_DVP_ERROR_INVALID_PARAMETER;

  auto* tex = asResource(src_texture);
  auto* buf = asResource(dst_buffer);
  if(tex->kind != ResourceState::Kind::Texture
     || buf->kind != ResourceState::Kind::Buffer)
    return NV_DVP_ERROR_INVALID_HANDLE;

  std::lock_guard<std::mutex> lock(ctx->mtx);

  /* WaitDVP / Memcpy / EndDVP / ClientWaitPartial all must run inside a
   * dvpBegin/dvpEnd pair (NVIDIA dvpapi.h docs). EndAPI / WaitAPI -
   * called by nv_dvp_release_texture / nv_dvp_acquire_texture - must
   * run *outside* such a pair, so we bracket only the DVP-side ops
   * here, not the API-side ones. */
  DVP_CHECK(dvpBegin(), ctx, NV_DVP_ERROR_SYNC_FAILED);

  /* Lock the texture for DVP. The caller is expected to have called
   * nv_dvp_release_texture (dvpMapBufferEndAPI) before this so the
   * API queue has signalled it's done writing; if not, WaitDVP will
   * stall waiting for that signal. */
  DVPStatus status = dvpMapBufferWaitDVP(tex->dvp);
  if(status == DVP_STATUS_OK)
  {
    /* DMA into the sysmem buffer. The AJA strategy guarantees (via its
     * producer-consumer pattern + AJAConsumerThread) that we only ever
     * reuse a buffer AJA has finished reading; the ClientWaitPartial
     * below blocks us until DMA is fully complete. */
    buf->bufSync.releaseValue++;
    status = dvpMemcpyLined(
        tex->dvp, /*srcSync=*/0, /*srcWaitValue=*/0, DVP_TIMEOUT_IGNORED,
        buf->dvp, buf->bufSync.obj, buf->bufSync.releaseValue,
        /*startingLine=*/0, /*numberOfLines=*/buf->height);
  }
  if(status == DVP_STATUS_OK)
    status = dvpMapBufferEndDVP(tex->dvp);
  if(status == DVP_STATUS_OK)
    status = dvpSyncObjClientWaitPartial(
        buf->bufSync.obj, buf->bufSync.releaseValue, DVP_TIMEOUT_IGNORED);

  /* Always close the dvpBegin scope, even on failure, otherwise
   * subsequent acquire/release calls would see a dangling open scope. */
  DVPStatus endStatus = dvpEnd();
  if(status != DVP_STATUS_OK)
  {
    setError(ctx, "DVP texture->buffer transfer failed", status);
    return NV_DVP_ERROR_TRANSFER_FAILED;
  }
  if(endStatus != DVP_STATUS_OK)
  {
    setError(ctx, "dvpEnd after texture->buffer", endStatus);
    return NV_DVP_ERROR_SYNC_FAILED;
  }

  return NV_DVP_SUCCESS;
}

NV_DVP_API NvDvpError nv_dvp_copy_buffer_to_texture(
    NvDvpContextHandle ctx, NvDvpResourceHandle src_buffer,
    NvDvpResourceHandle dst_texture)
{
  if(!ctx || !src_buffer || !dst_texture)
    return NV_DVP_ERROR_INVALID_PARAMETER;

  auto* buf = asResource(src_buffer);
  auto* tex = asResource(dst_texture);
  if(buf->kind != ResourceState::Kind::Buffer
     || tex->kind != ResourceState::Kind::Texture)
    return NV_DVP_ERROR_INVALID_HANDLE;

  std::lock_guard<std::mutex> lock(ctx->mtx);

  /* Upload sequence per Blender/UPBGE VideoDeckLink (the reference libdvp
   * consumer): EndAPI marks the API stream done with the texture BEFORE the
   * DVP scope; the copy waits on the sysmem buffer's sync (CPU-signalled
   * "data ready") and signals the texture's own sync on completion; WaitAPI
   * after the scope makes the API stream wait for the copy before sampling.
   * Deviating from this exact shape makes dvpMemcpyLined return
   * DVP_STATUS_ERROR. */
  const char* step = "dvpMapBufferEndAPI(tex)";
  DVPStatus status = dvpMapBufferEndAPI(tex->dvp);
  if(status != DVP_STATUS_OK)
  {
    setError(ctx, step, status);
    return NV_DVP_ERROR_TRANSFER_FAILED;
  }

  DVP_CHECK(dvpBegin(), ctx, NV_DVP_ERROR_SYNC_FAILED);

  step = "dvpMapBufferWaitDVP(tex)";
  status = dvpMapBufferWaitDVP(tex->dvp);
  if(status == DVP_STATUS_OK)
  {
    step = "dvpMemcpyLined(buf->tex)";
    buf->bufSync.acquireValue++;
    *buf->bufSync.sem = buf->bufSync.acquireValue;
    tex->bufSync.releaseValue++;
    status = dvpMemcpyLined(
        buf->dvp, buf->bufSync.obj, buf->bufSync.acquireValue,
        DVP_TIMEOUT_IGNORED, tex->dvp, tex->bufSync.obj,
        tex->bufSync.releaseValue,
        /*startingLine=*/0, /*numberOfLines=*/tex->height);
  }
  if(status == DVP_STATUS_OK)
  {
    step = "dvpMapBufferEndDVP(tex)";
    status = dvpMapBufferEndDVP(tex->dvp);
  }

  DVPStatus endStatus = dvpEnd();
  if(status != DVP_STATUS_OK)
  {
    setError(ctx, step, status);
    return NV_DVP_ERROR_TRANSFER_FAILED;
  }
  if(endStatus != DVP_STATUS_OK)
  {
    setError(ctx, "dvpEnd after buffer->texture", endStatus);
    return NV_DVP_ERROR_SYNC_FAILED;
  }

  status = dvpMapBufferWaitAPI(tex->dvp);
  if(status != DVP_STATUS_OK)
  {
    setError(ctx, "dvpMapBufferWaitAPI(tex)", status);
    return NV_DVP_ERROR_SYNC_FAILED;
  }

  return NV_DVP_SUCCESS;
}

/* ============================================================================
 * Per-frame CUDA transfers
 *
 * The CUDA-stream sync pattern differs from the GL/D3D11 case: instead of
 * the API-side dvpMapBufferEndAPI / dvpMapBufferWaitAPI calls (which are
 * implicit at queue boundaries on GL/D3D11), the CUDA path uses
 * dvpMapBufferWaitCUDAStream / dvpMapBufferEndCUDAStream which insert
 * wait/signal operations into a user-supplied CUstream. These stream
 * sync calls are themselves *inside* the dvpBegin/dvpEnd pair (per
 * NVIDIA's dvpapi.h docs), unlike the API variants.
 * ============================================================================ */

NV_DVP_API NvDvpError nv_dvp_copy_buffer_to_cuda(
    NvDvpContextHandle ctx, NvDvpResourceHandle src_buffer,
    NvDvpResourceHandle dst_cuda, void* cuda_stream)
{
  if(!ctx || !src_buffer || !dst_cuda
     || ctx->backend != NvDvpContext_t::Backend::CUDA)
    return NV_DVP_ERROR_INVALID_PARAMETER;

  auto* buf = asResource(src_buffer);
  auto* dst = asResource(dst_cuda);
  if(buf->kind != ResourceState::Kind::Buffer
     || dst->kind != ResourceState::Kind::Texture)
    return NV_DVP_ERROR_INVALID_HANDLE;

  std::lock_guard<std::mutex> lock(ctx->mtx);

  DVP_CHECK(dvpBegin(), ctx, NV_DVP_ERROR_SYNC_FAILED);

  CUstream stream = static_cast<CUstream>(cuda_stream);
  DVPStatus status = dvpMapBufferWaitCUDAStream(dst->dvp, stream);
  if(status == DVP_STATUS_OK)
  {
    buf->bufSync.releaseValue++;
    status = dvpMemcpyLined(
        buf->dvp, /*srcSync=*/0, /*srcWaitValue=*/0, DVP_TIMEOUT_IGNORED,
        dst->dvp, buf->bufSync.obj, buf->bufSync.releaseValue,
        /*startingLine=*/0, /*numberOfLines=*/dst->height);
  }
  if(status == DVP_STATUS_OK)
    status = dvpMapBufferEndCUDAStream(dst->dvp, stream);
  if(status == DVP_STATUS_OK)
    status = dvpSyncObjClientWaitPartial(
        buf->bufSync.obj, buf->bufSync.releaseValue, DVP_TIMEOUT_IGNORED);

  DVPStatus endStatus = dvpEnd();
  if(status != DVP_STATUS_OK)
  {
    setError(ctx, "DVP buffer->CUDA transfer failed", status);
    return NV_DVP_ERROR_TRANSFER_FAILED;
  }
  if(endStatus != DVP_STATUS_OK)
  {
    setError(ctx, "dvpEnd after buffer->CUDA", endStatus);
    return NV_DVP_ERROR_SYNC_FAILED;
  }

  return NV_DVP_SUCCESS;
}

NV_DVP_API NvDvpError nv_dvp_copy_cuda_to_buffer(
    NvDvpContextHandle ctx, NvDvpResourceHandle src_cuda,
    NvDvpResourceHandle dst_buffer, void* cuda_stream)
{
  if(!ctx || !src_cuda || !dst_buffer
     || ctx->backend != NvDvpContext_t::Backend::CUDA)
    return NV_DVP_ERROR_INVALID_PARAMETER;

  auto* src = asResource(src_cuda);
  auto* buf = asResource(dst_buffer);
  if(src->kind != ResourceState::Kind::Texture
     || buf->kind != ResourceState::Kind::Buffer)
    return NV_DVP_ERROR_INVALID_HANDLE;

  std::lock_guard<std::mutex> lock(ctx->mtx);

  DVP_CHECK(dvpBegin(), ctx, NV_DVP_ERROR_SYNC_FAILED);

  CUstream stream = static_cast<CUstream>(cuda_stream);
  DVPStatus status = dvpMapBufferWaitCUDAStream(src->dvp, stream);
  if(status == DVP_STATUS_OK)
  {
    buf->bufSync.releaseValue++;
    status = dvpMemcpyLined(
        src->dvp, /*srcSync=*/0, /*srcWaitValue=*/0, DVP_TIMEOUT_IGNORED,
        buf->dvp, buf->bufSync.obj, buf->bufSync.releaseValue,
        /*startingLine=*/0, /*numberOfLines=*/buf->height);
  }
  if(status == DVP_STATUS_OK)
    status = dvpMapBufferEndCUDAStream(src->dvp, stream);
  if(status == DVP_STATUS_OK)
    status = dvpSyncObjClientWaitPartial(
        buf->bufSync.obj, buf->bufSync.releaseValue, DVP_TIMEOUT_IGNORED);

  DVPStatus endStatus = dvpEnd();
  if(status != DVP_STATUS_OK)
  {
    setError(ctx, "DVP CUDA->buffer transfer failed", status);
    return NV_DVP_ERROR_TRANSFER_FAILED;
  }
  if(endStatus != DVP_STATUS_OK)
  {
    setError(ctx, "dvpEnd after CUDA->buffer", endStatus);
    return NV_DVP_ERROR_SYNC_FAILED;
  }

  return NV_DVP_SUCCESS;
}

/* ============================================================================
 * Cross-platform 4K-aligned allocation
 * ============================================================================ */

NV_DVP_API void* nv_dvp_aligned_alloc(uint64_t bytes)
{
#if defined(_WIN32)
  return ::_aligned_malloc(static_cast<size_t>(bytes), 4096);
#else
  void* p = nullptr;
  /* posix_memalign requires alignment to be a power of 2 and a multiple
   * of sizeof(void*); 4096 satisfies both. */
  if(::posix_memalign(&p, 4096, static_cast<size_t>(bytes)) != 0)
    return nullptr;
  return p;
#endif
}

NV_DVP_API void nv_dvp_aligned_free(void* ptr)
{
  if(!ptr)
    return;
#if defined(_WIN32)
  ::_aligned_free(ptr);
#else
  std::free(ptr);
#endif
}

} // extern "C"
