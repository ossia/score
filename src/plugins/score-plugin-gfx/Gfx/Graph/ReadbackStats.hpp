#pragma once
#include <score_plugin_gfx_export.h>

#include <atomic>
#include <cstdint>

namespace score::gfx
{
// Counts the GPU -> CPU buffer readbacks the render graph issues itself, so a
// test can assert that a pipeline built entirely on the GPU never stalls on
// one. A relaxed increment per readback, which already costs a frame sync.
//
// `data` is a readback whose bytes the CPU then uses (the raw raster
// indirect-draw fallback). `flush` is one whose bytes are thrown away and
// that only exists to make QRhi flush pending host writes (the Metal native
// copy path): it does not stall the frame.
struct ReadbackStats
{
  std::atomic<uint64_t> data{0};
  std::atomic<uint64_t> flush{0};
};

SCORE_PLUGIN_GFX_TEST_EXPORT ReadbackStats& readbackStats() noexcept;
}
