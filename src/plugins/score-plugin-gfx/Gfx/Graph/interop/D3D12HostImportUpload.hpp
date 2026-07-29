#pragma once

/**
 * @file D3D12HostImportUpload.hpp
 * @brief Zero-copy CPU->GPU upload on D3D12, the counterpart of
 *        VkHostImportUpload.
 *
 * `ID3D12Device3::OpenExistingHeapFromAddress` wraps memory the application
 * already owns in an ID3D12Heap, which is D3D12's equivalent of
 * VK_EXT_external_memory_host: a capture buffer the card has DMA'd into becomes
 * something the GPU can copy out of directly, with no staging copy.
 *
 * Three constraints, all established by probing the runtime rather than read
 * off a page (see rdma-test-utils/d3d12probe.cpp and d3d12pitch.cpp):
 *
 *  - The address must come from a virtual-memory reservation at the 64 KB
 *    system allocation granularity. `_aligned_malloc` memory is refused with
 *    E_INVALIDARG, so slots must come from `importableAlloc`. The *size* needs
 *    no rounding -- the opened heap reports exactly the requested bytes.
 *  - The opened heap comes back CUSTOM with SHARED|SHARED_CROSS_ADAPTER, and a
 *    placed resource is refused unless it opts in with
 *    D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER. Without it every combination of
 *    initial state fails; with it COPY_SOURCE succeeds.
 *  - `D3D12_PLACED_SUBRESOURCE_FOOTPRINT::RowPitch` is documented to require
 *    256-byte alignment. AMD's Windows driver was measured copying unaligned
 *    pitches byte-correctly anyway, but that is undocumented tolerance and not
 *    something to depend on, so init() refuses an unaligned pitch unless
 *    SCORE_GFX_D3D12_ALLOW_UNALIGNED_PITCH is set.
 */

#include <score_plugin_gfx_export.h>

#include <cstddef>
#include <cstdint>
#include <vector>

class QRhi;
class QRhiTexture;
class QRhiCommandBuffer;

namespace score::gfx::interop
{

class SCORE_PLUGIN_GFX_EXPORT D3D12HostImportUpload
{
public:
  ~D3D12HostImportUpload();

  /// D3D12 backend + ID3D12Device3 available. Returns 0 when unusable, else the
  /// required address alignment (the 64 KB allocation granularity).
  static std::size_t requiredAlignment(QRhi& rhi) noexcept;

  /// True when @p rowPitch can be used as a placed-footprint pitch. False means
  /// the caller must not take this rung (see the pitch note above).
  static bool pitchUsable(std::size_t rowPitch) noexcept;

  /// Opens one heap + placed buffer per slot. Each pointer must come from
  /// `importableAlloc` and each slot must be at least `bytes` long.
  /// @p rowPitch is the producer's stride, used to build the copy footprint.
  bool init(
      QRhi& rhi, const std::vector<void*>& slots, std::size_t bytes,
      std::size_t rowPitch);
  void release();

  bool valid() const noexcept { return !m_buffers.empty(); }

  /// Records the slot -> texture copy on @p cb. @p srcOffset is the plane's
  /// byte offset inside the slot, for planar frames held in one buffer.
  bool copyToTexture(
      QRhiCommandBuffer& cb, QRhiTexture& tex, std::size_t slot, int width,
      int height, std::size_t srcOffset = 0) noexcept;

private:
  struct Impl;
  Impl* m_d{};
  std::vector<void*> m_buffers; ///< ID3D12Resource per slot, type-erased
};

} // namespace score::gfx::interop
