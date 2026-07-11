#pragma once

/**
 * @file CudaVmmAllocator.hpp
 * @brief BAR1-mappable GPU memory allocator using the CUDA VMM API.
 *
 * Allocates GPU memory that is addressable from third-party DMA
 * engines (NIC, capture card with GPUDirect-RDMA support). The
 * resulting `CUdeviceptr` is the canonical handoff target for:
 *
 *   - Rivermax `rmx_register_memory()` — NIC writes RTP payloads
 *     directly into the registered GPU range, no host transit.
 *   - AJA `DMABufferLock(buf, inMap=false, inRDMA=true)` — when
 *     paired with `AJA_RDMA=1` driver build + `nvidia_peermem` kernel
 *     module.
 *   - Ximea `xiSetParam(XI_PRM_TRANSPORT_DATA_TARGET=GPU_RAM)` —
 *     industrial-camera GPUDirect capture.
 *
 * Mechanism (from Rivermax's `gpu.cpp:cudaAllocateMmap`):
 *
 *   cuMemGetAllocationGranularity(&gran, &prop, RECOMMENDED)
 *   round size up to multiple of gran
 *   cuMemCreate(&handle, size, &prop, 0)         // physical alloc
 *   cuMemAddressReserve(&dptr, size, ...)        // VA range
 *   cuMemMap(dptr, size, 0, handle, 0)           // bind VA to phys
 *   cuMemSetAccess(dptr, size, &access, 1)       // grant R/W
 *
 * Tear-down is symmetric: unmap, address-free, release. The handle
 * itself can be released early (after Map+SetAccess); the mapping
 * holds an internal reference.
 *
 * Requires CUDA driver 10.2+ (`CudaFunctions::vmmSupported == true`).
 * For older drivers, fall back to `cuMemAlloc` + tier-1 sysmem
 * mediation — but that's the consumer's choice; this class will
 * simply refuse to construct.
 */

#include <Gfx/Graph/interop/CudaFunctions.hpp>

#include <score_plugin_gfx_export.h>

#include <cstddef>

namespace score::gfx::interop
{

/** Move-only RAII handle for one VMM allocation. Empty after move. */
class SCORE_PLUGIN_GFX_EXPORT CudaVmmAllocation
{
public:
  CudaVmmAllocation() noexcept = default;

  CudaVmmAllocation(CudaVmmAllocation&& other) noexcept
      : m_cu(other.m_cu)
      , m_dptr(other.m_dptr)
      , m_handle(other.m_handle)
      , m_size(other.m_size)
      , m_handleReleased(other.m_handleReleased)
  {
    other.detach();
  }

  CudaVmmAllocation& operator=(CudaVmmAllocation&& other) noexcept
  {
    if(this != &other)
    {
      destroy();
      m_cu = other.m_cu;
      m_dptr = other.m_dptr;
      m_handle = other.m_handle;
      m_size = other.m_size;
      m_handleReleased = other.m_handleReleased;
      other.detach();
    }
    return *this;
  }

  CudaVmmAllocation(const CudaVmmAllocation&) = delete;
  CudaVmmAllocation& operator=(const CudaVmmAllocation&) = delete;

  ~CudaVmmAllocation() { destroy(); }

  /** True when this object owns a live mapping. */
  bool valid() const noexcept { return m_cu && m_dptr != 0 && m_size > 0; }

  /** The device pointer to hand to NIC / capture-card APIs.
   *  Bytes [0, size()) are valid. */
  CUdeviceptr devicePointer() const noexcept { return m_dptr; }

  /** Size in bytes. May be larger than requested due to granularity
   *  rounding. */
  std::size_t size() const noexcept { return m_size; }

  /** Export the underlying allocation as an OS shareable handle for
   *  Vulkan/D3D external-memory import (POSIX fd on Linux). Requires
   *  `exportable=true` at allocate() and a driver exposing
   *  cuMemExportToShareableHandle. The caller owns the fd (but note the
   *  Vulkan import consumes it on success). Returns -1 on failure. */
  int exportPosixFd() const noexcept;

