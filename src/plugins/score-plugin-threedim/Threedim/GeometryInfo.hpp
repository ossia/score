#pragma once
#include <ossia/detail/pod_vector.hpp>

#include <halp/controls.hpp>
#include <halp/geometry.hpp>
#include <halp/meta.hpp>

namespace Threedim
{

class GeometryInfo
{
public:
  halp_meta(name, "Geometry Info")
  halp_meta(category, "Visuals/3D")
  halp_meta(c_name, "geometry_info")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/geometry-info.html")
  halp_meta(uuid, "c4deb797-8d5f-4ffb-b25b-b541f5c54099")

  struct
  {
    struct
    {
      halp_meta(name, "Geometry");
      halp::dynamic_gpu_geometry mesh;
      float transform[16]{};
      bool dirty_mesh = false;
      bool dirty_transform = false;
    } geometry;
  } inputs;

  struct
  {
    halp::val_port<"Vertices", int> vertices;
    halp::val_port<"Indices", int> indices;
    halp::val_port<"Instances", int> instances;

    // halp::val_port<"Buffers", std::vector<halp::geometry_gpu_buffer>> buffers;
    halp::val_port<"Attributes", std::vector<halp::geometry_attribute>> attributes;
    halp::val_port<"Bindings", std::vector<halp::geometry_binding>> bindings;
    halp::val_port<"Inputs", std::vector<halp::geometry_input>> inputs;
  } outputs;

  void operator()()
  {
    outputs.vertices.value = inputs.geometry.mesh.vertices;
    outputs.indices.value = inputs.geometry.mesh.indices;
    outputs.instances.value = inputs.geometry.mesh.instances;
    // outputs.buffers.value = inputs.geometry.mesh.buffers;
    outputs.attributes.value = inputs.geometry.mesh.attributes;
    outputs.bindings.value = inputs.geometry.mesh.bindings;
    outputs.inputs.value = inputs.geometry.mesh.input;
  }
};

}
