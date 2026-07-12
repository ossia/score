/**
 * @file CudaInterop.h
 * @brief C API for vendor-neutral GPU<->device CUDA interop.
 *
 * Score's GPU-direct video paths render compute-shader output (v210, UYVY,
 * BGRA, ...) into a SHARED GPU buffer, then ask the bridge for (a) a flat
 * GPU device pointer the peer device can DMA, and (b) cross-API fences
 * (D3D12 / Vulkan) so the peer never reads a half-written frame.
 *
 * Consumers are capture-card output addons that DMA straight from the shared
 * GPU buffer (SDI/HDMI RDMA today; SMPTE 2110 and other physical-address
 * paths planned).
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

// Bridge symbols are exported from libscore_plugin_gfx so capture-card
// addon DSOs can resolve them at load time.
#define CUDA_INTEROP_API SCORE_PLUGIN_GFX_EXPORT

/* Opaque handle types */
typedef struct CudaInteropContext_t* CudaInteropContextHandle;
typedef struct CudaInteropResource_t* CudaInteropResourceHandle;
typedef struct CudaInteropSemaphore_t* CudaInteropSemaphoreHandle;
typedef struct CudaInteropImage_t* CudaInteropImageHandle;

/* Image format — values match CUarray_format from the CUDA driver API. */
typedef enum CudaInteropImageFormat {
  CUDA_INTEROP_FORMAT_UNSIGNED_INT8  = 0x01,
  CUDA_INTEROP_FORMAT_UNSIGNED_INT16 = 0x02,
  CUDA_INTEROP_FORMAT_UNSIGNED_INT32 = 0x03,
  CUDA_INTEROP_FORMAT_SIGNED_INT8    = 0x08,
  CUDA_INTEROP_FORMAT_SIGNED_INT16   = 0x09,
  CUDA_INTEROP_FORMAT_SIGNED_INT32   = 0x0a,
  CUDA_INTEROP_FORMAT_HALF           = 0x10,
  CUDA_INTEROP_FORMAT_FLOAT          = 0x20,
} CudaInteropImageFormat;

typedef struct CudaInteropImageDesc {
  uint32_t width;
  uint32_t height;
  uint32_t depth;        /* 0 or 1 for 2D */
  uint32_t numChannels;
  CudaInteropImageFormat format;
  uint32_t flags;        /* reserved; pass 0 */
} CudaInteropImageDesc;

typedef enum CudaInteropError {
  CUDA_INTEROP_SUCCESS = 0,
  CUDA_INTEROP_ERROR_NOT_INITIALIZED,
  CUDA_INTEROP_ERROR_INIT_FAILED,
  CUDA_INTEROP_ERROR_NO_DEVICE,
  CUDA_INTEROP_ERROR_ALLOC_FAILED,
  CUDA_INTEROP_ERROR_INTEROP_FAILED,
  CUDA_INTEROP_ERROR_COPY_FAILED,
  CUDA_INTEROP_ERROR_SYNC_FAILED,
  CUDA_INTEROP_ERROR_INVALID_HANDLE,
  CUDA_INTEROP_ERROR_INVALID_PARAMETER,
  CUDA_INTEROP_ERROR_FORMAT_NOT_SUPPORTED,
  CUDA_INTEROP_ERROR_UNKNOWN
} CudaInteropError;

/* ============================================================================
 * Context management
 * ============================================================================ */

CUDA_INTEROP_API CudaInteropError cuda_interop_init(CudaInteropContextHandle* ctx);
CUDA_INTEROP_API void cuda_interop_shutdown(CudaInteropContextHandle ctx);
CUDA_INTEROP_API bool cuda_interop_available(void);
CUDA_INTEROP_API const char* cuda_interop_get_error_string(CudaInteropContextHandle ctx);

/* ============================================================================
 * Shared GPU buffer importers
 *
 * Each backend allocates a SHARED + UAV-bindable buffer; the matching
 * importer registers it with CUDA, returns a flat GPU device pointer
 * (suitable for AJA's DMABufferLock(inRDMA=true), Magewell's
 * MWCaptureVideoFrameToPhysicalAddress, Rivermax's rmax_register_memory)
 * plus an opaque handle that cuda_interop_release_buffer cleans up.
 * ============================================================================ */

