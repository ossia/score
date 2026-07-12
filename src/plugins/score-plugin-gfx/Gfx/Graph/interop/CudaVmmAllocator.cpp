#include "CudaVmmAllocator.hpp"

namespace score::gfx::interop
{

namespace
{

/** Build the allocation descriptor common to both the granularity
 *  query and the `cuMemCreate` call. Centralised so they can't drift. */
CUmemAllocationProp makeAllocationProp(int device, bool exportable) noexcept
{
  CUmemAllocationProp prop{};
  prop.type = CU_MEM_ALLOCATION_TYPE_PINNED;
  prop.location.type = CU_MEM_LOCATION_TYPE_DEVICE;
  prop.location.id = device;

  // GPUDirect-RDMA capability flag — Rivermax sets this. The driver
  // honours it on NICs that advertise the capability; ignored otherwise.
  prop.allocFlags.gpuDirectRDMACapable = 1;

  if(exportable)
  {
#if defined(_WIN32)
    prop.requestedHandleTypes = CU_MEM_HANDLE_TYPE_WIN32;
#else
    prop.requestedHandleTypes = CU_MEM_HANDLE_TYPE_POSIX_FILE_DESCRIPTOR;
#endif
  }
  else
  {
    prop.requestedHandleTypes = CU_MEM_HANDLE_TYPE_NONE;
  }
  return prop;
}

std::size_t roundUp(std::size_t v, std::size_t multiple) noexcept
{
  if(multiple == 0)
    return v;
  return ((v + multiple - 1) / multiple) * multiple;
}

// CUpointer_attribute::CU_POINTER_ATTRIBUTE_SYNC_MEMOPS (driver ABI value).
constexpr int kCuPointerAttributeSyncMemops = 6;

} // namespace

std::size_t CudaVmmAllocator::alignedSize(
    score::gfx::CudaFunctions& cu, int device, std::size_t bytes) noexcept
{
  if(!cu.loaded() || !cu.vmmSupported || bytes == 0)
    return 0;

  const auto prop = makeAllocationProp(device, /*exportable=*/false);
  std::size_t gran = 0;
  if(cu.memGetGranularity(&gran, &prop, CU_MEM_ALLOC_GRANULARITY_RECOMMENDED)
     != CUDA_SUCCESS || gran == 0)
    return 0;
  return roundUp(bytes, gran);
}

CudaVmmAllocation CudaVmmAllocator::allocate(
    score::gfx::CudaFunctions& cu, int device, std::size_t bytes,
    bool exportable) noexcept
{
  CudaVmmAllocation result;
  if(!cu.loaded() || !cu.vmmSupported || bytes == 0)
    return result;

  const auto prop = makeAllocationProp(device, exportable);

  // Determine the alignment + size dictated by the device. Granularity
  // is typically 2 MiB on modern Quadro/Tesla; smaller on older parts.
  std::size_t gran = 0;
  if(cu.memGetGranularity(&gran, &prop, CU_MEM_ALLOC_GRANULARITY_RECOMMENDED)
     != CUDA_SUCCESS || gran == 0)
    return result;
  const std::size_t alignedBytes = roundUp(bytes, gran);

  // 1. cuMemCreate: physical allocation on the device.
  CUmemGenericAllocationHandle handle{};
  if(cu.memCreate(&handle, alignedBytes, &prop, 0) != CUDA_SUCCESS)
    return result;

  // 2. cuMemAddressReserve: carve a virtual-address range. Alignment
  //    matches granularity so the range is naturally aligned.
  CUdeviceptr dptr{};
  if(cu.memAddressReserve(&dptr, alignedBytes, gran, 0, 0) != CUDA_SUCCESS)
  {
    cu.memRelease(handle);
    return result;
  }

  // 3. cuMemMap: bind the VA range to the physical allocation.
  if(cu.memMap(dptr, alignedBytes, 0, handle, 0) != CUDA_SUCCESS)
  {
    cu.memAddressFree(dptr, alignedBytes);
    cu.memRelease(handle);
    return result;
  }

  // 4. cuMemSetAccess: grant the current device read-write access to
  //    the mapped range. Other devices in a multi-GPU setup would
  //    need additional access descriptors.
  CUmemAccessDesc access{};
  access.location.type = CU_MEM_LOCATION_TYPE_DEVICE;
  access.location.id = device;
  access.flags = CU_MEM_ACCESS_FLAGS_PROT_READWRITE;
  if(cu.memSetAccess(dptr, alignedBytes, &access, 1) != CUDA_SUCCESS)
  {
    cu.memUnmap(dptr, alignedBytes);
    cu.memAddressFree(dptr, alignedBytes);
    cu.memRelease(handle);
    return result;
  }

  // 5. Mark the range for synchronous memory ops so a third-party card DMA
  //    engine (Deltacast VHD_CreateSlotEx RDMAEnabled, Rivermax, ...) can
  //    target it directly. Best-effort hint: honoured on GPUDirect-RDMA
  //    capable GPUs, harmless (and may fail) elsewhere — not fatal.
  if(cu.pointerSetAttribute)
  {
    const int one = 1;
    cu.pointerSetAttribute(&one, kCuPointerAttributeSyncMemops, dptr);
  }

  result.m_cu = &cu;
  result.m_dptr = dptr;
  result.m_handle = handle;
  result.m_size = alignedBytes;
  result.m_handleReleased = false;
  return result;
}

int CudaVmmAllocation::exportPosixFd() const noexcept
{
#if defined(_WIN32)
  return -1;
#else
  if(!m_cu || !m_handle || m_handleReleased
     || !m_cu->memExportToShareableHandle)
    return -1;
  int fd = -1;
  // 1 == CU_MEM_HANDLE_TYPE_POSIX_FILE_DESCRIPTOR
  if(m_cu->memExportToShareableHandle(&fd, m_handle, 1, 0) != CUDA_SUCCESS)
    return -1;
  return fd;
#endif
}

int CudaVmmAllocation::exportDmaBufFd(bool pciBar1) const noexcept
{
#if defined(_WIN32)
  return -1;
#else
  // Exports the *mapped VA range* (not the physical handle), so it works
  // whether or not releaseHandleEarly() was called — only the mapping
  // needs to be live.
  if(!m_cu || m_dptr == 0 || m_size == 0
     || !m_cu->memGetHandleForAddressRange)
    return -1;
  const unsigned long long flags
      = pciBar1 ? CU_MEM_RANGE_FLAG_DMA_BUF_MAPPING_TYPE_PCIE : 0ull;
  int fd = -1;
  if(m_cu->memGetHandleForAddressRange(
         &fd, m_dptr, m_size, CU_MEM_RANGE_HANDLE_TYPE_DMA_BUF_FD, flags)
     != CUDA_SUCCESS)
    return -1; // CUDA_ERROR_NOT_SUPPORTED on non-dma-buf-capable devices
  return fd;
#endif
}

} // namespace score::gfx::interop
