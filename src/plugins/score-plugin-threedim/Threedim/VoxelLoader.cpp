#include "VoxelLoader.hpp"

#include <QMatrix4x4>

namespace Threedim
{

void VoxelLoader::reload()
{
  if(cached_filename.empty())
    return;

  float_vec buf;
  float_vec pal;
  int mode = inputs.mode.value;

  std::vector<mesh> result;
  if(mode == 0)
  {
    result = VoxPointCloudFromFile(cached_filename, buf, pal);
  }
  else
  {
    // mode 1 = simple, mode 2 = greedy → pass (mode - 1) to VoxMeshFromFile
    result = VoxMeshFromFile(cached_filename, buf, pal, mode - 1);
  }

  if(!result.empty())
  {
    std::swap(meshinfo, result);
    std::swap(complete, buf);
    std::swap(palette, pal);
    rebuild_geometry();
  }
}

void VoxelLoader::rebuild_geometry()
{
  if(!outputs.geometry.mesh.empty())
    outputs.geometry.mesh.clear();

  for(auto& m : meshinfo)
  {
    if(m.vertices <= 0)
      continue;

    halp::dynamic_geometry geom;

    geom.buffers.clear();
    geom.bindings.clear();
    geom.attributes.clear();
    geom.input.clear();

    if(m.points)
    {
      geom.topology = halp::primitive_topology::points;
      geom.cull_mode = halp::cull_mode::none;
      geom.front_face = halp::front_face::counter_clockwise;
    }
    else
    {
      geom.topology = halp::primitive_topology::triangles;
      geom.cull_mode = halp::cull_mode::back;
      geom.front_face = halp::front_face::clockwise;
    }
    geom.index = {};
    geom.vertices = m.vertices;

    // Buffer 0: vertex data
    geom.buffers.push_back(
        halp::geometry_cpu_buffer{
            .raw_data = this->complete.data(),
            .byte_size = int64_t(this->complete.size() * sizeof(float)),
            .dirty = true});

    // Position attribute
    geom.bindings.push_back(
        halp::geometry_binding{
            .stride = 3 * (int)sizeof(float),
            .step_rate = 1,
            .classification = halp::binding_classification::per_vertex});
    geom.attributes.push_back(
        halp::geometry_attribute{
            .binding = 0,
            .semantic = halp::attribute_semantic::position,
            .format = halp::attribute_format::float3,
            .byte_offset = 0});
    geom.input.push_back(
        halp::geometry_input{
            .buffer = 0, .byte_offset = m.pos_offset * (int)sizeof(float)});

    // Normal attribute (mesh mode only)
    if(m.normals)
    {
      geom.bindings.push_back(
          halp::geometry_binding{
              .stride = 3 * (int)sizeof(float),
              .step_rate = 1,
              .classification = halp::binding_classification::per_vertex});
      geom.attributes.push_back(
          halp::geometry_attribute{
              .binding = geom.attributes.back().binding + 1,
              .semantic = halp::attribute_semantic::normal,
              .format = halp::attribute_format::float3,
              .byte_offset = 0});
      geom.input.push_back(
          halp::geometry_input{
              .buffer = 0, .byte_offset = m.normal_offset * (int)sizeof(float)});
    }

    // Extra attributes (material_id)
    for(auto& extra : m.extras)
    {
      geom.bindings.push_back(
          halp::geometry_binding{
              .stride = extra.components * (int)sizeof(float),
              .step_rate = 1,
              .classification = halp::binding_classification::per_vertex});
      geom.attributes.push_back(
          halp::geometry_attribute{
              .binding = geom.attributes.back().binding + 1,
              .semantic = extra.semantic,
              .format = extra.format,
              .byte_offset = 0,
              .name = extra.name});
      geom.input.push_back(
          halp::geometry_input{
              .buffer = 0, .byte_offset = extra.offset * (int)sizeof(float)});
    }

    // Palette auxiliary buffer
    if(!this->palette.empty())
    {
      const int paletteBufIndex = geom.buffers.size();
      geom.buffers.push_back(
          halp::geometry_cpu_buffer{
              .raw_data = this->palette.data(),
              .byte_size = int64_t(this->palette.size() * sizeof(float)),
              .dirty = true});

      geom.auxiliary.push_back(
          halp::geometry_auxiliary_buffer{
              .name = "vox_palette",
              .buffer = paletteBufIndex,
              .byte_offset = 0,
              .byte_size = int64_t(this->palette.size() * sizeof(float))});
    }

    outputs.geometry.mesh.push_back(std::move(geom));
    outputs.geometry.dirty_mesh = true;
  }
}

std::function<void(VoxelLoader&)> VoxelLoader::ins::vox_t::process(file_type tv)
{
  if(tv.filename.empty())
    return {};

  return [filename = std::string(tv.filename)](VoxelLoader& o) mutable {
    o.cached_filename = std::move(filename);
    o.reload();
  };
}

}