  /** Export this mapped range as a **dma-buf** fd via
   *  cuMemGetHandleForAddressRange — the cross-API handle type that
   *  Vulkan (VK_EXT_external_memory_dma_buf) and GL (EGLImage) can import
   *  as an *aliasing* image, i.e. the CUDA→graphics zero-copy route behind
   *  AJA "capture-collapse" option (d). Unlike exportPosixFd() (opaque fd,
   *  which does NOT alias into Vulkan), a dma-buf fd is the spec-defined
   *  importable type.
   *
   *  Returns -1 when the device lacks CU_DEVICE_ATTRIBUTE_DMA_BUF_SUPPORTED
   *  (the export returns CUDA_ERROR_NOT_SUPPORTED) — empirically the case
   *  on the Quadro RTX 4000 + GeForce RTX 4090 in this rig under driver 595,
   *  which is why option (d) is NOT wired into the capture strategies and
   *  the bounce path is retained. Verified by scratchpad/rdmaprobe4.
   *
   *  @param pciBar1  If true, request a PCIe/BAR1 mapping for the dma-buf
   *                  (CU_MEM_RANGE_FLAG_DMA_BUF_MAPPING_TYPE_PCIE), needed
   *                  for third-party DMA peers; default is the plain export.
   *  The caller owns the returned fd (close() it, or the Vulkan/GL import
   *  dups it). Returns -1 on any failure. */
  int exportDmaBufFd(bool pciBar1 = false) const noexcept;

  /** Release the underlying physical-handle reference *while keeping*
   *  the mapping alive. Useful when the caller wants to ensure no
   *  inter-process handle leak after a successful map+setaccess.
   *  Idempotent. */
  void releaseHandleEarly() noexcept
  {
    if(m_cu && m_handle && !m_handleReleased)
    {
      m_cu->memRelease(m_handle);
      m_handleReleased = true;
    }
  }

private:
  friend class CudaVmmAllocator;

  void detach() noexcept
  {
    m_cu = nullptr;
    m_dptr = 0;
    m_handle = 0;
    m_size = 0;
    m_handleReleased = false;
  }

  void destroy() noexcept
  {
    if(!m_cu)
      return;
    if(m_dptr != 0 && m_size > 0)
    {
      m_cu->memUnmap(m_dptr, m_size);
      m_cu->memAddressFree(m_dptr, m_size);
    }
    if(m_handle && !m_handleReleased)
      m_cu->memRelease(m_handle);
    detach();
  }

  score::gfx::CudaFunctions* m_cu{};
  CUdeviceptr m_dptr{};
  CUmemGenericAllocationHandle m_handle{};
  std::size_t m_size{};
  bool m_handleReleased{};
};

/** Stateless allocator. Holds no state itself; the per-allocation
 *  bookkeeping lives in the returned `CudaVmmAllocation`. */
class SCORE_PLUGIN_GFX_EXPORT CudaVmmAllocator
{
public:
  /** Allocate `bytes` of BAR1-mappable GPU memory on `device`.
   *
   *  @param cu      Loaded CudaFunctions. Must have `vmmSupported == true`.
   *  @param device  CUDA device id (from `cuDeviceGet`).
   *  @param bytes   Requested allocation size; will be rounded up to
   *                 the recommended granularity (typically 2 MiB on
   *                 modern GPUs).
   *  @param exportable  If true, request a POSIX FD / Win32 HANDLE
   *                     exportable allocation (e.g. for cross-process
   *                     sharing or Vulkan import). Increases the cost
   *                     slightly; only set true if you'll actually
   *                     export.
   *  @return  A valid `CudaVmmAllocation` on success; an empty one
   *           (`valid() == false`) on failure. Use `cu.getErrorString`
   *           on the caller side for diagnostics.
   *
   *  The caller's CUDA context must be current on the calling thread
   *  before invoking this; the function does NOT push/pop a context. */
  static CudaVmmAllocation allocate(
      score::gfx::CudaFunctions& cu, int device, std::size_t bytes,
      bool exportable = false) noexcept;

  /** Round `bytes` up to the VMM allocation granularity for the given
   *  device. Useful when the caller wants to know the actual size that
   *  will be allocated (e.g. to align stride). */
  static std::size_t alignedSize(
      score::gfx::CudaFunctions& cu, int device, std::size_t bytes) noexcept;
};

} // namespace score::gfx::interop
