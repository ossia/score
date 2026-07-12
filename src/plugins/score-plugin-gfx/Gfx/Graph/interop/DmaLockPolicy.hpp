#pragma once
#include <cstdint>

/**
 * @file DmaLockPolicy.hpp
 * @brief DMA-lock policy concept for the shared NVIDIA-DVP shims.
 *
 * The DVP capture/output strategy templates (DvpCapture*, DvpOutput*) are
 * vendor-neutral: they own all the DVP + RHI machinery. The one per-vendor
 * variable is how the page-locked sysmem slot is pinned for the card's DMA
 * engine. A vendor supplies a small policy:
 *
 *   struct Policy {
 *     bool valid() const noexcept;                              // preflight
 *     bool lock(const void* ptr, std::uint32_t bytes) noexcept; // page-lock
 *     void unlock(const void* ptr, std::uint32_t bytes) noexcept;
 *   };
 *
 * AJA's policy (AjaDmaLockPolicy) wraps CNTV2Card::DMABufferLock. Vendors that
 * fill the slot via a plain CPU memcpy (no card-side DMA into the sysmem, e.g.
 * DeckLink) use NoDmaLock below.
 */

namespace score::gfx::interop
{

/// No-op DMA lock: the slot is filled by a CPU copy, so no page-pin is needed.
struct NoDmaLock
{
  bool valid() const noexcept { return true; }
  bool lock(const void*, std::uint32_t) noexcept { return true; }
  void unlock(const void*, std::uint32_t) noexcept { }
};

} // namespace score::gfx::interop
