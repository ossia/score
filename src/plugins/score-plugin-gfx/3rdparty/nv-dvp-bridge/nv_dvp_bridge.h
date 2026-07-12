/**
 * @file nv_dvp_bridge.h
 * @brief C API for AJA <-> NVIDIA "GPUDirect for Video" (DVP) on Windows.
 *
 * Despite the name, NVIDIA's GPUDirect for Video does not implement true
 * GPU<->card peer-to-peer DMA on Windows; it is a high-throughput,
 * hardware-synchronised DMA path between a registered backend GPU
 * resource (D3D11 texture / OpenGL texture) and a registered, page-locked
 * system-memory buffer. AJA's `dvplowlatencydemo` uses this path.
 *
 * Per output frame on score's AJA tier-3 path:
 *
 *   1. QRhi encoder (V210 / UYVY / BGRA fragment encoder) writes the AJA
 *      pixel format into a QRhi RGBA8 texture whose dimensions match the
 *      encoded frame layout (e.g. 1280x1080 for 1920x1080 v210).
 *   2. The bridge performs a `dvpMemcpyLined` from the texture into a
 *      page-locked, AJA-DMA-locked system-memory buffer. The DVP DMA
 *      engine handles ordering between QRhi's queue and the copy via
 *      DVP sync objects.
 *   3. AJA's `AutoCirculateTransfer` ships the system buffer over PCIe
 *      to the SDI card.
 *
 * The bridge expects the NVIDIA GPUDirect for Video SDK headers
 * (`DVPAPI.h`, `dvpapi_d3d11.h`, `dvpapi_gl.h`) to be available at build
 * time. Without them, score-plugin-gfx skips this bridge and AJA falls
 * back to encoder + CPU staging via QRhi readback.
 *
 * Thread model: every thread that calls a transfer function must bracket
 * its work with `nv_dvp_thread_begin` / `nv_dvp_thread_end` once. The
 * thread that calls `nv_dvp_init_*` does the API binding (e.g.
 * `dvpInitD3D11Device`); the underlying GL or D3D11 device must be
 * usable from the calling thread at init time.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* Symbol visibility: the bridge's .cpp is compiled directly into the
 * AJA addon (NV_DVP_BRIDGE_INLINE=1) so no DLL boundary is needed.
 * The DLL build mode is preserved in case a future use needs it.
 */
#if defined(NV_DVP_BRIDGE_INLINE)
  #define NV_DVP_API
#elif defined(_WIN32)
  #ifdef NV_DVP_BRIDGE_EXPORTS
    #define NV_DVP_API __declspec(dllexport)
  #else
    #define NV_DVP_API __declspec(dllimport)
  #endif
#else
  #define NV_DVP_API
#endif

/* Opaque handle types */
typedef struct NvDvpContext_t* NvDvpContextHandle;
typedef struct NvDvpResource_t* NvDvpResourceHandle; /* texture or buffer */

typedef enum NvDvpError {
  NV_DVP_SUCCESS = 0,
  NV_DVP_ERROR_NOT_INITIALIZED,
  NV_DVP_ERROR_INIT_FAILED,
  NV_DVP_ERROR_INVALID_PARAMETER,
  NV_DVP_ERROR_INVALID_HANDLE,
  NV_DVP_ERROR_INTEROP_FAILED,
  NV_DVP_ERROR_TRANSFER_FAILED,
  NV_DVP_ERROR_SYNC_FAILED,
  NV_DVP_ERROR_ALLOC_FAILED,
  NV_DVP_ERROR_UNKNOWN
} NvDvpError;

/** Pixel format the DVP DMA understands. Only formats actually emitted
 *  by score's AJA encoders / used by AJA frame buffer formats are
 *  exposed. The bridge maps these to the matching DVP_* enum. */
typedef enum NvDvpFormat {
  NV_DVP_FORMAT_RGBA8 = 0, /**< DVP_RGBA + DVP_UNSIGNED_BYTE */
  NV_DVP_FORMAT_BGRA8       /**< DVP_BGRA + DVP_UNSIGNED_BYTE */
} NvDvpFormat;

/* ============================================================================
 * Context Management
 *
 * Each context binds DVP to a single backend (D3D11 device or GL context).
 * To use both D3D11 and GL in the same process, create two contexts.
 * ============================================================================ */

/** Bind DVP to an existing D3D11 device. The device must remain valid
 *  for the lifetime of the context. The bridge does not AddRef the
 *  device; the caller (QRhi in score's case) owns it. */
NV_DVP_API NvDvpError nv_dvp_init_d3d11(
    NvDvpContextHandle* out_ctx,
    void* d3d11_device); /* ID3D11Device* */