/** @brief Import a SHARED + D3D11_BIND_UNORDERED_ACCESS ID3D11Buffer. */
CUDA_INTEROP_API CudaInteropError cuda_interop_import_d3d11_buffer(
    CudaInteropContextHandle ctx,
    void* d3d11_buffer,                   /* ID3D11Buffer* */
    void* d3d11_device,                   /* ID3D11Device* (unused, kept for API stability) */
    uint32_t buffer_size,
    void** out_device_ptr,
    CudaInteropResourceHandle* out_handle);

/** @brief Import a SHARED placed D3D12 buffer resource (NT HANDLE). */
CUDA_INTEROP_API CudaInteropError cuda_interop_import_d3d12_buffer(
    CudaInteropContextHandle ctx,
    void* shared_resource_handle,         /* Win32 HANDLE */
    uint64_t buffer_size,
    void** out_device_ptr,
    CudaInteropResourceHandle* out_handle);

/** @brief Import a Vulkan VkBuffer backed by external memory (Win32 HANDLE / fd). */
CUDA_INTEROP_API CudaInteropError cuda_interop_import_vulkan_buffer(
    CudaInteropContextHandle ctx,
    void* external_memory_handle,
    uint64_t buffer_size,
    void** out_device_ptr,
    CudaInteropResourceHandle* out_handle);

/**
 * @brief Register a GL Shader Storage Buffer Object for P2P.
 * GL <-> CUDA buffer interop is direct. The GL context must be current
 * on the calling thread.
 */
CUDA_INTEROP_API CudaInteropError cuda_interop_import_gl_buffer(
    CudaInteropContextHandle ctx,
    uint32_t gl_buffer_id,
    uint32_t buffer_size,
    void** out_device_ptr,
    CudaInteropResourceHandle* out_handle);

CUDA_INTEROP_API void cuda_interop_release_buffer(
    CudaInteropContextHandle ctx,
    CudaInteropResourceHandle h);

/**
 * @brief Register a GL buffer for CUDA interop WITHOUT keeping it mapped.
 *
 * Unlike cuda_interop_import_gl_buffer (which maps once and leaves the buffer
 * mapped for its lifetime — correct when CUDA only ever *reads* a GL-written
 * buffer, e.g. the output path), this registers the buffer and immediately
 * returns it to GL ownership. Use it when CUDA *writes* the buffer and GL then
 * reads it: the write must go through cuda_interop_gl_write_buffer, which maps,
 * copies, and unmaps so the CUDA writes are flushed before GL samples. A GL
 * buffer that stays mapped while CUDA writes it is never seen coherently by GL.
 */
CUDA_INTEROP_API CudaInteropError cuda_interop_register_gl_buffer(
    CudaInteropContextHandle ctx,
    uint32_t gl_buffer_id,
    uint32_t buffer_size,
    CudaInteropResourceHandle* out_handle);

/**
 * @brief Map a GL buffer registered with cuda_interop_register_gl_buffer, copy
 *        @p size bytes from @p src_device_ptr into it, then unmap (which
 *        flushes the writes back to GL). Stream-synchronised on return, so the
 *        caller may issue the GL read immediately afterwards.
 */
CUDA_INTEROP_API CudaInteropError cuda_interop_gl_write_buffer(
    CudaInteropContextHandle ctx,
    CudaInteropResourceHandle h,
    void* src_device_ptr,
    uint32_t size);

/**
 * @brief Register a GL *texture* (image) for CUDA interop — the capture-collapse
 *        path. Unlike cuda_interop_register_gl_buffer (which stages
 *        through an SSBO the caller must then glTexSubImage2D into the sampled
 *        texture), this registers the sampled texture itself so
 *        cuda_interop_gl_write_image can cuMemcpy2D a bounce straight into its
 *        level-0 array — collapsing the two render-thread VRAM copies into one.
 *        Register-only (not kept mapped); the write path maps/unmaps per frame
 *        so GL sees coherent data. @p gl_target is the raw GL texture target
 *        (e.g. GL_TEXTURE_2D). Returns CUDA_INTEROP_ERROR_INTEROP_FAILED if the
 *        driver lacks cuGraphicsGLRegisterImage or the texture isn't
 *        CUDA-registrable — callers fall back to the buffer path.
 */
