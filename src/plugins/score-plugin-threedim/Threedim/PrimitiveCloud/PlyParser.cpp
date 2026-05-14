#include "PlyParser.hpp"

#include <miniply.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace Threedim::PrimitiveCloud
{

namespace
{

// Standard mesh column set. A PLY whose vertex element has only these
// columns (and a face element) is a regular triangle mesh and goes
// through the existing AssetLoader mesh path.
bool is_mesh_column(std::string_view name) noexcept
{
  static constexpr std::string_view mesh_cols[] = {
      "x", "y", "z",
      "nx", "ny", "nz",
      "red", "green", "blue", "alpha",
      "r", "g", "b", "a",
      "s", "t", "u", "v",
      "texture_u", "texture_v",
  };
  for(auto c : mesh_cols)
    if(name == c)
      return true;
  return false;
}

// Bytes per PLY scalar type. Lists aren't supported on the splat path
// (caller filters them out) so countType is irrelevant here.
uint32_t byte_size_for(miniply::PLYPropertyType t) noexcept
{
  using PT = miniply::PLYPropertyType;
  switch(t)
  {
    case PT::Char:   case PT::UChar:  return 1;
    case PT::Short:  case PT::UShort: return 2;
    case PT::Int:    case PT::UInt:   return 4;
    case PT::Float:                    return 4;
    case PT::Double:                   return 8;
    default: return 0;
  }
}

// Round `v` up to the next multiple of `align` (a power of two).
uint32_t align_up(uint32_t v, uint32_t align) noexcept
{
  return (v + (align - 1)) & ~(align - 1);
}

// Detect whether the vertex element looks like a splat. Returns true
// if it carries any column NOT in the standard mesh set OR if there
// is no `face` element in the file.
bool detect_splat_shape(miniply::PLYReader& reader)
{
  bool has_face = false;
  bool has_extra = false;

  for(uint32_t i = 0, end = reader.num_elements(); i < end; ++i)
  {
    auto* el = reader.get_element(i);
    if(!el) continue;
    if(el->name == "face")
    {
      has_face = true;
      continue;
    }
    if(el->name == miniply::kPLYVertexElement)
    {
      for(auto& p : el->properties)
      {
        // List columns aren't a splat thing — skip.
        if(p.countType != miniply::PLYPropertyType::None)
          continue;
        if(!is_mesh_column(p.name))
        {
          has_extra = true;
          break;
        }
      }
    }
  }
  return has_extra || !has_face;
}

// Recognise a known column-name fingerprint and return the canonical
// format_id. Empty result means "unknown / wired by hand".
std::string detect_format_id(const miniply::PLYElement& vtx)
{
  bool has_f_dc = false;
  bool has_f_rest = false;
  bool has_scale = false;
  bool has_rot = false;
  bool has_opacity = false;
  for(auto& p : vtx.properties)
  {
    if(p.countType != miniply::PLYPropertyType::None)
      continue;
    const auto& n = p.name;
    if(n == "f_dc_0" || n == "f_dc_1" || n == "f_dc_2") has_f_dc = true;
    else if(n.rfind("f_rest_", 0) == 0) has_f_rest = true;
    else if(n == "scale_0" || n == "scale_1" || n == "scale_2") has_scale = true;
    else if(n == "rot_0" || n == "rot_1" || n == "rot_2" || n == "rot_3") has_rot = true;
    else if(n == "opacity") has_opacity = true;
  }
  if(has_f_dc && has_f_rest && has_scale && has_rot && has_opacity)
    return "3dgs.classic";
  return {};
}

} // namespace

bool ply_is_splat_shaped(std::string_view path)
{
  // miniply::PLYReader expects a NUL-terminated path. string_view from
  // halp::file_port::filename is null-terminated in practice but not
  // guaranteed; copy to be safe.
  std::string p{path};
  miniply::PLYReader reader(p.c_str());
  if(!reader.valid())
    return false;
  return detect_splat_shape(reader);
}

ossia::primitive_cloud_component_ptr parse_ply(std::string_view path)
{
  std::string p{path};
  miniply::PLYReader reader(p.c_str());
  if(!reader.valid())
    return nullptr;

  if(!detect_splat_shape(reader))
    return nullptr;

  // Walk to the vertex element.
  while(reader.has_element())
  {
    if(!reader.element_is(miniply::kPLYVertexElement))
    {
      reader.next_element();
      continue;
    }
    if(!reader.load_element())
      return nullptr;
    break;
  }
  if(!reader.has_element())
    return nullptr;

  const auto* vtx = reader.element();
  if(!vtx)
    return nullptr;
  const uint32_t N = reader.num_rows();
  if(N == 0)
    return nullptr;

  // Skip list columns: not part of the splat schema. We collect the
  // scalar-only column subset and lay them out tightly in row order.
  // The conventional layout is: each scalar at its natural alignment,
  // row stride padded to 4 (almost every splat PLY is all-float so
  // this is essentially "sum of bytes per column"; we do the more
  // conservative thing for mixed-type files).
  struct Col
  {
    uint32_t prop_idx;
    miniply::PLYPropertyType type;
    uint32_t offset_in_row;
    uint32_t size;
    std::string name;
  };
  std::vector<Col> cols;
  cols.reserve(vtx->properties.size());

  uint32_t row_offset = 0;
  uint32_t row_align = 1;
  for(uint32_t i = 0; i < (uint32_t)vtx->properties.size(); ++i)
  {
    const auto& p = vtx->properties[i];
    if(p.countType != miniply::PLYPropertyType::None)
      continue; // list — skip
    const uint32_t sz = byte_size_for(p.type);
    if(sz == 0)
      continue;
    row_offset = align_up(row_offset, sz);
    cols.push_back(Col{i, p.type, row_offset, sz, p.name});
    row_offset += sz;
    if(sz > row_align)
      row_align = sz;
  }
  if(cols.empty())
    return nullptr;
  const uint32_t row_stride = align_up(row_offset, row_align);

  // Allocate the packed row buffer. shared_ptr<uint8_t[]> wraps the
  // storage; the buffer_resource keeps it alive via its data field.
  const std::size_t bytes = std::size_t(N) * row_stride;
  auto storage = std::shared_ptr<uint8_t[]>(new uint8_t[bytes]());

  // Extract each scalar column at its row offset.
  for(const auto& c : cols)
  {
    uint32_t idx = c.prop_idx;
    reader.extract_properties_with_stride(
        &idx, 1, c.type,
        storage.get() + c.offset_in_row, row_stride);
  }

  // AABB: find x/y/z by name, read each position from the packed buffer.
  ossia::aabb bounds{};
  bounds.min[0] = bounds.min[1] = bounds.min[2] = 1.f;
  bounds.max[0] = bounds.max[1] = bounds.max[2] = -1.f;
  {
    const Col* cx = nullptr; const Col* cy = nullptr; const Col* cz = nullptr;
    for(const auto& c : cols)
    {
      if(c.name == "x") cx = &c;
      else if(c.name == "y") cy = &c;
      else if(c.name == "z") cz = &c;
    }
    if(cx && cy && cz
       && cx->type == miniply::PLYPropertyType::Float
       && cy->type == miniply::PLYPropertyType::Float
       && cz->type == miniply::PLYPropertyType::Float)
    {
      const uint8_t* base = storage.get();
      for(uint32_t i = 0; i < N; ++i)
      {
        float x, y, z;
        std::memcpy(&x, base + i * row_stride + cx->offset_in_row, sizeof(float));
        std::memcpy(&y, base + i * row_stride + cy->offset_in_row, sizeof(float));
        std::memcpy(&z, base + i * row_stride + cz->offset_in_row, sizeof(float));
        bounds.expand(x, y, z);
      }
    }
  }

  // Wrap as a buffer_resource. Storage uses storage_buffer usage so
  // ScenePreprocessor uploads it as an SSBO.
  auto br = std::make_shared<ossia::buffer_resource>();
  br->resource = ossia::buffer_data{
      .data = std::shared_ptr<const void>(storage, storage.get()),
      .byte_size = (int64_t)bytes,
      .usage_hint = ossia::buffer_data::usage::storage_buffer};
  br->content_hash = (uint64_t)(uintptr_t)storage.get();

  auto out = std::make_shared<ossia::primitive_cloud_component>();
  out->raw_data = std::move(br);
  out->row_stride = row_stride;
  out->primitive_count = N;
  out->topology = ossia::primitive_topology::points;
  out->format_id = detect_format_id(*vtx);
  // For known formats, name the per-row struct so ScenePreprocessor
  // exposes raw_data as a per-vertex `splat: <Type>` ATTRIBUTE and the
  // CSF can declare a matching TYPES entry. Empty falls back to the
  // legacy AUXILIARY raw_splats path.
  if(out->format_id == "3dgs.classic")
    out->struct_type_name = "Splat3DGS";
  out->bounds = bounds;
  out->stable_id = ossia::mint_stable_id();

  // (format_params left empty for v1: format CSF authors declare the
  // LAYOUT block themselves matching the PLY column order. Adding a
  // reflective column-table here later is a pure addition — no
  // consumer depends on its absence.)

  return out;
}

} // namespace Threedim::PrimitiveCloud
