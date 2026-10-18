#pragma once
#include <ossia/dataflow/geometry_port.hpp>

#include <score_plugin_gfx_export.h>

#include <cstdint>
#include <span>
#include <vector>

namespace score::gfx
{
// The triangle list a fan over `order` draws: {order[0], order[i - 1], order[i]}
// for every i >= 2, the fan's own winding.
SCORE_PLUGIN_GFX_EXPORT
std::vector<uint32_t> triangleFanToList(std::span<const uint32_t> order);

// Rewrites a triangle_fan geometry as an indexed triangle list on the CPU. The
// vertex buffers are kept as they are, CPU or GPU; a CPU uint32 index buffer is
// appended. Returns false and leaves `g` untouched when it is not a fan, or a
// fan whose order is not visible on the CPU: a GPU index buffer, an indirect
// draw, or per-command draws.
SCORE_PLUGIN_GFX_EXPORT
bool lowerTriangleFan(ossia::geometry& g);
}
