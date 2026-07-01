/**
 * @file CudaP2PBridge.h
 * @brief C API for vendor-neutral GPU<->device CUDA P2P interop.
 *
 * Score's GPU-direct video paths render compute-shader output (v210, UYVY,
 * BGRA, ...) into a SHARED GPU buffer, then ask the bridge for (a) a flat
 * GPU device pointer the peer device can DMA, and (b) cross-API fences
 * (D3D12 / Vulkan) so the peer never reads a half-written frame.
 *
 * Consumers:
 *   - score-addon-aja (AJA tier-3 RDMA via DMABufferLock(inRDMA=true))
 *   - score-addon-magewell [planned: ProCapture PhysicalAddress path]
 *   - score-addon-rivermax [planned: SMPTE 2110]
 *
 * The bridge is fully runtime-loaded (libcuda.so.1 / nvcuda.dll via
 * dlopen) — no link-time CUDAToolkit dependency. See CudaFunctions.hpp.
 *
 */

#pragma once

#include <score_plugin_gfx_export.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

// Bridge symbols are exported from libscore_plugin_gfx so addon DSOs
// (score-addon-aja and future Magewell/Rivermax addons) can resolve them.
#define CUDA_P2P_API SCORE_PLUGIN_GFX_EXPORT

/* Opaque handle types */
typedef struct CudaP2PContext_t* CudaP2PContextHandle;
typedef struct CudaP2PResource_t* CudaP2PResourceHandle;
typedef struct CudaP2PSemaphore_t* CudaP2PSemaphoreHandle;
typedef struct CudaP2PImage_t* CudaP2PImageHandle;

/* Image format — values match CUarray_format from the CUDA driver API. */
typedef enum CudaP2PImageFormat {
  CUDA_P2P_FORMAT_UNSIGNED_INT8  = 0x01,
  CUDA_P2P_FORMAT_UNSIGNED_INT16 = 0x02,
  CUDA_P2P_FORMAT_UNSIGNED_INT32 = 0x03,
  CUDA_P2P_FORMAT_SIGNED_INT8    = 0x08,
  CUDA_P2P_FORMAT_SIGNED_INT16   = 0x09,
  CUDA_P2P_FORMAT_SIGNED_INT32   = 0x0a,
  CUDA_P2P_FORMAT_HALF           = 0x10,
  CUDA_P2P_FORMAT_FLOAT          = 0x20,
} CudaP2PImageFormat;

typedef struct CudaP2PImageDesc {
  uint32_t width;
  uint32_t height;
  uint32_t depth;        /* 0 or 1 for 2D */
  uint32_t numChannels;
  CudaP2PImageFormat format;
  uint32_t flags;        /* reserved; pass 0 */
} CudaP2PImageDesc;

typedef enum CudaP2PError {
  CUDA_P2P_SUCCESS = 0,
  CUDA_P2P_ERROR_NOT_INITIALIZED,
  CUDA_P2P_ERROR_INIT_FAILED,
  CUDA_P2P_ERROR_NO_DEVICE,
  CUDA_P2P_ERROR_ALLOC_FAILED,
  CUDA_P2P_ERROR_INTEROP_FAILED,
  CUDA_P2P_ERROR_COPY_FAILED,
  CUDA_P2P_ERROR_SYNC_FAILED,
  CUDA_P2P_ERROR_INVALID_HANDLE,
  CUDA_P2P_ERROR_INVALID_PARAMETER,
  CUDA_P2P_ERROR_FORMAT_NOT_SUPPORTED,
  CUDA_P2P_ERROR_UNKNOWN
} CudaP2PError;

/* ============================================================================
 * Context management
 * ============================================================================ */

CUDA_P2P_API CudaP2PError cuda_p2p_init(CudaP2PContextHandle* ctx);
CUDA_P2P_API void cuda_p2p_shutdown(CudaP2PContextHandle ctx);
CUDA_P2P_API bool cuda_p2p_available(void);
CUDA_P2P_API const char* cuda_p2p_get_error_string(CudaP2PContextHandle ctx);

