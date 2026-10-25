#include "MorphTargets.hpp"

#include <cmath>
#include <cstring>
#include <memory>
#include <vector>

namespace Threedim
{

namespace
{
// The float3 stream of `sem` in `prim`, when it is a tightly packed float3
// CPU buffer starting at offset 0 -- the layout GltfParser and FbxParser emit.
// Anything else is not morphed.
struct Stream
{
  int buffer_index{-1};
  const float* data{};
};

Stream findStream(const ossia::mesh_primitive& prim, ossia::attribute_semantic sem)
{
  for(const auto& a : prim.attributes)
  {
    if(a.semantic != sem)
      continue;
    if(a.format != ossia::vertex_format::float3 || a.byte_offset != 0
       || (a.byte_stride != 0 && a.byte_stride != 3 * sizeof(float))
       || a.buffer_index >= prim.vertex_buffers.size())
      return {};
    const auto& buf = prim.vertex_buffers[a.buffer_index];
    if(!buf)
      return {};
    const auto* bd = ossia::get_if<ossia::buffer_data>(&buf->resource);
    if(!bd || !bd->data
       || bd->byte_size < int64_t(prim.vertex_count) * 3 * int64_t(sizeof(float)))
      return {};
    return {int(a.buffer_index), static_cast<const float*>(bd->data.get())};
  }
  return {};
}

const float* deltaData(const ossia::buffer_resource_ptr& buf, std::size_t floats_needed)
{
  if(!buf)
    return nullptr;
  const auto* bd = ossia::get_if<ossia::buffer_data>(&buf->resource);
  if(!bd || !bd->data || bd->byte_size < int64_t(floats_needed * sizeof(float)))
    return nullptr;
  return static_cast<const float*>(bd->data.get());
}

// The renderer uploads a primitive's vertices once per mesh_primitive::stable_id
// (ScenePreprocessor's mesh slabs), so a re-blended primitive needs an id of
// its own. Deriving it from the source id and the weights gives the same blend
// the same id back, and so the slab already uploaded for it.
uint64_t blendedId(uint64_t source, std::span<const float> weights)
{
  uint64_t h = source ^ 0x9E3779B97F4A7C15ull;
  for(float w : weights)
  {
    uint32_t bits;
    std::memcpy(&bits, &w, sizeof(bits));
    h = (h ^ bits) * 0x100000001B3ull;
  }
  return h != 0 ? h : 1;
}

ossia::buffer_resource_ptr makeBuffer(std::shared_ptr<std::vector<float>> floats)
{
  auto br = std::make_shared<ossia::buffer_resource>();
  ossia::buffer_data bd;
  bd.data = std::shared_ptr<const void>(floats, floats->data());
  bd.byte_size = int64_t(floats->size() * sizeof(float));
  bd.usage_hint = ossia::buffer_data::usage::vertex_buffer;
  br->resource = std::move(bd);
  br->dirty_index = 1;
  return br;
}
}

std::shared_ptr<ossia::mesh_component>
blendMorphTargets(const ossia::mesh_component& mesh, std::span<const float> weights)
{
  auto out = std::make_shared<ossia::mesh_component>(mesh);
  const std::size_t targets = mesh.morph_targets.size();

  // Per target, how far to move from the current blend.
  std::vector<float> delta_w(targets, 0.f);
  bool any = false;
  for(std::size_t t = 0; t < targets; ++t)
  {
    const float from = t < mesh.morph_weights.size() ? mesh.morph_weights[t] : 0.f;
    const float to = t < weights.size() ? weights[t] : 0.f;
    delta_w[t] = to - from;
    any = any || delta_w[t] != 0.f;
  }

  out->morph_weights.assign(weights.begin(), weights.begin() + std::min(weights.size(), targets));
  out->morph_weights.resize(targets, 0.f);
  out->dirty_index = mesh.dirty_index + 1;
  if(!any)
    return out;

  ossia::aabb mesh_bounds{};
  bool mesh_bounds_set = false;
  std::size_t vertex_base = 0;
  for(auto& prim : out->primitives)
  {
    const std::size_t n = prim.vertex_count;
    bool blended_any = false;
    for(auto sem : {ossia::attribute_semantic::position, ossia::attribute_semantic::normal})
    {
      const bool is_pos = sem == ossia::attribute_semantic::position;
      const auto stream = findStream(prim, sem);
      if(!stream.data)
        continue;

      auto blended = std::make_shared<std::vector<float>>(stream.data, stream.data + n * 3);
      bool touched = false;
      for(std::size_t t = 0; t < targets; ++t)
      {
        if(delta_w[t] == 0.f)
          continue;
        const auto& target = mesh.morph_targets[t];
        const float* d = deltaData(
            is_pos ? target.position_deltas : target.normal_deltas, (vertex_base + n) * 3);
        if(!d)
          continue;
        d += vertex_base * 3;
        const float w = delta_w[t];
        for(std::size_t i = 0; i < n * 3; ++i)
          (*blended)[i] += w * d[i];
        touched = true;
      }
      if(!touched)
        continue;

      if(!is_pos)
      {
        for(std::size_t i = 0; i < n; ++i)
        {
          float* v = blended->data() + i * 3;
          const float len = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
          if(len > 1e-12f)
            v[0] /= len, v[1] /= len, v[2] /= len;
        }
      }
      else
      {
        prim.bounds = ossia::compute_aabb_from_positions(blended->data(), n);
      }
      prim.vertex_buffers[stream.buffer_index] = makeBuffer(std::move(blended));
      blended_any = true;
    }
    if(blended_any)
      prim.stable_id = blendedId(prim.stable_id, out->morph_weights);

    if(!prim.bounds.empty())
    {
      if(!mesh_bounds_set)
        mesh_bounds = prim.bounds, mesh_bounds_set = true;
      else
      {
        mesh_bounds.expand(prim.bounds.min[0], prim.bounds.min[1], prim.bounds.min[2]);
        mesh_bounds.expand(prim.bounds.max[0], prim.bounds.max[1], prim.bounds.max[2]);
      }
    }
    vertex_base += n;
  }
  if(mesh_bounds_set)
    out->bounds = mesh_bounds;
  return out;
}

}
