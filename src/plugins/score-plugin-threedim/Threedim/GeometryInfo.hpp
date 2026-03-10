#pragma once
#include <ossia/dataflow/geometry_port.hpp>
#include <ossia/detail/pod_vector.hpp>

#include <fmt/format.h>
#include <halp/controls.hpp>
#include <halp/geometry.hpp>
#include <halp/meta.hpp>

namespace Threedim
{

class GeometryInfo
{
public:
  halp_meta(name, "Geometry Info")
  halp_meta(category, "Visuals/Utilities")
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
#if 1
    halp::val_port<"Readable", std::string> readable;
#endif
  } outputs;

  static std::string_view semantic_name(ossia::attribute_semantic sem)
  {
    auto s = static_cast<ossia::attribute_semantic>(sem);
    if(s != ossia::attribute_semantic::custom)
      return ossia::semantic_to_name(s);
    return "custom";
  }

  static std::string_view format_name(halp::attribute_format v)
  {
    switch(v)
    {
      case halp::attribute_format::float4:
        return "float4";
      case halp::attribute_format::float3:
        return "float3";
      case halp::attribute_format::float2:
        return "float2";
      case halp::attribute_format::float1:
        return "float1";
      case halp::attribute_format::unormbyte4:
        return "unormbyte4";
      case halp::attribute_format::unormbyte2:
        return "unormbyte2";
      case halp::attribute_format::unormbyte1:
        return "unormbyte1";
      case halp::attribute_format::uint4:
        return "uint4";
      case halp::attribute_format::uint3:
        return "uint3";
      case halp::attribute_format::uint2:
        return "uint2";
      case halp::attribute_format::uint1:
        return "uint1";
      case halp::attribute_format::sint4:
        return "sint4";
      case halp::attribute_format::sint3:
        return "sint3";
      case halp::attribute_format::sint2:
        return "sint2";
      case halp::attribute_format::sint1:
        return "sint1";
      case halp::attribute_format::half4:
        return "half4";
      case halp::attribute_format::half3:
        return "half3";
      case halp::attribute_format::half2:
        return "half2";
      case halp::attribute_format::half1:
        return "half1";
      case halp::attribute_format::ushort4:
        return "ushort4";
      case halp::attribute_format::ushort3:
        return "ushort3";
      case halp::attribute_format::ushort2:
        return "ushort2";
      case halp::attribute_format::ushort1:
        return "ushort1";
      case halp::attribute_format::sshort4:
        return "sshort4";
      case halp::attribute_format::sshort3:
        return "sshort3";
      case halp::attribute_format::sshort2:
        return "sshort2";
      case halp::attribute_format::sshort1:
        return "sshort1";
      default:
        return "unknown";
    }
  }

  static std::string_view classification_name(halp::binding_classification v)
  {
    switch(v)
    {
      case halp::binding_classification::per_vertex:
        return "per_vertex";
      case halp::binding_classification::per_instance:
        return "per_instance";
      default:
        return "unknown";
    }
  }

  void operator()()
  {
    outputs.vertices.value = inputs.geometry.mesh.vertices;
    outputs.indices.value = inputs.geometry.mesh.indices;
    outputs.instances.value = inputs.geometry.mesh.instances;
    // outputs.buffers.value = inputs.geometry.mesh.buffers;
    outputs.attributes.value = inputs.geometry.mesh.attributes;
    outputs.bindings.value = inputs.geometry.mesh.bindings;
    outputs.inputs.value = inputs.geometry.mesh.input;

#if 1
    {
      std::string& ret = outputs.readable.value;
      ret.clear();
      fmt::format_to(
          std::back_inserter(ret), "vertices: {}, indices: {}, instances: {}\n",
          inputs.geometry.mesh.vertices, inputs.geometry.mesh.indices,
          inputs.geometry.mesh.instances);

      int i = 0;
      for(auto& v : inputs.geometry.mesh.attributes)
      {
        fmt::format_to(
            std::back_inserter(ret),
            "Attribute {}: semantic={}, binding={},  fmt={}, offset={}\n", i++,
            semantic_name(static_cast<ossia::attribute_semantic>(v.semantic)), v.binding,
            format_name(v.format), v.byte_offset);
      }

      i = 0;
      for(auto& v : inputs.geometry.mesh.bindings)
      {
        fmt::format_to(
            std::back_inserter(ret), "Binding {}: stride={}, step={}, class={}\n", i++,
            v.stride, v.step_rate, classification_name(v.classification));
      }

      i = 0;
      for(auto& v : inputs.geometry.mesh.input)
      {
        fmt::format_to(
            std::back_inserter(ret), "Input {}: buffer_idx={}, offset={}\n", i++,
            v.buffer, v.byte_offset);
      }
    }
#endif
  }
};

}