/* ============================================================================
 * Shared GPU buffer importers
 *
 * Each backend allocates a SHARED + UAV-bindable buffer; the matching
 * importer registers it with CUDA, returns a flat GPU device pointer
 * (suitable for AJA's DMABufferLock(inRDMA=true), Magewell's
 * MWCaptureVideoFrameToPhysicalAddress, Rivermax's rmax_register_memory)
 * plus an opaque handle that cuda_p2p_release_buffer cleans up.
 * ============================================================================ */

/** @brief Import a SHARED + D3D11_BIND_UNORDERED_ACCESS ID3D11Buffer. */
CUDA_P2P_API CudaP2PError cuda_p2p_import_d3d11_buffer(
    CudaP2PContextHandle ctx,
    void* d3d11_buffer,                   /* ID3D11Buffer* */
    void* d3d11_device,                   /* ID3D11Device* (unused, kept for API stability) */
    uint32_t buffer_size,
    void** out_device_ptr,
    CudaP2PResourceHandle* out_handle);

/** @brief Import a SHARED placed D3D12 buffer resource (NT HANDLE). */
CUDA_P2P_API CudaP2PError cuda_p2p_import_d3d12_buffer(
    CudaP2PContextHandle ctx,
    void* shared_resource_handle,         /* Win32 HANDLE */
    uint64_t buffer_size,
    void** out_device_ptr,
    CudaP2PResourceHandle* out_handle);

/** @brief Import a Vulkan VkBuffer backed by external memory (Win32 HANDLE / fd). */
CUDA_P2P_API CudaP2PError cuda_p2p_import_vulkan_buffer(
    CudaP2PContextHandle ctx,
    void* external_memory_handle,
    uint64_t buffer_size,
    void** out_device_ptr,
    CudaP2PResourceHandle* out_handle);

/**
 * @brief Register a GL Shader Storage Buffer Object for P2P.
 * GL <-> CUDA buffer interop is direct. The GL context must be current
 * on the calling thread.
 */
CUDA_P2P_API CudaP2PError cuda_p2p_import_gl_buffer(
    CudaP2PContextHandle ctx,
    uint32_t gl_buffer_id,
    uint32_t buffer_size,
    void** out_device_ptr,
    CudaP2PResourceHandle* out_handle);

CUDA_P2P_API void cuda_p2p_release_buffer(
    CudaP2PContextHandle ctx,
    CudaP2PResourceHandle h);

/* ============================================================================
 * Shared GPU image importers
 *
 * For consumers that need a CUDA-mapped image (NV12 Y/UV plane sampling,
 * NVDEC zero-copy decode targets, ...). The image is materialized as a
 * mipmapped array internally; the level-0 CUarray is returned via
 * out_cuda_array (opaque void*; cast to CUarray at use sites).
 *
 * Used by HWCUDA (NVDEC → Vulkan zero-copy). The Vulkan-side memory + handle
 * are produced by score::gfx::vkinterop::createExportableImage +
 * exportMemoryHandle, then handed here.
 * ============================================================================ */

/**
 * @brief Import a Vulkan-exported image as a CUDA mipmapped array (level 0
 *        usable as a memcpy destination via CUDA_MEMCPY2D::dstArray).
 *
 * @param ctx                    bridge context.
 * @param external_memory_handle Win32 HANDLE / Linux fd from
 *                               score::gfx::vkinterop::exportMemoryHandle.
 * @param memory_size            size of the underlying VkDeviceMemory (from
 *                               vkGetImageMemoryRequirements).
 * @param desc                   image format + extents.
 * @param offset_in_memory       byte offset within the imported memory.
 *                               Usually 0; non-zero for sub-allocations.
 * @param out_cuda_array         filled with the level-0 array (CUarray)
 *                               suitable for CUDA_MEMCPY2D::dstArray.
 * @param out_handle             opaque handle for cuda_p2p_release_image.
 */
CUDA_P2P_API CudaP2PError cuda_p2p_import_vulkan_image(
    CudaP2PContextHandle ctx,
    void* external_memory_handle,
    uint64_t memory_size,
    const CudaP2PImageDesc* desc,
    uint64_t offset_in_memory,
    void** out_cuda_array,
    CudaP2PImageHandle* out_handle);

