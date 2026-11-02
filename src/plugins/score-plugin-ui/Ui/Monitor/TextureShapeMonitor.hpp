#pragma once
#include <Process/Monitoring/Monitor.hpp>

#include <atomic>
#include <cstdint>

namespace Ui::Monitor
{
//! The texture a view's node got, for the views: score::gfx::TextureShape,
//! packed (TextureShape::pack). See Ui/Monitor/TextureLayers.hpp.
struct TextureShapeMonitor : Process::Monitor
{
  std::atomic<uint64_t> shape{};
};
}
