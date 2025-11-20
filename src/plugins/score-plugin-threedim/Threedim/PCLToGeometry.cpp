#include "PCLToGeometry.hpp"

#include <Threedim/MeshHelpers.hpp>
#include <Threedim/TinyObj.hpp>
#include <rnd/random.hpp>

#include <QDebug>
#include <QString>

namespace Threedim
{
PCLToMesh::PCLToMesh()
{
  rebuild_transform(inputs, outputs);
  outputs.geometry.dirty_mesh = true;
}

void PCLToMesh::operator()()
{
  auto& tex = this->inputs.in.buffer;
  if (!tex.changed)
    return;

  float* data = reinterpret_cast<float*>(tex.bytes);
  create_mesh(std::span<float>(data, tex.bytesize / sizeof(float)));
}

void PCLToMesh::create_mesh(std::span<float> v)
{
  {
    // std::size_t vertices = v.size() / 3;

    // this->complete.clear();
    // this->complete.resize(std::ceil((v.size() / 3.) * (3 + 3 + 2)));
    // std::copy_n(v.begin(), v.size(), complete.begin());

    // auto& pch = rnd::fast_random_device();
    //    this->complete.resize(6 * 25000);
    //    for (float& v : this->complete)
    //      v = std::uniform_real_distribution<>{0.f, 1.f}(pch);

    auto prev_size = outputs.geometry.mesh.buffers.main_buffer.size;
    const bool changed = v.size() != prev_size;
    //complete.assign(v.begin(), v.end());

    outputs.geometry.mesh.buffers.main_buffer.data = (float*)this->inputs.in.buffer.bytes;//complete.data();
    outputs.geometry.mesh.buffers.main_buffer.size = this->inputs.in.buffer.bytesize / sizeof(float);//complete.size();
    outputs.geometry.mesh.buffers.main_buffer.dirty = true;

    outputs.geometry.mesh.input.input0.offset = 0;
    outputs.geometry.mesh.vertices = v.size() / 6;
    outputs.geometry.dirty_mesh = true; // FIXME
  }
}


PCLToMesh2::PCLToMesh2()
{
  rebuild_transform(inputs, outputs);
  outputs.geometry.dirty_mesh = true;
}

void PCLToMesh2::operator()()
{
  auto& tex = this->inputs.in.buffer;
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
  buf.size = this->inputs.in.buffer.bytesize;
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
           .location = halp::attribute_location::position,
           .format = halp::attribute_format::float3,
           .offset = 0});
      break;
    case XYZ_RGB:
      vertice_stride = 6;
      attributes.push_back(
          {.binding = 0,
           .location = halp::attribute_location::position,
           .format = halp::attribute_format::float3,
           .offset = 0});
      attributes.push_back(
          {.binding = 0,
           .location = halp::attribute_location::color,
           .format = halp::attribute_format::float3,
           .offset = 3 * sizeof(float)});
      break;
    case XYZW:
      vertice_stride = 4;
      attributes.push_back(
          {.binding = 0,
           .location = halp::attribute_location::position,
           .format = halp::attribute_format::float4,
           .offset = 0});
      break;
    case XYZW_RGBA:
      vertice_stride = 8;
      attributes.push_back(
          {.binding = 0,
           .location = halp::attribute_location::position,
           .format = halp::attribute_format::float4,
           .offset = 0});
      attributes.push_back(
          {.binding = 0,
           .location = halp::attribute_location::color,
           .format = halp::attribute_format::float3,
           .offset = 3 * sizeof(float)});
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
  struct halp::geometry_input xyz_input;
  xyz_input.buffer = 0;
  xyz_input.offset = 0;
  inputs.push_back(xyz_input);

  // Vertices count.
  outputs.geometry.mesh.vertices = (tex.bytesize / (sizeof(float) * vertice_stride));

  outputs.geometry.dirty_mesh = true;
}

}