CUDA_P2P_API void cuda_p2p_release_image(
    CudaP2PContextHandle ctx,
    CudaP2PImageHandle h);

/* ============================================================================
 * Device-side copies (the Vulkan tier-3 capture per-frame buffer->texture copy)
 * ============================================================================ */

/**
 * @brief Copy a flat device buffer (e.g. an AJA-DMA'd VkBuffer, CUDA-imported)
 *        into a CUDA array (level 0 of a CUDA-imported VkImage). Synchronous
 *        (stream-synchronized on return). Rows are @p width_bytes wide, @p
 *        height of them, source row stride @p src_pitch_bytes.
 */
CUDA_P2P_API CudaP2PError cuda_p2p_copy_buffer_to_array(
    CudaP2PContextHandle ctx,
    void* src_device_ptr,
    void* dst_cuda_array,
    uint32_t width_bytes,
    uint32_t height,
    uint32_t src_pitch_bytes);

/**
 * @brief Copy a CUDA array (level 0 of a CUDA-imported VkImage) into a flat
 *        device buffer (e.g. an RDMA-capable CUDA VMM slot the card DMAs out).
 *        Synchronous (stream-synchronized on return). The inverse of
 *        cuda_p2p_copy_buffer_to_array — the per-frame texture->buffer copy for
 *        GPU-direct OUTPUT. Rows are @p width_bytes wide, @p height of them,
 *        destination row stride @p dst_pitch_bytes.
 */
CUDA_P2P_API CudaP2PError cuda_p2p_copy_array_to_buffer(
    CudaP2PContextHandle ctx,
    void* src_cuda_array,
    void* dst_device_ptr,
    uint32_t width_bytes,
    uint32_t height,
    uint32_t dst_pitch_bytes);

/**
 * @brief Upload host bytes into a flat device pointer (cuMemcpyHtoD).
 *        Utility — mainly for tests that need to seed a device buffer without
 *        a peer DMA source.
 */
CUDA_P2P_API CudaP2PError cuda_p2p_upload_buffer(
    CudaP2PContextHandle ctx,
    void* dst_device_ptr,
    const void* host_data,
    uint64_t size);

/* ============================================================================
 * External semaphore primitives (D3D12 fence + Vulkan timeline)
 * ============================================================================ */

CUDA_P2P_API CudaP2PError cuda_p2p_import_d3d12_fence(
    CudaP2PContextHandle ctx,
    void* shared_fence_handle,
    CudaP2PSemaphoreHandle* sem);

CUDA_P2P_API CudaP2PError cuda_p2p_import_vulkan_semaphore(
    CudaP2PContextHandle ctx,
    void* external_semaphore_handle,
    CudaP2PSemaphoreHandle* sem);

/**
 * @brief Import a BINARY Vulkan external semaphore (OPAQUE_WIN32 / OPAQUE_FD).
 *        Sibling of cuda_p2p_import_vulkan_semaphore (which imports a TIMELINE
 *        semaphore). Used by the GPU-direct OUTPUT fence, where QRhi signals a
 *        binary VkSemaphore at queue-submit and CUDA waits on it before the
 *        array->buffer copy. Wait with cuda_p2p_wait_semaphore(value=0) — the
 *        value is ignored for binary (OPAQUE) semaphore types.
 */
CUDA_P2P_API CudaP2PError cuda_p2p_import_vulkan_semaphore_binary(
    CudaP2PContextHandle ctx,
    void* external_semaphore_handle,
    CudaP2PSemaphoreHandle* sem);

/**
 * @brief Schedule a wait on the given external-semaphore value inside the
 *        bridge's CUDA stream. Non-blocking on the host.
 */
CUDA_P2P_API CudaP2PError cuda_p2p_wait_semaphore(
    CudaP2PContextHandle ctx,
    CudaP2PSemaphoreHandle sem,
    uint64_t value);

CUDA_P2P_API void cuda_p2p_release_semaphore(
    CudaP2PContextHandle ctx,
    CudaP2PSemaphoreHandle sem);

/* ============================================================================
 * Synchronization
 * ============================================================================ */

CUDA_P2P_API CudaP2PError cuda_p2p_sync(CudaP2PContextHandle ctx);

#ifdef __cplusplus
}
#endif