/** Bind DVP to the current OpenGL context. The caller must have made
 *  the context current on the calling thread before calling this; DVP
 *  resolves required entry points from that context. */
NV_DVP_API NvDvpError nv_dvp_init_gl(NvDvpContextHandle* out_ctx);

/** Bind DVP to the current CUDA primary context.
 *
 * Cross-platform (Win + Linux). Caller must have a CUDA context
 * active on the calling thread — `cuCtxSetCurrent(ctx)` from the
 * shared `CudaFunctions` table is the typical setup. DVP's CUDA path
 * is what enables sysmem↔CUDA-resource DMA with CUstream-side sync,
 * useful for vendors without true GPU-direct P2P (e.g. DeckLink) and
 * for cases where CUDA-stream integration is cleaner than a manual
 * sync-object pair. */
NV_DVP_API NvDvpError nv_dvp_init_cuda(NvDvpContextHandle* out_ctx);

/** Tear down the DVP context. Must be called from the same thread that
 *  was last responsible for `nv_dvp_thread_begin` on this context if
 *  any thread is still inside `begin`/`end`. */
NV_DVP_API void nv_dvp_shutdown(NvDvpContextHandle ctx);

/** Whether DVP is available at runtime. Returns false on non-NVIDIA
 *  GPUs and when the DVP runtime DLL isn't present. Cheap to call. */
NV_DVP_API bool nv_dvp_available(void);

NV_DVP_API const char* nv_dvp_get_error_string(NvDvpContextHandle ctx);

/* ============================================================================
 * Per-thread scope
 *
 * `dvpBegin` / `dvpEnd` is per-thread; AJA's demos call once at thread
 * startup, not per frame. Score's AJA strategies call begin from
 * AJANode's render thread (the one that does the offscreen frame +
 * encoder dispatch) once and end on shutdown.
 * ============================================================================ */

NV_DVP_API NvDvpError nv_dvp_thread_begin(NvDvpContextHandle ctx);
NV_DVP_API NvDvpError nv_dvp_thread_end(NvDvpContextHandle ctx);

/* ============================================================================
 * Resource registration
 *
 * Textures and buffers are registered once and reused across many
 * transfers. The returned handle owns DVP-side state including a sync
 * object pair that tracks DMA ordering against API access.
 * ============================================================================ */

/** Register a D3D11 texture (typically a colour-attachment texture
 *  produced by QRhi's encoder render target). The texture must be
 *  D3D11_USAGE_DEFAULT, ALLOW the bind flags QRhi sets for render
 *  targets, and have format + dimensions exactly matching the
 *  arguments. */
NV_DVP_API NvDvpError nv_dvp_register_d3d11_texture(
    NvDvpContextHandle ctx,
    void* d3d11_texture, /* ID3D11Texture2D* */
    NvDvpFormat format,
    uint32_t width,
    uint32_t height,
    NvDvpResourceHandle* out_handle);

/** Register an OpenGL 2D texture by GL id. The OpenGL context bound
 *  at `nv_dvp_init_gl` time must be current when this is called. */
NV_DVP_API NvDvpError nv_dvp_register_gl_texture(
    NvDvpContextHandle ctx,
    uint32_t gl_texture_id,
    NvDvpFormat format,
    uint32_t width,
    uint32_t height,
    NvDvpResourceHandle* out_handle);

/** Register a CUDA-allocated GPU **flat device pointer** with DVP.
 *  Useful for true P2P GPU buffers (CUDA-imported VkBuffer, CUDA-
 *  allocated cudaMalloc region, etc) that need DVP-mediated transfers
 *  with stream-side sync. `cuda_device_ptr` is passed as `uint64_t`
 *  to avoid pulling `<cuda.h>` into consumers. */
NV_DVP_API NvDvpError nv_dvp_register_cuda_device_ptr(
    NvDvpContextHandle ctx,
    uint64_t cuda_device_ptr,
    NvDvpFormat format,
    uint32_t width,
    uint32_t height,
    NvDvpResourceHandle* out_handle);

/** Register a CUDA array (typically the level-0 array of a CUDA-imported
 *  mipmapped array — see `cuda_interop_import_vulkan_image`) with DVP. */
NV_DVP_API NvDvpError nv_dvp_register_cuda_array(
    NvDvpContextHandle ctx,
    void* cuda_array,       /* CUarray opaque pointer */
    NvDvpFormat format,
    uint32_t width,
    uint32_t height,
    NvDvpResourceHandle* out_handle);

/** Register a system-memory buffer. The caller owns the memory; the
 *  bridge stores the pointer and trusts it stays valid until
 *  unregister. Recommended allocation: `nv_dvp_aligned_alloc(size)` which
 *  returns 4K-aligned memory cross-platform (`_aligned_malloc` on
 *  Windows, `posix_memalign` on POSIX). Vendor-side pinning (e.g.
 *  AJA `DMABufferLock(inMap=true)`) is the caller's responsibility —
 *  the bridge does not pin. */
