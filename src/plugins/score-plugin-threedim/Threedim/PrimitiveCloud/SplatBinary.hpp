#pragma once

#include <ossia/dataflow/geometry_port.hpp>

#include <string_view>

namespace Threedim::PrimitiveCloud
{

// Parse an Antimatter15 .splat file (32 bytes per primitive,
// fixed schema).
//
// On-disk row layout (little-endian, packed, no padding):
//   bytes  0..11   position xyz, 3 × float32
//   bytes 12..23   scale_xyz, 3 × float32 (linear, NOT log-space)
//   bytes 24..27   color rgba, 4 × uint8 unorm
//   bytes 28..31   rotation quat, 4 × uint8 (sign-encoded as
//                  (q + 1) * 127.5 around index 0; recipient
//                  reconstructs by (b - 128) / 128)
//
// We pass these bytes through verbatim. The "3dgs.splat-binary" preset's
// CSF declares the matching LAYOUT, dequantizes color8 to color, and
// reconstructs the quat from the int8s.
//
// Returns nullptr if `bytes.size() % 32 != 0` or the input is empty.
ossia::primitive_cloud_component_ptr parse_splat_binary(std::string_view bytes);

} // namespace Threedim::PrimitiveCloud
