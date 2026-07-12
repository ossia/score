#pragma once
/**
 * @file VendorDmaRegistrar.hpp
 * @brief Vendor-supplied pin / unpin callbacks shared by the GPU-direct and
 *        host-staged output helpers.
 *
 * A capture/output card's SDK exposes a way to page-lock (pin) a memory region
 * for its DMA engine: AJA `DMABufferLock`, Magewell `MWPinVideoBuffer`,
 * Blackmagic DeckLink's frame allocator, NVIDIA Rivermax `rmx_register_memory`,
 * etc. The generic output helpers (RdmaVideoOutput for GPU-side P2P buffers,
 * CpuStagedVideoOutput for host-side staging buffers) own the buffers and invoke
 * these callbacks once per buffer at init / shutdown; the vendor adapter keeps
 * its SDK call in its own addon.
 *
 * `registerSlot` returning false aborts init; partial state is rolled back
 * before the helper's `init()` returns.
 *
 * `verifyTransfer` (optional) is a one-time capability probe: after the
 * first slot is pinned, the helper hands its pointer back so the vendor can
 * attempt a single real DMA in the direction it will actually use. Pinning
 * a GPU buffer only proves nvidia_p2p_get_pages accepted the range — it does
 * NOT prove the card↔GPU PCIe path permits P2P in that direction. On AMD
 * platforms, cross-host-bridge P2P routinely passes posted writes (capture)
 * but blocks non-posted reads (playout), so a card can pin an output buffer
 * yet fail every transfer. Returning false here aborts init so the strategy
 * chain falls back cleanly instead of emitting silent per-frame drops. When
 * null, the helper skips the probe (pin success is taken as sufficient).
 */
#include <cstdint>
#include <functional>

namespace score::gfx::interop
{
struct VendorDmaRegistrar
{
  std::function<bool(void* ptr, std::uint32_t size)> registerSlot;
  std::function<void(void* ptr, std::uint32_t size)> releaseSlot;
  std::function<bool(void* ptr, std::uint32_t size)> verifyTransfer;
};
}