CUDA_INTEROP_API CudaInteropError cuda_interop_register_gl_image(
    CudaInteropContextHandle ctx,
    uint32_t gl_texture_id,
    uint32_t gl_target,
    CudaInteropResourceHandle* out_handle);

/**
 * @brief Map a GL texture registered with cuda_interop_register_gl_image, copy a
 *        pitched region from @p src_device_ptr into its level-0 array via
 *        cuMemcpy2D, then unmap (flushing the CUDA writes back to GL).
 *        Stream-synchronised on return, so the caller may sample the texture
 *        immediately afterwards. Rows are @p width_bytes wide, @p height of
 *        them, source row stride @p src_pitch_bytes.
 */
CUDA_INTEROP_API CudaInteropError cuda_interop_gl_write_image(
    CudaInteropContextHandle ctx,
    CudaInteropResourceHandle h,
    void* src_device_ptr,
    uint32_t width_bytes,
    uint32_t height,
    uint32_t src_pitch_bytes);

/* ============================================================================
 * CUDA-owned linear buffers (pinnable for third-party DMA)
 *
 * nvidia_p2p_get_pages — the kernel interface behind every vendor's
 * "pin GPU memory" call (AJA DMABufferLock(inRDMA=true), Magewell, ...) —
 * only accepts VA ranges owned by the CUDA allocator. Graphics-API memory
 * imported into CUDA (cuGraphicsGLRegisterBuffer et al) can be *read* by
 * CUDA but never pinned for third-party DMA. Strategies that need a
 * vendor-pinnable buffer therefore allocate it here and bridge to/from
 * the graphics API with cuda_interop_copy_dtod (one VRAM->VRAM copy).
 * ============================================================================ */

/**
 * @brief Allocate `size` bytes of CUDA linear device memory
 *        (cuMemAlloc). The range is marked SYNC_MEMOPS when the driver
 *        supports it so third-party DMA engines stay coherent with
 *        in-stream work. Free with cuda_interop_free_buffer.
 */
CUDA_INTEROP_API CudaInteropError cuda_interop_alloc_buffer(
    CudaInteropContextHandle ctx,
    uint64_t size,
    void** out_device_ptr);

CUDA_INTEROP_API void cuda_interop_free_buffer(
    CudaInteropContextHandle ctx,
    void* device_ptr);

/**
 * @brief Flat device-to-device copy (cuMemcpyDtoDAsync on the bridge
 *        stream + stream sync — synchronous on return). Either pointer
 *        may be CUDA-owned or a mapped graphics resource.
 */
/**
 * @brief Pitched flat device-to-device copy (cuMemcpy2DAsync + stream sync).
 *        For bridging between a tightly-packed wire frame and a row-padded
 *        linear image/buffer view.
 */
CUDA_INTEROP_API CudaInteropError cuda_interop_copy_dtod_2d(
    CudaInteropContextHandle ctx,
    void* dst_device_ptr,
    uint64_t dst_pitch_bytes,
    void* src_device_ptr,
    uint64_t src_pitch_bytes,
    uint64_t width_bytes,
    uint64_t height);

CUDA_INTEROP_API CudaInteropError cuda_interop_copy_dtod(
    CudaInteropContextHandle ctx,
    void* dst_device_ptr,
    void* src_device_ptr,
    uint64_t size);

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
 * @param out_handle             opaque handle for cuda_interop_release_image.
 */
CUDA_INTEROP_API CudaInteropError cuda_interop_import_vulkan_image(
    CudaInteropContextHandle ctx,
    void* external_memory_handle,
    uint64_t memory_size,
    const CudaInteropImageDesc* desc,
    uint64_t offset_in_memory,
    void** out_cuda_array,
    CudaInteropImageHandle* out_handle);

CUDA_INTEROP_API void cuda_interop_release_image(
    CudaInteropContextHandle ctx,
    CudaInteropImageHandle h);

