#include "PCLToGeometry.hpp"

#include <Threedim/TinyObj.hpp>

#include <QDebug>

namespace Threedim
{

PCLToMesh2::PCLToMesh2()
{
  rebuild_transform(inputs, outputs);
  outputs.geometry.dirty_mesh = true;
}

void PCLToMesh2::operator()()
{
  auto& tex = this->inputs.in.buffer;

  // FIXME optimize
  // if (!tex.changed)
  //   return;

  // float* data = reinterpret_cast<float*>(tex.bytes);
  // create_mesh(std::span<float>(data, tex.bytesize / sizeof(float)));

  auto& mesh = outputs.geometry.mesh;
  auto& buffers = mesh.buffers;
  auto& bindings = mesh.bindings;
  auto& attributes = mesh.attributes;
  auto& inputs = mesh.input;

  mesh.topology = halp::primitive_topology::points;
  mesh.cull_mode = halp::cull_mode::none;
  mesh.front_face = halp::front_face::counter_clockwise;

  buffers.clear();
  bindings.clear();
  attributes.clear();
  inputs.clear();

  // Buffers
  halp::geometry_gpu_buffer buf{};
  buf.handle = this->inputs.in.buffer.handle;
  buf.byte_size = this->inputs.in.buffer.byte_size;
  buf.dirty = true;
  buffers.push_back(buf);

  // Bindings
  int vertice_stride = 0;
  switch(this->inputs.type)
  {
    case XYZ:
      vertice_stride = 3;
      attributes.push_back(
          {.binding = 0,
           .semantic = halp::attribute_semantic::position,
           .format = halp::attribute_format::float3,
           .byte_offset = 0});
      break;
    case XYZ_RGB:
      vertice_stride = 6;
      attributes.push_back({
          .binding = 0,
          .semantic = halp::attribute_semantic::position,
          .format = halp::attribute_format::float3,
          .byte_offset = 0,
      });
      attributes.push_back({
          .binding = 0,
          .semantic = halp::attribute_semantic::color0,
          .format = halp::attribute_format::float3,
          .byte_offset = 3 * sizeof(float),
      });
      break;
    case XYZW:
      vertice_stride = 4;
      attributes.push_back({
          .binding = 0,
          .semantic = halp::attribute_semantic::position,
          .format = halp::attribute_format::float4,
          .byte_offset = 0,
      });
      break;
    case XYZW_RGBA:
      vertice_stride = 8;
      attributes.push_back({
          .binding = 0,
          .semantic = halp::attribute_semantic::position,
          .format = halp::attribute_format::float4,
          .byte_offset = 0,
      });
      attributes.push_back({
          .binding = 0,
          .semantic = halp::attribute_semantic::color0,
          .format = halp::attribute_format::float4,
          .byte_offset = 4 * sizeof(float),
      });
      break;
    default:
      return;
  }

  bindings.push_back({
     .stride = vertice_stride * int(sizeof(float)),
     .step_rate = 1,
     .classification = halp::binding_classification::per_vertex
  });

  // Input. We have only one buffer so one input.
  // FIXME what when more than one buffer.
  struct halp::geometry_input xyz_input;
  xyz_input.buffer = 0;
  xyz_input.byte_offset = this->inputs.in.buffer.byte_offset;
  inputs.push_back(xyz_input);

  // Vertices count.
  outputs.geometry.mesh.vertices = (tex.byte_size / (sizeof(float) * vertice_stride));

  outputs.geometry.dirty_mesh = true;
}

}
