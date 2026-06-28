#pragma once
/**
 * @file VendorDmaRegistrar.hpp
 * @brief Vendor-supplied pin / unpin callbacks shared by the GPU-direct and
 *        host-staged output helpers.
 *
 * A capture/output card's SDK exposes a way to page-lock (pin) a memory region
 * for its DMA engine: AJA `DMABufferLock`, Magewell `MWPinVideoBuffer`,
 * Blackmagic DeckLink's frame allocator, NVIDIA Rivermax `rmx_register_memory`,
 * etc. The generic output helpers (GpuDirectOutput for GPU-side P2P buffers,
 * HostStagedOutput for host-side staging buffers) own the buffers and invoke
 * these callbacks once per buffer at init / shutdown; the vendor adapter keeps
 * its SDK call in its own addon.
 *
 * `registerSlot` returning false aborts init; partial state is rolled back
 * before the helper's `init()` returns.
 */
#include <cstdint>
#include <functional>

namespace score::gfx::interop
{
struct VendorDmaRegistrar
{
  std::function<bool(void* ptr, std::uint32_t size)> registerSlot;
  std::function<void(void* ptr, std::uint32_t size)> releaseSlot;
};
}
