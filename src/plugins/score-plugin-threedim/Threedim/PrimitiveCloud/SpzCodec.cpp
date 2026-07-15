#include "SpzCodec.hpp"

#include <load-spz.h>

#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

namespace Threedim::PrimitiveCloud
{

namespace
{

// Canonical row layout matching what the 3dgs.classic preset's
// AUXILIARY LAYOUT in 01_Decode.cs expects. Field offsets in floats.
struct CanonicalRow
{
  static constexpr uint32_t kFloats = 62;
  static constexpr uint32_t kBytes  = kFloats * sizeof(float);

  static constexpr uint32_t kPos    = 0;   // 3 floats
  static constexpr uint32_t kNormal = 3;   // 3 floats (zero-filled)
  static constexpr uint32_t kSHDC   = 6;   // 3 floats
  static constexpr uint32_t kSHRest = 9;   // 45 floats (channel-major)
  static constexpr uint32_t kAlpha  = 54;  // 1 float (pre-sigmoid)
  static constexpr uint32_t kScale  = 55;  // 3 floats (log-space)
  static constexpr uint32_t kRot    = 58;  // 4 floats (w,x,y,z)
};

} // namespace

ossia::primitive_cloud_component_ptr parse_spz(std::string_view bytes)
{
  if(bytes.empty())
    return nullptr;

  // The Niantic library expects the gzipped/NGSP payload as a
  // std::vector<uint8_t>. Copy in (single allocation; the cost is
  // dwarfed by the gzip inflate). Specify RUB→RDF in the unpack
  // options so the library handles the basis flip for us.
  std::vector<uint8_t> data(
      reinterpret_cast<const uint8_t*>(bytes.data()),
      reinterpret_cast<const uint8_t*>(bytes.data()) + bytes.size());

  spz::UnpackOptions opts;
  opts.to = spz::CoordinateSystem::RDF;

  spz::GaussianCloud cloud = spz::loadSpz(data, opts);
  if(cloud.numPoints <= 0 || cloud.positions.empty())
    return nullptr;

  const uint32_t N        = (uint32_t)cloud.numPoints;
  const uint32_t shDeg    = (uint32_t)cloud.shDegree;
  const uint32_t shCoefs  = (shDeg == 0) ? 0
                          : (shDeg == 1) ? 3
                          : (shDeg == 2) ? 8
                          : (shDeg == 3) ? 15
                          : 24; // degree 4
  const uint32_t restPad  = 15;  // 3dgs.classic preset always reads 15 R/G/B coefs

  if(cloud.positions.size() != (size_t)N * 3
     || cloud.scales.size() != (size_t)N * 3
     || cloud.rotations.size() != (size_t)N * 4
     || cloud.alphas.size() != (size_t)N
     || cloud.colors.size() != (size_t)N * 3)
  {
    return nullptr;
  }
  if(shCoefs > 0 && cloud.sh.size() != (size_t)N * shCoefs * 3)
    return nullptr;

  const std::size_t totalBytes
      = (std::size_t)N * (std::size_t)CanonicalRow::kBytes;
  auto storage = std::shared_ptr<uint8_t[]>(new uint8_t[totalBytes]());

  ossia::aabb bounds{};
  bounds.min[0] = bounds.min[1] = bounds.min[2] = 1.f;
  bounds.max[0] = bounds.max[1] = bounds.max[2] = -1.f;

  // Effective coefficient count we'll actually fill per-channel
  // (clamped to 15 — preset hardcodes 45 = 3·15 rest floats; degree-4
  // input gets truncated to degree 3 here, lossy but renderable).
  const uint32_t fillCoefs = (shCoefs > restPad) ? restPad : shCoefs;

  float* base = reinterpret_cast<float*>(storage.get());
  for(uint32_t i = 0; i < N; ++i)
  {
    float* row = base + (std::size_t)i * CanonicalRow::kFloats;

    // Position.
    const float x = cloud.positions[i * 3 + 0];
    const float y = cloud.positions[i * 3 + 1];
    const float z = cloud.positions[i * 3 + 2];
    row[CanonicalRow::kPos + 0] = x;
    row[CanonicalRow::kPos + 1] = y;
    row[CanonicalRow::kPos + 2] = z;
    bounds.expand(x, y, z);

    // Normals — not stored in SPZ; leave zero-filled.

    // SH DC (= colors).
    row[CanonicalRow::kSHDC + 0] = cloud.colors[i * 3 + 0];
    row[CanonicalRow::kSHDC + 1] = cloud.colors[i * 3 + 1];
    row[CanonicalRow::kSHDC + 2] = cloud.colors[i * 3 + 2];

    // SH rest. SPZ packs (R,G,B) inner per coefficient; PLY canonical
    // is channel-major (R block, G block, B block) per row. Transpose.
    if(fillCoefs > 0)
    {
      const float* sh_src
          = cloud.sh.data() + (std::size_t)i * shCoefs * 3;
      float* shR = row + CanonicalRow::kSHRest + 0  * restPad;
      float* shG = row + CanonicalRow::kSHRest + 1  * restPad;
      float* shB = row + CanonicalRow::kSHRest + 2  * restPad;
      for(uint32_t c = 0; c < fillCoefs; ++c)
      {
        shR[c] = sh_src[c * 3 + 0];
        shG[c] = sh_src[c * 3 + 1];
        shB[c] = sh_src[c * 3 + 2];
      }
      // Remaining coefs (fillCoefs..restPad) stay zero.
    }

    // Alpha — both formats store the pre-sigmoid value; pass through.
    row[CanonicalRow::kAlpha] = cloud.alphas[i];

    // Scale (log-space).
    row[CanonicalRow::kScale + 0] = cloud.scales[i * 3 + 0];
    row[CanonicalRow::kScale + 1] = cloud.scales[i * 3 + 1];
    row[CanonicalRow::kScale + 2] = cloud.scales[i * 3 + 2];

    // Rotation. SPZ: (x,y,z,w). PLY canonical: (w,x,y,z).
    row[CanonicalRow::kRot + 0] = cloud.rotations[i * 4 + 3]; // w
    row[CanonicalRow::kRot + 1] = cloud.rotations[i * 4 + 0]; // x
    row[CanonicalRow::kRot + 2] = cloud.rotations[i * 4 + 1]; // y
    row[CanonicalRow::kRot + 3] = cloud.rotations[i * 4 + 2]; // z
  }

  auto br = std::make_shared<ossia::buffer_resource>();
  br->resource = ossia::buffer_data{
      .data = std::shared_ptr<const void>(storage, storage.get()),
      .byte_size = (int64_t)totalBytes,
      .usage_hint = ossia::buffer_data::usage::storage_buffer};
  br->content_hash = (uint64_t)(uintptr_t)storage.get();

  auto out = std::make_shared<ossia::primitive_cloud_component>();
  out->raw_data        = std::move(br);
  out->row_stride      = CanonicalRow::kBytes;
  out->primitive_count = N;
  out->topology        = ossia::primitive_topology::points;
  out->format_id        = "3dgs.classic";
  out->struct_type_name = "Splat3DGS";
  out->bounds           = bounds;
  out->stable_id        = ossia::mint_stable_id();
  return out;
}

} // namespace Threedim::PrimitiveCloud
