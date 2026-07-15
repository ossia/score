#pragma once

#include <ossia/dataflow/geometry_port.hpp>

#include <span>
#include <string_view>

namespace Threedim::PrimitiveCloud
{

// Decode a Niantic SPZ v1-3 file into a primitive_cloud_component.
//
// SPZ stores splats column-grouped (positions, then scales, then
// rotations, then alphas, then colors, then SH) inside a gzip-
// compressed payload, in the RUB coordinate system. We unpack via
// the vendored Niantic library, rotate to RDF (the convention every
// existing 3dgs.classic preset assumes), then transpose into the
// canonical 62-float / 248-byte PLY-compatible row layout:
//
//   floats 0..2    x, y, z
//   floats 3..5    nx, ny, nz   (zero — not in SPZ)
//   floats 6..8    f_dc_0..2     (SH DC = colors)
//   floats 9..53   f_rest_0..44  (R coeffs, then G, then B; padded
//                                 with zero for shDegree<3)
//   float  54       opacity (pre-sigmoid)
//   floats 55..57  scale_0..2 (log-space)
//   floats 58..61  rot_0..3   (PLY convention w,x,y,z)
//
// Returns nullptr on parse failure or v4 files (ZSTD support not
// vendored — converting v4 → v3 with the upstream `spz-tool` works
// around it). Sets format_id = "3dgs.classic" so the existing preset
// picks it up transparently.
ossia::primitive_cloud_component_ptr parse_spz(std::string_view bytes);

} // namespace Threedim::PrimitiveCloud
