#pragma once

#include <ossia/dataflow/geometry_port.hpp>

#include <span>
#include <string_view>

namespace Threedim::PrimitiveCloud
{

// Cheap header-only sniff: is the PLY file at `path` shaped like a
// primitive cloud (no `face` element, or has columns outside the
// standard mesh set {x,y,z,nx,ny,nz,red,green,blue,alpha,s,t,u,v})?
// Reads only the textual header, doesn't load row data.
bool ply_is_splat_shaped(std::string_view path);

// Parse `path` and produce a primitive_cloud_component. The component's
// raw_data is a single tightly-packed buffer of the PLY rows: each row
// is a struct of the columns in their PLY-declared order, std430-style
// natural alignment (each float at +4, each int at +4, each uchar at
// +1 with no inter-field padding — but the row stride is rounded to
// the largest field alignment within the row, see
// internal::row_stride_for).
//
// Returns nullptr if the PLY is not splat-shaped, or if parsing fails.
//
// Every scalar column is listed in `columns` (name, PLY scalar type, byte
// offset in the row), so a consumer can read any layout by column name.
//
// Sets format_id from the column layout:
//   - exactly the 62 float32 columns of the INRIA export, in its order
//     -> "3dgs.classic" / "Splat3DGS"
//   - the same followed by filter_3D (Mip-Splatting, GOF)
//     -> "3dgs.mip" / "SplatMip"
//   - the same with scale_0/1 and no scale_2 (2DGS)
//     -> "2dgs.surfel" / "Splat2DGS"
//   - any other layout with x/y/z, f_dc_0..2, opacity, scale_0..2 and
//     rot_0..3 (no normals, SH degree 0 to 2, reordered, extra columns,
//     double or integer columns): "3dgs", with no struct type
//   - else empty (the user wires the chain by hand or saves a preset)
ossia::primitive_cloud_component_ptr parse_ply(std::string_view path);

} // namespace Threedim::PrimitiveCloud
