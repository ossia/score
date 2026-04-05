#pragma once
#include <Threedim/TinyObj.hpp>

namespace Threedim
{
// Palette buffer layout (std430-compatible):
// 256 entries × 8 floats = 2048 floats = 8192 bytes
// Each entry: [r, g, b, a, metallic, roughness, emissive, ior]
static constexpr int vox_palette_entries = 256;
static constexpr int vox_palette_floats_per_entry = 8;
static constexpr int vox_palette_total_floats
    = vox_palette_entries * vox_palette_floats_per_entry;
static constexpr int64_t vox_palette_byte_size
    = vox_palette_total_floats * sizeof(float);

// Loads a MagicaVoxel .vox file as a point cloud.
// Per-voxel output: position (float3) + material_id (float1).
// Palette output: 256 × {rgba, metallic, roughness, emissive, ior}.
std::vector<mesh> VoxPointCloudFromFile(std::string_view filename, float_vec& data, float_vec& palette);

// Loads a MagicaVoxel .vox file as a pre-built mesh with neighbor-culled faces.
// Uses ogt_voxel_meshify for face culling.
// Per-vertex output: position (float3) + normal (float3) + material_id (float1).
// mode: 0 = simple (per-voxel quads), 1 = greedy (merged same-color regions)
std::vector<mesh> VoxMeshFromFile(std::string_view filename, float_vec& data, float_vec& palette, int mode);
}