NV_DVP_API NvDvpError nv_dvp_register_sysmem_buffer(
    NvDvpContextHandle ctx,
    void* sysmem_ptr,
    NvDvpFormat format,
    uint32_t width,
    uint32_t height,
    uint32_t stride_bytes,
    NvDvpResourceHandle* out_handle);

NV_DVP_API void nv_dvp_unregister(
    NvDvpContextHandle ctx, NvDvpResourceHandle handle);

/* ============================================================================
 * Cross-platform 4K-aligned allocation helper
 *
 * DVP wants `bufferAddrAlignment` (typically 4K). NVIDIA's documentation
 * recommends page-aligned allocation regardless of the reported value.
 * Windows uses `_aligned_malloc` / `_aligned_free`; POSIX uses
 * `posix_memalign` paired with plain `free`. This helper hides the split.
 *
 * NULL is returned on allocation failure.
 * ============================================================================ */

NV_DVP_API void* nv_dvp_aligned_alloc(uint64_t bytes);
NV_DVP_API void nv_dvp_aligned_free(void* ptr);

/* ============================================================================
 * API-side sync: bracket GL/D3D access to a registered texture
 *
 * Between `nv_dvp_acquire_texture` and `nv_dvp_release_texture` the
 * texture is owned by the API (QRhi) for rendering. Outside that range
 * the bridge owns it for DVP DMA. Calling `copy_texture_to_buffer`
 * implicitly releases the texture for DVP, then re-acquires it after
 * the DMA completes; explicit acquire/release is only needed when QRhi
 * wants to render to the texture between transfers without a transfer
 * call in between (typical for the input pipeline overwriting the
 * texture each frame).
 * ============================================================================ */

NV_DVP_API NvDvpError nv_dvp_acquire_texture(
    NvDvpContextHandle ctx, NvDvpResourceHandle texture);

NV_DVP_API NvDvpError nv_dvp_release_texture(
    NvDvpContextHandle ctx, NvDvpResourceHandle texture);

/* ============================================================================
 * Per-frame transfers
 *
 * These are *synchronous* from the caller's perspective: the function
 * returns when the DMA is complete and the destination is consistent.
 * Synchronous transfer simplifies the AJA strategy at the cost of one
 * frame of pipelining headroom; AJA's own dvplowlatencydemo uses
 * asynchronous transfers + a multi-frame circular buffer for that
 * reason. Score's AJANode already runs the AJA writes on a separate
 * `AJAConsumerThread`, so the synchronous bridge call only stalls the
 * render thread for the duration of the DMA (~ms on a typical PCIe).
 * ============================================================================ */

/** OUTPUT path: copy from a registered texture to a registered sysmem
 *  buffer. Blocks until the destination is consistent. */
NV_DVP_API NvDvpError nv_dvp_copy_texture_to_buffer(
    NvDvpContextHandle ctx,
    NvDvpResourceHandle src_texture,
    NvDvpResourceHandle dst_buffer);

/** INPUT path (capture): copy from a registered sysmem buffer to a
 *  registered texture. Blocks until the destination is consistent. */
NV_DVP_API NvDvpError nv_dvp_copy_buffer_to_texture(
    NvDvpContextHandle ctx,
    NvDvpResourceHandle src_buffer,
    NvDvpResourceHandle dst_texture);

/* ============================================================================
 * Per-frame CUDA transfers
 *
 * Same shape as the texture variants but using CUstream-side sync. The
 * caller provides a `CUstream` (or NULL for the default stream) and
 * DVP's `dvpMapBufferWaitCUDAStream` / `dvpMapBufferEndCUDAStream`
 * insert wait/signal operations into that stream. This is the path
 * vendors without GPU-direct P2P (DeckLink, possibly some Magewell
 * configurations) use to push captured frames into a CUDA-mapped
 * texture without a CPU memcpy.
 *
 * `cuda_stream` is passed as `void*` (CUstream is opaque) — pass NULL
 * for the default stream.
 * ============================================================================ */

NV_DVP_API NvDvpError nv_dvp_copy_buffer_to_cuda(
    NvDvpContextHandle ctx,
    NvDvpResourceHandle src_buffer,
    NvDvpResourceHandle dst_cuda,
    void* cuda_stream);

NV_DVP_API NvDvpError nv_dvp_copy_cuda_to_buffer(
    NvDvpContextHandle ctx,
    NvDvpResourceHandle src_cuda,
    NvDvpResourceHandle dst_buffer,
    void* cuda_stream);

#ifdef __cplusplus
}
#endif