/* ============================================================================
 * Device-side copies (the Vulkan capture per-frame buffer->texture copy)
 * ============================================================================ */

/**
 * @brief Copy a flat device buffer (e.g. an AJA-DMA'd VkBuffer, CUDA-imported)
 *        into a CUDA array (level 0 of a CUDA-imported VkImage). Synchronous
 *        (stream-synchronized on return). Rows are @p width_bytes wide, @p
 *        height of them, source row stride @p src_pitch_bytes.
 */
CUDA_INTEROP_API CudaInteropError cuda_interop_copy_buffer_to_array(
    CudaInteropContextHandle ctx,
    void* src_device_ptr,
    void* dst_cuda_array,
    uint32_t width_bytes,
    uint32_t height,
    uint32_t src_pitch_bytes);

/**
 * @brief Copy a CUDA array (level 0 of a CUDA-imported VkImage) into a flat
 *        device buffer (e.g. an RDMA-capable CUDA VMM slot the card DMAs out).
 *        Synchronous (stream-synchronized on return). The inverse of
 *        cuda_interop_copy_buffer_to_array — the per-frame texture->buffer copy for
 *        GPU-direct OUTPUT. Rows are @p width_bytes wide, @p height of them,
 *        destination row stride @p dst_pitch_bytes.
 */
CUDA_INTEROP_API CudaInteropError cuda_interop_copy_array_to_buffer(
    CudaInteropContextHandle ctx,
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
CUDA_INTEROP_API CudaInteropError cuda_interop_upload_buffer(
    CudaInteropContextHandle ctx,
    void* dst_device_ptr,
    const void* host_data,
    uint64_t size);

/**
 * @brief Download bytes from a flat device pointer into host memory
 *        (cuMemcpyDtoH). Utility — inverse of cuda_interop_upload_buffer;
 *        used by tests / diagnostics to inspect a device buffer.
 */
CUDA_INTEROP_API CudaInteropError cuda_interop_download_buffer(
    CudaInteropContextHandle ctx,
    void* host_data,
    void* src_device_ptr,
    uint64_t size);

/* ============================================================================
 * External semaphore primitives (D3D12 fence + Vulkan timeline)
 * ============================================================================ */

CUDA_INTEROP_API CudaInteropError cuda_interop_import_d3d12_fence(
    CudaInteropContextHandle ctx,
    void* shared_fence_handle,
    CudaInteropSemaphoreHandle* sem);

CUDA_INTEROP_API CudaInteropError cuda_interop_import_vulkan_semaphore(
    CudaInteropContextHandle ctx,
    void* external_semaphore_handle,
    CudaInteropSemaphoreHandle* sem);

/**
 * @brief Import a BINARY Vulkan external semaphore (OPAQUE_WIN32 / OPAQUE_FD).
 *        Sibling of cuda_interop_import_vulkan_semaphore (which imports a TIMELINE
 *        semaphore). Used by the GPU-direct OUTPUT fence, where QRhi signals a
 *        binary VkSemaphore at queue-submit and CUDA waits on it before the
 *        array->buffer copy. Wait with cuda_interop_wait_semaphore(value=0) — the
 *        value is ignored for binary (OPAQUE) semaphore types.
 */
CUDA_INTEROP_API CudaInteropError cuda_interop_import_vulkan_semaphore_binary(
    CudaInteropContextHandle ctx,
    void* external_semaphore_handle,
    CudaInteropSemaphoreHandle* sem);

/**
 * @brief Schedule a wait on the given external-semaphore value inside the
 *        bridge's CUDA stream. Non-blocking on the host.
 */
CUDA_INTEROP_API CudaInteropError cuda_interop_wait_semaphore(
    CudaInteropContextHandle ctx,
    CudaInteropSemaphoreHandle sem,
    uint64_t value);

CUDA_INTEROP_API void cuda_interop_release_semaphore(
    CudaInteropContextHandle ctx,
    CudaInteropSemaphoreHandle sem);

/* ============================================================================
 * Synchronization
 * ============================================================================ */

CUDA_INTEROP_API CudaInteropError cuda_interop_sync(CudaInteropContextHandle ctx);

#ifdef __cplusplus
}
#endif
